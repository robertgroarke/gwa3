"""Executor role for the two-model bridge architecture."""

from __future__ import annotations

import json
import time

import httpx

from .action_dispatcher import ActionDispatcher, ToolExecutionSummary
from .agent_history_events import build_assistant_response_message
from .event_bus import BridgeEventBus
from .history_manager import HistoryManager
from .llm_client import LLMClient
from .observation import build_executor_view
from .plan import Plan
from .prompt_assets import EXECUTOR_SYSTEM_PROMPT
from .telemetry import BridgeTelemetry
from .tool_schema import EXECUTOR_TOOL_NAMES, EXECUTOR_TOOLS


EXECUTOR_OWNED_PHASE_KINDS = {"combat", "boss", "long_walk"}


class ExecutorLoop:
    def __init__(
        self,
        executor_llm: LLMClient,
        dispatcher: ActionDispatcher,
        event_bus: BridgeEventBus | None = None,
        telemetry: BridgeTelemetry | None = None,
        fallback_llm: LLMClient | None = None,
        executor_mode: str = "llm",
    ):
        self.executor_llm = executor_llm
        self.active_llm = executor_llm
        self.fallback_llm = fallback_llm
        self.dispatcher = dispatcher
        self.event_bus = event_bus
        self.telemetry = telemetry
        self.executor_mode = executor_mode
        self.history_manager = HistoryManager(max_messages=24)
        self.consecutive_timeouts = 0
        self.using_single_model_fallback = False
        self.unhealthy = False

    @property
    def history(self) -> list[dict]:
        return self.history_manager.messages

    async def emit(self, event: str, data: dict | None = None) -> None:
        if self.event_bus is not None:
            await self.event_bus.emit(event, data or {})

    async def tick(self, plan: Plan, snapshot: dict | None) -> ToolExecutionSummary:
        if self.executor_mode == "disabled":
            return self._idle("executor_disabled")
        if self.executor_mode == "health-check":
            return self._health_check(plan, snapshot)
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
                self.telemetry.record_executor_tick(self.executor_mode, "llm_response", llm_call=True)
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
        assistant_message = build_assistant_response_message(
            content=response.content,
            tool_calls=response.tool_calls,
        )
        if assistant_message is not None:
            self.history.append(assistant_message)

        summary = await self.dispatcher.execute_response(
            response,
            role="executor",
            allowed_tool_names=EXECUTOR_TOOL_NAMES,
            history=self.history,
        )
        if self.telemetry is not None:
            for result in summary.results:
                self.telemetry.record_tool_result("executor", bool(result.get("success")))
            self.telemetry.record_executor_tick(
                self.executor_mode,
                "tool_summary",
                useful=summary.had_game_action,
            )
        if not summary.had_game_action:
            summary.counts_as_stall = True
        self._trim_history()
        return summary

    def _idle(self, reason: str, *, counts_as_stall: bool = False) -> ToolExecutionSummary:
        if self.telemetry is not None:
            self.telemetry.record_idle_tick("executor", reason, counts_as_stall=counts_as_stall)
            self.telemetry.record_executor_tick(self.executor_mode, reason)
        return ToolExecutionSummary(
            had_tool_call=False,
            had_game_action=False,
            idle_reason=reason,
            counts_as_stall=counts_as_stall,
            results=[{"success": True, "action": "idle", "reason": reason}],
        )

    def _health_check(self, plan: Plan, snapshot: dict | None) -> ToolExecutionSummary:
        if snapshot is None:
            return self._idle("health_check_missing_snapshot", counts_as_stall=True)
        party = snapshot.get("party") or {}
        bot = snapshot.get("bot") or {}
        map_state = snapshot.get("map") or {}
        reason = "healthy"
        counts_as_stall = False
        if party.get("is_defeated"):
            reason = "party_defeated"
            counts_as_stall = True
        elif map_state.get("loading_state") not in (None, 1):
            reason = "map_loading"
        elif bot.get("state") in {"stalled", "error"}:
            reason = f"bot_{bot.get('state')}"
            counts_as_stall = True
        if self.telemetry is not None:
            self.telemetry.record_executor_tick("health-check", reason, useful=not counts_as_stall)
        return ToolExecutionSummary(
            had_tool_call=False,
            had_game_action=False,
            idle_reason=f"health_check:{reason}",
            counts_as_stall=counts_as_stall,
            results=[{"success": True, "action": "health_check", "reason": reason}],
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
            *self.history_manager.recent_for_prompt(8),
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
        if str(plan.phase_kind) in EXECUTOR_OWNED_PHASE_KINDS:
            return bool(next_step) and next_step != "wait"
        return next_step.startswith("executor:")

    def _trim_history(self) -> None:
        self.history_manager.trim_preserving_user_messages()
