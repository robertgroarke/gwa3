"""Planner role for the two-model bridge architecture."""

from __future__ import annotations

import json
import re
import time
from typing import Any

import httpx

from .action_dispatcher import ActionDispatcher
from .event_bus import BridgeEventBus
from .llm_client import LLMClient, LLMResponse, ToolCall
from .observation import build_planner_view
from .plan import Plan, PlanState, PlanValidationError
from .telemetry import BridgeTelemetry
from .tool_schema import PLANNER_ONLY_TOOL_NAMES, PLANNER_TOOLS


TEKKS_STAGE_X = 12061.0
TEKKS_STAGE_Y = 22485.0
TEKKS_SEARCH_X = 12396.0
TEKKS_SEARCH_Y = 22407.0
TEKKS_READY_DISTANCE = 1200.0
ROUTE_TOOL_NAMES = {
    "froggy_travel_to_gadds",
    "froggy_run_town_setup",
    "froggy_run_full_maintenance",
    "froggy_travel_to_sparkfly",
    "froggy_run_sparkfly_route_to_tekks",
    "froggy_prepare_tekks_dungeon_entry",
    "froggy_run_dungeon_loop",
    "return_to_outpost",
}
SNAPSHOT_REQUIRED_TOOL_NAMES = ROUTE_TOOL_NAMES | {"set_bot_state"}


PLANNER_SYSTEM_PROMPT = """\
You are the strategic planner for GWA3 Froggy HM advisory mode.

You own strategy only. The executor owns tactical tool calls. Emit exactly one JSON
Plan object in your assistant message. Do not use Markdown. Use schema version 1.
If you also emit tool calls, the assistant message content must still be the
complete Plan JSON object.

Required Plan fields:
- version: 1
- phase: short machine label
- phase_kind: one of route, long_walk, boss, dialog, merchant, combat
- intent: one concise sentence
- next_step: one concrete executor instruction
- constraints: string list
- expected_route: list of route/checkpoint objects, if useful
- abort_conditions: list with party_defeated and inventory_full at minimum
- fallback: safe fallback action
- trace_id: unique-ish string

Configured lane only. Froggy HM's native bot owns route/combat micro unless the
Plan explicitly calls for LLM control. If taking control, the planner may call
set_bot_state({"state":"llm_controlled"}) before emitting the Plan. The executor
must never invent strategy outside the Plan.

Froggy HM route gates:
- If map_id is not 638 (Gadd's Encampment), 558 (Sparkfly Swamp), 615, or 616
  (Bogroot Growths), first plan/call froggy_travel_to_gadds.
- Call froggy_run_town_setup only while map_id is 638.
- Call froggy_travel_to_sparkfly only after town setup from map_id 638.
- If a recent froggy_run_full_maintenance summary already exists for map_id 638
  and free_slots did not improve, do not repeat maintenance; proceed with
  froggy_run_town_setup, then froggy_travel_to_sparkfly.
- Call froggy_run_sparkfly_route_to_tekks only while map_id is 558.
- Call froggy_prepare_tekks_dungeon_entry only near Tekks in map_id 558.
- If map_id is 558 and the player is already near Tekks (about 1200 units of
  Tekks stage/search coords), do not repeat froggy_run_sparkfly_route_to_tekks;
  advance with froggy_prepare_tekks_dungeon_entry.
- Call froggy_run_dungeon_loop only while map_id is 615 or 616, or as explicit
  recovery from Sparkfly after Tekks entry state is ambiguous.
- If a completed dungeon still needs to leave map_id 615 or 616, call
  return_to_outpost before planning Gadd's maintenance.
"""


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
    ):
        self.llm = llm
        self.plan_state = plan_state
        self.dispatcher = dispatcher
        self.event_bus = event_bus
        self.telemetry = telemetry
        self.objective = objective or "Farm Bogroot Growths HM repeatedly with Froggy HM."
        self.run_history = run_history or []
        self.history: list[dict] = []
        self.invalid_json_count = 0
        self.unhealthy = False

    async def emit(self, event: str, data: dict | None = None) -> None:
        if self.event_bus is not None:
            await self.event_bus.emit(event, data or {})

    async def replan(
        self,
        reason: str,
        snapshot: dict | None,
        events: list[dict] | None = None,
        user_messages: list[str] | None = None,
    ) -> Plan | None:
        if self.telemetry is not None:
            self.telemetry.record_replan(reason)

        for attempt in range(2):
            started = time.perf_counter()
            try:
                response = await self.llm.chat_completion(
                    messages=self._build_messages(reason, snapshot, events or [], user_messages or []),
                    tools=PLANNER_TOOLS,
                    tool_choice="auto",
                    temperature=0.1,
                    max_tokens=1400,
                )
                if self.telemetry is not None:
                    self.telemetry.record_llm_call("planner", time.perf_counter() - started, response.usage)
                plan = self._parse_plan(response, snapshot=snapshot, reason=reason)
                await self.plan_state.replace(plan)
                self.history.append({"role": "assistant", "content": json.dumps(plan.to_dict())})
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
                    continue
                self.unhealthy = True
                return await self._fallback_plan(reason, snapshot)
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
        allowed = PLANNER_ONLY_TOOL_NAMES | {"wait"}
        calls = self._with_inferred_route_tool(response.tool_calls, plan, snapshot)
        calls = self._align_route_calls_to_plan(calls, plan, snapshot)
        calls = self._apply_route_loop_guards(calls, snapshot)
        if self._requires_llm_control(calls):
            control_call = self._first_control_takeover(calls)
            control = LLMResponse(tool_calls=[
                control_call or ToolCall(
                    id="planner-control",
                    name="set_bot_state",
                    arguments='{"state":"llm_controlled"}',
                )
            ])
            summary = await self.dispatcher.execute_response(
                control,
                role="planner",
                allowed_tool_names=allowed,
                history=self.history,
            )
            if any(not result.get("success") for result in summary.results):
                await self.emit("degradation", {
                    "reason": "planner_control_takeover_failed",
                    "results": summary.results,
                })
                return
            if control_call is not None:
                calls = [call for call in calls if call is not control_call]

        await self.dispatcher.execute_response(
            LLMResponse(content=response.content, tool_calls=calls, usage=response.usage),
            role="planner",
            allowed_tool_names=allowed,
            history=self.history,
        )

    def _with_inferred_route_tool(
        self,
        calls: list[ToolCall],
        plan: Plan | None,
        snapshot: dict | None,
    ) -> list[ToolCall]:
        if plan is None or snapshot is None:
            return calls

        route_tool = self._preferred_route_tool_from_plan(plan, snapshot)

        if route_tool is None:
            return calls
        if any(call.name in ROUTE_TOOL_NAMES for call in calls):
            return calls
        if any(call.name not in {"wait", "query_state", "set_bot_state"} for call in calls):
            return calls
        calls = [call for call in calls if call.name != "query_state"]
        return [
            *calls,
            ToolCall(id=f"inferred-{route_tool}", name=route_tool, arguments="{}"),
        ]

    def _align_route_calls_to_plan(
        self,
        calls: list[ToolCall],
        plan: Plan | None,
        snapshot: dict | None,
    ) -> list[ToolCall]:
        """Make the accepted Plan authoritative over conflicting planner tool calls."""
        if plan is None or snapshot is None:
            return calls
        preferred = self._preferred_route_tool_from_plan(plan, snapshot)
        if preferred is None:
            return calls

        aligned: list[ToolCall] = []
        replaced_route = False
        saw_route = False
        for call in calls:
            if call.name not in ROUTE_TOOL_NAMES:
                aligned.append(call)
                continue
            saw_route = True
            if not replaced_route:
                aligned.append(ToolCall(
                    id=f"{call.id}-plan-aligned",
                    name=preferred,
                    arguments="{}",
                ))
                replaced_route = True
        if not saw_route:
            return calls
        return aligned

    def _preferred_route_tool_from_plan(self, plan: Plan, snapshot: dict | None) -> str | None:
        map_id = self._snapshot_map_id(snapshot)
        next_step_text = str(plan.next_step or "").lower()
        broad_text = " ".join(
            str(part or "")
            for part in (
                plan.phase,
                plan.intent,
                plan.next_step,
                " ".join(str(item or "") for item in (plan.constraints or [])),
            )
        ).lower()

        def mentions(tool_name: str) -> bool:
            return tool_name in next_step_text or (
                tool_name in broad_text and "fallback" not in next_step_text
            )

        if "return_to_outpost" in next_step_text and map_id in {615, 616}:
            return "return_to_outpost"
        if "froggy_travel_to_gadds" in next_step_text and map_id != 638:
            return "froggy_travel_to_gadds"
        if "froggy_run_full_maintenance" in next_step_text and map_id == 638:
            return "froggy_run_full_maintenance"
        if "froggy_run_town_setup" in next_step_text and map_id == 638:
            return "froggy_run_town_setup"
        if "froggy_travel_to_sparkfly" in next_step_text and map_id == 638:
            return "froggy_travel_to_sparkfly"
        if (
            map_id == 558
            and self._is_near_tekks(snapshot)
            and (
                "froggy_prepare_tekks_dungeon_entry" in next_step_text
                or "froggy_run_sparkfly_route_to_tekks" in next_step_text
            )
        ):
            return "froggy_prepare_tekks_dungeon_entry"
        if "froggy_run_sparkfly_route_to_tekks" in next_step_text and map_id == 558:
            return "froggy_run_sparkfly_route_to_tekks"
        if "froggy_prepare_tekks_dungeon_entry" in next_step_text and map_id == 558:
            return "froggy_prepare_tekks_dungeon_entry"
        if "froggy_run_dungeon_loop" in next_step_text:
            if map_id in {615, 616}:
                return "froggy_run_dungeon_loop"
            if map_id == 558 and any(marker in broad_text for marker in ("tekks", "dialog", "ambiguous", "recovery")):
                return "froggy_run_dungeon_loop"

        near_tekks = self._is_near_tekks(snapshot)
        if "return_to_outpost" in broad_text and map_id in {615, 616}:
            return "return_to_outpost"
        if mentions("froggy_travel_to_gadds") and map_id != 638:
            return "froggy_travel_to_gadds"
        if (
            mentions("froggy_run_full_maintenance")
            and map_id == 638
            and not self._recent_maintenance_saturated(snapshot)
        ):
            return "froggy_run_full_maintenance"
        if mentions("froggy_run_town_setup") and map_id == 638:
            return "froggy_run_town_setup"
        if mentions("froggy_travel_to_sparkfly") and map_id == 638:
            return "froggy_travel_to_sparkfly"
        if (
            map_id == 558
            and near_tekks
            and (
                mentions("froggy_prepare_tekks_dungeon_entry")
                or mentions("froggy_run_sparkfly_route_to_tekks")
            )
        ):
            return "froggy_prepare_tekks_dungeon_entry"
        if mentions("froggy_run_sparkfly_route_to_tekks") and map_id == 558:
            return "froggy_run_sparkfly_route_to_tekks"
        if mentions("froggy_prepare_tekks_dungeon_entry") and map_id == 558:
            return "froggy_prepare_tekks_dungeon_entry"
        if mentions("froggy_run_dungeon_loop"):
            if map_id in {615, 616}:
                return "froggy_run_dungeon_loop"
            if map_id == 558 and any(marker in broad_text for marker in ("tekks", "dialog", "ambiguous", "recovery")):
                return "froggy_run_dungeon_loop"
        return None

    def _apply_route_loop_guards(
        self,
        calls: list[ToolCall],
        snapshot: dict | None,
    ) -> list[ToolCall]:
        if self._snapshot_map_id(snapshot) == 0:
            return self._replace_snapshot_required_calls(calls, "snapshot-required")
        guarded: list[ToolCall] = []
        replaced = False
        near_tekks = self._is_near_tekks(snapshot)
        recently_routed_to_tekks = self._recent_successful_tool("froggy_run_sparkfly_route_to_tekks")
        recently_prepared_tekks = self._recent_successful_tool("froggy_prepare_tekks_dungeon_entry")
        recently_completed_dungeon = self._recent_completed_dungeon_without_new_entry()
        recently_ran_town_setup = self._recent_successful_tool("froggy_run_town_setup")
        map_id = self._snapshot_map_id(snapshot)
        for call in calls:
            if (
                call.name in {"froggy_travel_to_gadds", "return_to_outpost", "froggy_travel_to_sparkfly"}
                and map_id == 558
            ):
                replaced = True
                next_tool = (
                    "froggy_prepare_tekks_dungeon_entry"
                    if near_tekks or recently_routed_to_tekks
                    else "froggy_run_sparkfly_route_to_tekks"
                )
                guarded.append(ToolCall(
                    id=f"{call.id}-sparkfly-route-guard",
                    name=next_tool,
                    arguments="{}",
                ))
                continue
            if call.name == "froggy_run_dungeon_loop" and recently_completed_dungeon:
                replaced = True
                if map_id == 638:
                    if self._snapshot_needs_maintenance(snapshot) and not self._recent_maintenance_saturated(snapshot):
                        next_tool = "froggy_run_full_maintenance"
                    else:
                        next_tool = "froggy_run_town_setup"
                else:
                    next_tool = "query_state"
                guarded.append(ToolCall(
                    id=f"{call.id}-post-completion-refresh-guard",
                    name=next_tool,
                    arguments="{}",
                ))
                continue
            if (
                call.name == "froggy_travel_to_sparkfly"
                and map_id == 638
                and self._snapshot_needs_maintenance(snapshot)
                and self._recent_maintenance_saturated(snapshot)
                and recently_ran_town_setup
            ):
                guarded.append(call)
                continue
            if (
                call.name in {"froggy_run_dungeon_loop", "return_to_outpost", "froggy_travel_to_sparkfly"}
                and map_id == 638
                and self._snapshot_needs_maintenance(snapshot)
                and not self._recent_maintenance_saturated(snapshot)
            ):
                replaced = True
                guarded.append(ToolCall(
                    id=f"{call.id}-post-run-maintenance-guard",
                    name="froggy_run_full_maintenance",
                    arguments="{}",
                ))
                continue
            if (
                call.name in {"froggy_run_dungeon_loop", "return_to_outpost", "froggy_travel_to_sparkfly"}
                and map_id == 638
                and self._snapshot_needs_maintenance(snapshot)
                and self._recent_maintenance_saturated(snapshot)
            ):
                replaced = True
                guarded.append(ToolCall(
                    id=f"{call.id}-post-run-saturated-maintenance-guard",
                    name="froggy_run_town_setup",
                    arguments="{}",
                ))
                continue
            if call.name in {"froggy_run_dungeon_loop", "return_to_outpost"} and map_id == 638:
                replaced = True
                guarded.append(ToolCall(
                    id=f"{call.id}-post-run-town-guard",
                    name="froggy_run_town_setup",
                    arguments="{}",
                ))
                continue
            if (
                call.name == "froggy_prepare_tekks_dungeon_entry"
                and map_id == 558
                and not near_tekks
                and not recently_routed_to_tekks
            ):
                replaced = True
                guarded.append(ToolCall(
                    id=f"{call.id}-tekks-distance-guard",
                    name="froggy_run_sparkfly_route_to_tekks",
                    arguments="{}",
                ))
                continue
            if (
                call.name == "froggy_prepare_tekks_dungeon_entry"
                and map_id in {558, 615, 616}
                and recently_prepared_tekks
            ):
                replaced = True
                guarded.append(ToolCall(
                    id=f"{call.id}-tekks-entry-guard",
                    name="froggy_run_dungeon_loop",
                    arguments="{}",
                ))
                continue
            if (
                call.name == "froggy_run_sparkfly_route_to_tekks"
                and map_id == 558
                and (near_tekks or recently_routed_to_tekks)
            ):
                replaced = True
                guarded.append(ToolCall(
                    id=f"{call.id}-tekks-loop-guard",
                    name="froggy_prepare_tekks_dungeon_entry",
                    arguments="{}",
                ))
                continue
            if call.name == "froggy_run_full_maintenance" and self._recent_maintenance_saturated(snapshot):
                replaced = True
                guarded.append(ToolCall(
                    id=f"{call.id}-maintenance-loop-guard",
                    name="froggy_run_town_setup",
                    arguments="{}",
                ))
                continue
            if call.name != "froggy_run_full_maintenance":
                guarded.append(call)
                continue
            guarded.append(call)
        if replaced:
            return guarded
        return calls

    def _replace_snapshot_required_calls(self, calls: list[ToolCall], suffix: str) -> list[ToolCall]:
        guarded: list[ToolCall] = []
        replaced = False
        query_added = False
        for call in calls:
            if call.name in SNAPSHOT_REQUIRED_TOOL_NAMES:
                replaced = True
                if not query_added:
                    guarded.append(ToolCall(
                        id=f"{call.id}-{suffix}",
                        name="query_state",
                        arguments='{"wait_ms":250}',
                    ))
                    query_added = True
                continue
            guarded.append(call)
        if replaced:
            return guarded or [
                ToolCall(id=f"planner-{suffix}", name="query_state", arguments='{"wait_ms":250}')
            ]
        return calls

    @staticmethod
    def _snapshot_map_id(snapshot: dict | None) -> int:
        map_state = (snapshot or {}).get("map") or {}
        try:
            return int(map_state.get("map_id") or 0)
        except (TypeError, ValueError):
            return 0

    @staticmethod
    def _is_near_tekks(snapshot: dict | None) -> bool:
        if PlannerLoop._snapshot_map_id(snapshot) != 558:
            return False
        me = (snapshot or {}).get("me") or {}
        try:
            x = float(me.get("x") or 0.0)
            y = float(me.get("y") or 0.0)
        except (TypeError, ValueError):
            return False
        if x == 0.0 and y == 0.0:
            return False
        stage_dist = ((x - TEKKS_STAGE_X) ** 2 + (y - TEKKS_STAGE_Y) ** 2) ** 0.5
        search_dist = ((x - TEKKS_SEARCH_X) ** 2 + (y - TEKKS_SEARCH_Y) ** 2) ** 0.5
        return min(stage_dist, search_dist) <= TEKKS_READY_DISTANCE

    def _recent_successful_tool(self, tool_name: str, lookback: int = 12) -> bool:
        return self._recent_success_index(tool_name, lookback=lookback) is not None

    def _recent_success_index(self, tool_name: str, lookback: int = 20) -> int | None:
        start = max(0, len(self.history) - lookback)
        for index in range(len(self.history) - 1, start - 1, -1):
            item = self.history[index]
            if item.get("role") != "tool":
                continue
            try:
                result = json.loads(item.get("content") or "{}")
            except (TypeError, ValueError, json.JSONDecodeError):
                continue
            if result.get("action") == tool_name and result.get("success") is True:
                return index
        return None

    def _recent_completed_dungeon_without_new_entry(self) -> bool:
        completed = self._recent_success_index("froggy_run_dungeon_loop", lookback=30)
        if completed is None:
            return False
        prepared = self._recent_success_index("froggy_prepare_tekks_dungeon_entry", lookback=30)
        return prepared is None or completed > prepared

    def _recent_maintenance_saturated(self, snapshot: dict | None) -> bool:
        if not snapshot:
            return False
        map_state = snapshot.get("map") or {}
        try:
            map_id = int(map_state.get("map_id") or 0)
        except (TypeError, ValueError):
            map_id = 0
        if map_id != 638:
            return False
        inventory = snapshot.get("inventory") or {}
        try:
            current_free_slots = int(inventory.get("free_slots_total"))
        except (TypeError, ValueError):
            return False
        for summary in reversed(self.run_history[-5:]):
            if summary.get("reason") != "tool:froggy_run_full_maintenance":
                continue
            try:
                summary_map = int(summary.get("map") or 0)
                summary_free_slots = int(summary.get("free_slots"))
                captured_at = float(summary.get("captured_at") or 0.0)
            except (TypeError, ValueError):
                continue
            if summary_map != 638:
                continue
            if time.time() - captured_at > 900:
                continue
            return current_free_slots <= summary_free_slots
        return False

    @staticmethod
    def _snapshot_needs_maintenance(snapshot: dict | None, minimum_free_slots: int = 5) -> bool:
        inventory = (snapshot or {}).get("inventory") or {}
        try:
            return int(inventory.get("free_slots_total")) < minimum_free_slots
        except (TypeError, ValueError):
            return False

    @staticmethod
    def _first_control_takeover(calls: list[ToolCall]) -> ToolCall | None:
        for call in calls:
            if call.name != "set_bot_state":
                continue
            try:
                args = call.parsed_arguments
            except (json.JSONDecodeError, TypeError, ValueError):
                continue
            if args.get("state") == "llm_controlled":
                return call
        return None

    @staticmethod
    def _requires_llm_control(calls: list[ToolCall]) -> bool:
        local_or_status = {
            "wait",
            "query_state",
            "search_trade_prices",
            "get_recipe",
            "get_outpost_info",
            "get_material_info",
            "get_dungeon_info",
            "get_blessing_info",
            "get_hero_build",
            "get_quest_info",
        }
        return any(call.name not in local_or_status and call.name != "set_bot_state" for call in calls)

    def _build_messages(
        self,
        reason: str,
        snapshot: dict | None,
        events: list[dict],
        user_messages: list[str],
    ) -> list[dict]:
        view = build_planner_view(snapshot, self.run_history)
        history = self._prompt_history()
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
        }
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
                "events": self._json_size(events[-10:]),
                "run_history": self._json_size(view.get("run_history", [])),
            })
        return messages

    def _prompt_history(self) -> list[dict]:
        compact: list[dict] = []
        for item in self.history[-8:]:
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
            compact.append({"role": role or "assistant", "content": content})
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
                "final_map_id",
            )
            if result.get(key) is not None
        }

    @staticmethod
    def _json_size(value: object) -> int:
        return len(json.dumps(value, separators=(",", ":"), ensure_ascii=False).encode("utf-8"))

    def _parse_plan(
        self,
        response: LLMResponse,
        snapshot: dict | None = None,
        reason: str = "",
    ) -> Plan:
        try:
            data = extract_plan_json(response.content or "")
            return Plan.from_dict(data)
        except (PlanValidationError, json.JSONDecodeError, TypeError, ValueError):
            repaired = self._repair_plan_from_tool_call(response, snapshot, reason)
            if repaired is not None:
                return repaired
            raise

    def _repair_plan_from_tool_call(
        self,
        response: LLMResponse,
        snapshot: dict | None,
        reason: str,
    ) -> Plan | None:
        """Recover when codex-exec returns a tool call but omits Plan JSON."""
        for call in response.tool_calls:
            if call.name == "wait":
                continue
            if call.name not in PLANNER_ONLY_TOOL_NAMES:
                continue
            phase, phase_kind, intent = _plan_repair_labels(call.name, snapshot)
            return Plan(
                phase=phase,
                phase_kind=phase_kind,
                intent=intent,
                next_step=f"call {call.name}",
                deviation="planner_content_repaired_from_tool_call",
                constraints=[
                    "configured_lane_only",
                    "planner_tool_call_already_selected",
                    "executor_must_not_freelance",
                ],
                abort_conditions=[
                    {"kind": "party_defeated"},
                    {"kind": "inventory_full"},
                ],
                fallback="handoff_to_froggy",
                trace_id=f"repair-{int(time.time())}-{reason}-{call.name}",
            )
        return None

    async def _fallback_plan(self, reason: str, snapshot: dict | None) -> Plan:
        phase = ((snapshot or {}).get("bot") or {}).get("state") or "fallback"
        plan = Plan(
            phase=str(phase),
            phase_kind="route",
            intent="Planner output was invalid; keep Froggy in control while waiting for the next valid plan.",
            next_step="wait",
            constraints=["do_not_freelance", "froggy_native_control"],
            abort_conditions=[{"kind": "party_defeated"}, {"kind": "inventory_full"}],
            fallback="handoff_to_froggy",
            trace_id=f"fallback-{int(time.time())}-{reason}",
        )
        await self.plan_state.replace(plan)
        await self.emit("plan.updated", plan.to_dict())
        await self.emit("degradation", {"reason": "single_model_fallback:planner_invalid_json"})
        await self.dispatcher.force_game_action(
            "set_bot_state",
            {"state": "idle"},
            role="planner",
            request_id="planner-fallback-handoff",
        )
        if self.telemetry is not None:
            self.telemetry.record_degradation("planner_invalid_json")
        return plan


def extract_plan_json(text: str) -> dict[str, Any]:
    stripped = text.strip()
    if stripped.startswith("```"):
        stripped = re.sub(r"^```(?:json)?\s*", "", stripped)
        stripped = re.sub(r"\s*```$", "", stripped)
    if not stripped.startswith("{"):
        match = re.search(r"\{.*\}", stripped, re.DOTALL)
        if not match:
            raise PlanValidationError("planner response did not contain a JSON object")
        stripped = match.group(0)
    data = json.loads(stripped)
    if not isinstance(data, dict):
        raise PlanValidationError("planner response JSON was not an object")
    return data


def _plan_repair_labels(tool_name: str, snapshot: dict | None) -> tuple[str, str, str]:
    map_id = PlannerLoop._snapshot_map_id(snapshot)
    labels = {
        "froggy_run_full_maintenance": (
            "maintenance",
            "merchant",
            "Run town maintenance because the planner selected the maintenance tool.",
        ),
        "froggy_run_town_setup": (
            "town_setup",
            "route",
            "Prepare the Froggy HM run because the planner selected town setup.",
        ),
        "froggy_travel_to_sparkfly": (
            "travel_to_sparkfly",
            "long_walk",
            "Travel to Sparkfly because the planner selected the Sparkfly travel tool.",
        ),
        "froggy_run_sparkfly_route_to_tekks": (
            "route_to_tekks",
            "long_walk",
            "Route to Tekks because the planner selected the Sparkfly-to-Tekks tool.",
        ),
        "froggy_prepare_tekks_dungeon_entry": (
            "tekks_entry",
            "dialog",
            "Enter Bogroot Growths because the planner selected the Tekks entry tool.",
        ),
        "froggy_run_dungeon_loop": (
            "dungeon_loop",
            "boss" if map_id == 616 else "long_walk",
            "Run the Froggy HM dungeon loop because the planner selected it.",
        ),
        "return_to_outpost": (
            "return_to_outpost",
            "route",
            "Return to outpost because the planner selected the outpost handoff tool.",
        ),
        "set_bot_state": (
            "bot_state",
            "route",
            "Update bot state because the planner selected a state transition.",
        ),
    }
    return labels.get(tool_name, (
        tool_name,
        "route",
        f"Execute planner-selected tool {tool_name}.",
    ))
