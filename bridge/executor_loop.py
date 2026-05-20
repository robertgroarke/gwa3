"""Executor role for the two-model bridge architecture."""

from __future__ import annotations

import json
import time

import httpx

from .action_dispatcher import ActionDispatcher, ToolExecutionSummary
from .event_bus import BridgeEventBus
from .llm_client import LLMClient, LLMResponse, ToolCall
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
            return ToolExecutionSummary()
        bot = snapshot.get("bot", {}) or {}
        if bot.get("state") != "llm_controlled":
            response = LLMResponse(tool_calls=[
                ToolCall(id="froggy-control-wait", name="wait", arguments='{"milliseconds":500}')
            ])
            self.history.append({
                "role": "assistant",
                "content": "froggy native control active; waiting for planner takeover",
            })
            return await self.dispatcher.execute_response(response, "executor", EXECUTOR_TOOL_NAMES, self.history)
        if self._abort_fired(plan, snapshot):
            response = LLMResponse(tool_calls=[
                ToolCall(id="abort-wait", name="wait", arguments='{"milliseconds":500}')
            ])
            self.history.append({"role": "assistant", "content": "abort condition fired; waiting for planner"})
            return await self.dispatcher.execute_response(response, "executor", EXECUTOR_TOOL_NAMES, self.history)
        if not self._plan_allows_executor_action(plan):
            response = LLMResponse(tool_calls=[
                ToolCall(id="planner-helper-wait", name="wait", arguments='{"milliseconds":500}')
            ])
            self.history.append({
                "role": "assistant",
                "content": "plan does not explicitly authorize executor tools; waiting for planner",
            })
            return await self.dispatcher.execute_response(response, "executor", EXECUTOR_TOOL_NAMES, self.history)

        started = time.perf_counter()
        try:
            response = await self.active_llm.chat_completion(
                messages=self._build_messages(plan, snapshot),
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
            return ToolExecutionSummary()
        except httpx.HTTPError as exc:
            self.consecutive_timeouts += 1
            await self.emit("degradation", {
                "reason": "executor_http_error",
                "count": self.consecutive_timeouts,
                "error": str(exc),
            })
            if self.consecutive_timeouts >= 3:
                await self.enable_single_model_fallback("executor_http_errors")
            return ToolExecutionSummary()

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
