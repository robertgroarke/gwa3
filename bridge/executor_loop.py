"""Executor role for the two-model bridge architecture."""

from __future__ import annotations

import json
import time

import httpx

from .action_dispatcher import ActionDispatcher, ToolExecutionSummary
from .event_bus import BridgeEventBus
from .llm_client import LLMClient
from .observation import build_executor_view
from .plan import Plan
from .telemetry import BridgeTelemetry
from .tool_schema import EXECUTOR_TOOL_NAMES, EXECUTOR_TOOLS


EXECUTOR_SYSTEM_PROMPT = """\
You are the tactical executor for GWA3 Froggy HM advisory mode.

You receive a Plan plus a trimmed game snapshot. Emit only tool calls that directly
advance the current Plan. Never invent strategy, never change maps or party setup,
and never use planner-only tools. Only call tactical tools when the Plan next_step
explicitly starts with "executor:". If an abort condition fires, emit wait only and
let the planner re-plan. If the Plan is completed or unclear, emit wait.
"""


class ExecutorLoop:
    def __init__(
        self,
        executor_llm: LLMClient,
        dispatcher: ActionDispatcher,
        event_bus: BridgeEventBus | None = None,
        telemetry: BridgeTelemetry | None = None,
        fallback_llm: LLMClient | None = None,
    ):
        self.executor_llm = executor_llm
        self.active_llm = executor_llm
        self.fallback_llm = fallback_llm
        self.dispatcher = dispatcher
        self.event_bus = event_bus
        self.telemetry = telemetry
        self.history: list[dict] = []
        self.consecutive_timeouts = 0
        self.using_single_model_fallback = False
        self.unhealthy = False

    async def emit(self, event: str, data: dict | None = None) -> None:
        if self.event_bus is not None:
            await self.event_bus.emit(event, data or {})

    async def tick(self, plan: Plan, snapshot: dict | None) -> ToolExecutionSummary:
        if snapshot is None:
            return self._idle("missing_snapshot")
        bot = snapshot.get("bot", {}) or {}
        if bot.get("state") != "llm_controlled":
            self._append_idle_history("froggy native control active; executor idle")
            return self._idle("froggy_native_control")
        if self._abort_fired(plan, snapshot):
            self._append_idle_history("abort condition fired; executor waiting for planner")
            return self._idle("abort_condition", counts_as_stall=True)
        if not self._plan_allows_executor_action(plan):
            self._append_idle_history("planner-owned plan; executor idle")
            return self._idle("planner_owned_plan")

        started = time.perf_counter()
        try:
            messages = self._build_messages(plan, snapshot)
            if self.telemetry is not None:
                self.telemetry.record_prompt("executor", messages, self._prompt_sections(messages))
            response = await self.active_llm.chat_completion(
                messages=messages,
                tools=EXECUTOR_TOOLS,
                tool_choice="auto",
                temperature=0.1,
                max_tokens=700,
            )
            self.consecutive_timeouts = 0
            self.unhealthy = False
            if self.telemetry is not None:
                self.telemetry.record_llm_call("executor", time.perf_counter() - started, response.usage)
        except (httpx.TimeoutException, TimeoutError) as exc:
            self.consecutive_timeouts += 1
            await self.emit("degradation", {
                "reason": "executor_timeout",
                "count": self.consecutive_timeouts,
                "error": str(exc),
            })
            if self.telemetry is not None:
                self.telemetry.record_degradation("executor_timeout")
            if self.consecutive_timeouts >= 3:
                await self.enable_single_model_fallback("three_executor_timeouts")
            return self._idle("executor_timeout", counts_as_stall=True)
        except httpx.HTTPError as exc:
            self.consecutive_timeouts += 1
            await self.emit("degradation", {
                "reason": "executor_http_error",
                "count": self.consecutive_timeouts,
                "error": str(exc),
            })
            if self.consecutive_timeouts >= 3:
                await self.enable_single_model_fallback("executor_http_errors")
            return self._idle("executor_http_error", counts_as_stall=True)

        if response.content:
            await self.emit("chat.assistant", {"message": response.content})
            self.history.append({"role": "assistant", "content": response.content})

        summary = await self.dispatcher.execute_response(
            response,
            role="executor",
            allowed_tool_names=EXECUTOR_TOOL_NAMES,
            history=self.history,
        )
        if self.telemetry is not None:
            for result in summary.results:
                self.telemetry.record_tool_result("executor", bool(result.get("success")))
        if not summary.had_game_action:
            summary.counts_as_stall = True
        if response.tool_calls:
            self.history.append({
                "role": "assistant",
                "content": response.content,
                "tool_calls": [
                    {
                        "id": tc.id,
                        "type": "function",
                        "function": {"name": tc.name, "arguments": tc.arguments},
                    }
                    for tc in response.tool_calls
                ],
            })
        self._trim_history()
        return summary

    def _idle(self, reason: str, *, counts_as_stall: bool = False) -> ToolExecutionSummary:
        if self.telemetry is not None:
            self.telemetry.record_idle_tick("executor", reason, counts_as_stall=counts_as_stall)
        return ToolExecutionSummary(
            had_tool_call=False,
            had_game_action=False,
            idle_reason=reason,
            counts_as_stall=counts_as_stall,
            results=[{"success": True, "action": "idle", "reason": reason}],
        )

    def _append_idle_history(self, content: str) -> None:
        if self.history and self.history[-1].get("content") == content:
            return
        self.history.append({"role": "assistant", "content": content})
        self._trim_history()

    async def enable_single_model_fallback(self, reason: str) -> None:
        if self.using_single_model_fallback:
            self.unhealthy = True
            await self.emit("degradation", {
                "reason": f"both_llms_unhealthy:{reason}",
            })
            if self.telemetry is not None:
                self.telemetry.record_degradation(f"both_llms_unhealthy:{reason}")
            return
        if self.fallback_llm is None:
            self.unhealthy = True
            return
        self.active_llm = self.fallback_llm
        self.using_single_model_fallback = True
        self.consecutive_timeouts = 0
        await self.emit("degradation", {
            "reason": f"single_model_fallback:{reason}",
        })
        if self.telemetry is not None:
            self.telemetry.record_degradation(f"single_model_fallback:{reason}")

    def _build_messages(self, plan: Plan, snapshot: dict) -> list[dict]:
        payload = build_executor_view(snapshot, plan.to_dict())
        return [
            {"role": "system", "content": EXECUTOR_SYSTEM_PROMPT},
            *self.history[-8:],
            {"role": "user", "content": json.dumps(payload, separators=(",", ":"))},
        ]

    @staticmethod
    def _prompt_sections(messages: list[dict]) -> dict[str, int]:
        return {
            "system": len((messages[0].get("content") or "").encode("utf-8")) if messages else 0,
            "history": sum(len(json.dumps(message, separators=(",", ":")).encode("utf-8")) for message in messages[1:-1]),
            "payload": len((messages[-1].get("content") or "").encode("utf-8")) if messages else 0,
        }

    @staticmethod
    def _abort_fired(plan: Plan, snapshot: dict) -> bool:
        for condition in plan.abort_conditions:
            if not isinstance(condition, dict):
                continue
            kind = condition.get("kind")
            if kind == "party_defeated" and (snapshot.get("party") or {}).get("is_defeated"):
                return True
            if kind == "inventory_full" and (snapshot.get("inventory") or {}).get("free_slots_total") == 0:
                return True
        return False

    @staticmethod
    def _plan_allows_executor_action(plan: Plan) -> bool:
        next_step = (plan.next_step or "").strip().lower()
        return next_step.startswith("executor:")

    def _trim_history(self) -> None:
        if len(self.history) > 24:
            self.history = self.history[-24:]
