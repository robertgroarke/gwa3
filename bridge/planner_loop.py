"""Planner role for the two-model bridge architecture."""

from __future__ import annotations

import hashlib
import json
import time

import httpx

from .action_dispatcher import ActionDispatcher
from .event_bus import BridgeEventBus
from .history_manager import HistoryManager
from .knowledge_tools import KNOWLEDGE_TOOL_NAMES
from .llm_client import LLMClient, LLMResponse, ToolCall
from .observation import build_planner_view
from .plan import Plan, PlanState, PlanValidationError
from .prompt_assets import PLANNER_SYSTEM_PROMPT
from .route_spec import build_planner_route_guidance, validate_route_tool_choice
from .telemetry import BridgeTelemetry
from .tool_schema import (
    PLANNER_CONTROL_TOOL_NAMES,
    planner_tool_names_for_observation,
    planner_tools_for_observation,
)


PLANNER_LOCAL_OR_STATUS_TOOLS = {
    "wait",
    "query_state",
    "search_trade_prices",
    *KNOWLEDGE_TOOL_NAMES,
}


class PlannerLoop:
    def __init__(
        self,
        llm: LLMClient,
        plan_state: PlanState,
        dispatcher: ActionDispatcher,
        event_bus: BridgeEventBus | None = None,
        telemetry: BridgeTelemetry | None = None,
        objective: str | None = None,
        run_history: list[dict] | None = None,
        prompt_mode: str = "full",
        profile_name: str = "",
    ):
        self.llm = llm
        self.plan_state = plan_state
        self.dispatcher = dispatcher
        self.event_bus = event_bus
        self.telemetry = telemetry
        self.objective = objective or "Farm Bogroot Growths HM repeatedly with Froggy HM."
        self.run_history = run_history or []
        self.prompt_mode = prompt_mode
        self.profile_name = profile_name
        self._stable_context_hash: str | None = None
        self.history_manager = HistoryManager(max_messages=40)
        self.invalid_json_count = 0
        self.unhealthy = False

    @property
    def history(self) -> list[dict]:
        return self.history_manager.messages

    async def emit(self, event: str, data: dict | None = None) -> None:
        if self.event_bus is not None:
            await self.event_bus.emit(event, data or {})

    async def replan(
        self,
        reason: str,
        snapshot: dict | None,
        events: list[dict] | None = None,
        user_messages: list[str] | None = None,
        accept_plan=None,
    ) -> Plan | None:
        if self.telemetry is not None:
            self.telemetry.record_replan(reason)

        planner_error: str | None = None
        for attempt in range(2):
            started = time.perf_counter()
            try:
                response = await self.llm.chat_completion(
                    messages=self._build_messages(
                        reason,
                        snapshot,
                        events or [],
                        user_messages or [],
                        planner_error=planner_error,
                    ),
                    tools=planner_tools_for_observation(snapshot),
                    tool_choice="auto",
                    temperature=0.1,
                    max_tokens=1400,
                )
                if self.telemetry is not None:
                    self.telemetry.record_llm_call("planner", time.perf_counter() - started, response.usage)
                plan = self._parse_plan(response, snapshot=snapshot, reason=reason)
                if accept_plan is not None and not accept_plan(plan):
                    await self.emit("degradation", {"reason": "planner_output_discarded_stale"})
                    if self.telemetry is not None:
                        self.telemetry.record_degradation("planner_output_discarded_stale")
                    return None
                await self.plan_state.replace(plan)
                await self.emit("plan.updated", plan.to_dict())
                await self._execute_planner_tools(response, plan, snapshot)
                self.unhealthy = False
                self.invalid_json_count = 0
                return plan
            except (PlanValidationError, json.JSONDecodeError, TypeError, ValueError) as exc:
                self.invalid_json_count += 1
                await self.emit("degradation", {
                    "reason": f"planner_invalid_json:{type(exc).__name__}",
                    "attempt": attempt + 1,
                })
                if attempt == 0:
                    planner_error = str(exc)
                    continue
                self.unhealthy = True
                if self.telemetry is not None:
                    self.telemetry.record_degradation("planner_invalid_json")
                return None
            except (httpx.TimeoutException, TimeoutError) as exc:
                await self.emit("degradation", {
                    "reason": "planner_slow",
                    "error": str(exc),
                })
                if attempt == 0:
                    continue
                self.unhealthy = True
                return None
            except httpx.HTTPError as exc:
                await self.emit("degradation", {
                    "reason": "planner_http_error",
                    "error": str(exc),
                })
                self.unhealthy = True
                return None
        return None

    async def _execute_planner_tools(
        self,
        response: LLMResponse,
        plan: Plan | None = None,
        snapshot: dict | None = None,
    ) -> None:
        allowed = planner_tool_names_for_observation(snapshot) - PLANNER_CONTROL_TOOL_NAMES
        calls = self._action_tool_calls(response.tool_calls)
        missing_control_tools = self._game_tools_missing_control(calls, snapshot)
        if missing_control_tools:
            await self.emit("degradation", {
                "reason": "planner_game_tool_without_llm_control",
                "tools": missing_control_tools,
                "bot_state": self._snapshot_bot_state(snapshot),
            })
        route_mismatches = [
            mismatch
            for call in calls
            if (mismatch := validate_route_tool_choice(call.name, snapshot)) is not None
        ]
        if route_mismatches:
            await self.emit("degradation", {
                "reason": "planner_route_tool_mismatch",
                "mismatches": route_mismatches,
            })

        if plan is not None:
            self._append_plan_history(plan, calls)

        await self.dispatcher.execute_response(
            LLMResponse(content=response.content, tool_calls=calls, usage=response.usage),
            role="planner",
            allowed_tool_names=allowed,
            history=self.history,
        )

    def _append_plan_history(self, plan: Plan, calls: list[ToolCall]) -> None:
        message = {
            "role": "assistant",
            "content": json.dumps(plan.to_dict()),
        }
        if calls:
            message["tool_calls"] = [
                {
                    "id": call.id,
                    "type": "function",
                    "function": {"name": call.name, "arguments": call.arguments},
                }
                for call in calls
            ]
        self.history.append(message)

    @staticmethod
    def _action_tool_calls(calls: list[ToolCall]) -> list[ToolCall]:
        return [
            call
            for call in calls
            if call.name not in PLANNER_CONTROL_TOOL_NAMES
        ]

    @staticmethod
    def _has_control_takeover(calls: list[ToolCall]) -> bool:
        for call in calls:
            if call.name != "set_bot_state":
                continue
            try:
                args = call.parsed_arguments
            except (json.JSONDecodeError, TypeError, ValueError):
                continue
            if args.get("state") == "llm_controlled":
                return True
        return False

    @staticmethod
    def _snapshot_bot_state(snapshot: dict | None) -> str:
        return str(((snapshot or {}).get("bot") or {}).get("state") or "")

    @classmethod
    def _game_tools_missing_control(cls, calls: list[ToolCall], snapshot: dict | None) -> list[str]:
        if cls._snapshot_bot_state(snapshot) == "llm_controlled":
            return []
        if cls._has_control_takeover(calls):
            return []
        return [
            call.name
            for call in calls
            if call.name not in PLANNER_LOCAL_OR_STATUS_TOOLS
            and call.name != "set_bot_state"
        ]

    def _build_messages(
        self,
        reason: str,
        snapshot: dict | None,
        events: list[dict],
        user_messages: list[str],
        planner_error: str | None = None,
    ) -> list[dict]:
        view = build_planner_view(snapshot, self.run_history)
        history = self._prompt_history()
        route_guidance = build_planner_route_guidance()
        stable_context = {
            "profile": self.profile_name,
            "route_runbook": route_guidance,
        }
        context_hash = self._json_hash(stable_context)
        send_stable_context = (
            self.prompt_mode == "full"
            or self._stable_context_hash != context_hash
        )
        if send_stable_context:
            self._stable_context_hash = context_hash
            stable_context_payload = stable_context
            route_guidance_payload = route_guidance
        else:
            stable_context_payload = {"cached": True, "context_hash": context_hash}
            route_guidance_payload = {"cached": True, "context_hash": context_hash}
            if self.prompt_mode == "delta" and isinstance(view, dict):
                view = dict(view)
                if "run_history" in view:
                    view["run_history"] = {"cached": True}
        current_plan = None
        if self.plan_state is not None:
            # Caller owns async plan access; prompt can rely on latest UI state copy.
            current_plan = "see prior assistant messages"
        payload = {
            "objective": self.objective,
            "replan_reason": reason,
            "events": events[-10:],
            "user_messages": user_messages[-5:],
            "current_plan": current_plan,
            "planner_view": view,
            "route_guidance": route_guidance_payload,
            "stable_context": stable_context_payload,
            "prompt_mode": self.prompt_mode,
            "profile": self.profile_name,
        }
        if planner_error:
            payload["previous_plan_error"] = planner_error
            payload["retry_instruction"] = (
                "The previous planner response was rejected. Call submit_plan exactly "
                "once with a schema-valid Plan before selecting any route/control tools."
            )
        payload_json = json.dumps(payload, separators=(",", ":"))
        messages = [
            {"role": "system", "content": PLANNER_SYSTEM_PROMPT},
            *history,
            {"role": "user", "content": payload_json},
        ]
        if self.telemetry is not None:
            self.telemetry.record_prompt("planner", messages, {
                "system": len(PLANNER_SYSTEM_PROMPT.encode("utf-8")),
                "history": self._json_size(history),
                "payload": len(payload_json.encode("utf-8")),
                "planner_view": self._json_size(view),
                "stable_context": self._json_size(stable_context_payload),
                "route_guidance": self._json_size(route_guidance_payload),
                "events": self._json_size(events[-10:]),
                "run_history": self._json_size(view.get("run_history", [])),
            })
            self.telemetry.record_prompt_sections("planner", self.prompt_mode, context_hash, {
                "system": len(PLANNER_SYSTEM_PROMPT.encode("utf-8")),
                "history": self._json_size(history),
                "payload": len(payload_json.encode("utf-8")),
                "planner_view": self._json_size(view),
                "stable_context": self._json_size(stable_context_payload),
                "route_guidance": self._json_size(route_guidance_payload),
                "events": self._json_size(events[-10:]),
            })
        return messages

    def _prompt_history(self) -> list[dict]:
        compact: list[dict] = []
        for item in self.history_manager.recent_for_prompt(8):
            role = item.get("role")
            if role == "tool":
                compact.append({
                    "role": "tool",
                    "tool_call_id": item.get("tool_call_id", ""),
                    "content": json.dumps(
                        self._compact_tool_history_result(item.get("content")),
                        separators=(",", ":"),
                    ),
                })
                continue
            content = str(item.get("content") or "")
            try:
                plan = json.loads(content)
                if isinstance(plan, dict):
                    content = json.dumps({
                        key: plan.get(key)
                        for key in ("phase", "intent", "next_step", "deviation")
                        if plan.get(key) is not None
                    }, separators=(",", ":"))
            except (json.JSONDecodeError, TypeError, ValueError):
                content = content[:600]
            compact_message = {"role": role or "assistant", "content": content}
            if role == "assistant" and item.get("tool_calls"):
                compact_message["tool_calls"] = item.get("tool_calls")
            compact.append(compact_message)
        return compact

    @staticmethod
    def _compact_tool_history_result(content: object) -> dict:
        try:
            result = json.loads(str(content or "{}"))
        except (json.JSONDecodeError, TypeError, ValueError):
            return {"content": str(content or "")[:400]}
        if not isinstance(result, dict):
            return {"content": str(result)[:400]}
        return {
            key: result.get(key)
            for key in (
                "success",
                "error",
                "action",
                "request_id",
                "game_action",
                "waited_ms",
                "map_id",
                "current_map_id",
                "final_map_id",
                "reward_claimed",
                "quest_reward_claimed",
                "completed_dungeon_run",
                "completed_full_maintenance",
                "completed_town_setup",
                "completed_entry_travel",
                "completed_sparkfly_route_to_tekks",
                "completed_tekks_entry_prepare",
                "completed_recovery_travel",
                "waypoint_iterations",
                "chest_successes",
                "boss_completed",
                "recommended_next_action",
                "reason",
            )
            if result.get(key) is not None
        }

    @staticmethod
    def _json_size(value: object) -> int:
        return len(json.dumps(value, separators=(",", ":"), ensure_ascii=False).encode("utf-8"))

    @staticmethod
    def _json_hash(value: object) -> str:
        encoded = json.dumps(value, separators=(",", ":"), ensure_ascii=False, sort_keys=True).encode("utf-8")
        return hashlib.sha256(encoded).hexdigest()[:16]

    def _parse_plan(
        self,
        response: LLMResponse,
        snapshot: dict | None = None,
        reason: str = "",
    ) -> Plan:
        data = self._plan_data_from_tool_call(response)
        if data is not None:
            return Plan.from_dict(data)

        raise PlanValidationError("planner response did not call submit_plan")

    @staticmethod
    def _plan_data_from_tool_call(response: LLMResponse) -> dict | None:
        submit_plan_calls = [
            call for call in response.tool_calls if call.name == "submit_plan"
        ]
        if len(submit_plan_calls) > 1:
            raise PlanValidationError("planner response called submit_plan multiple times")
        if not submit_plan_calls:
            return None

        call = submit_plan_calls[0]
        data = call.parsed_arguments
        if not isinstance(data, dict):
            raise PlanValidationError("submit_plan arguments must be an object")
        return data
