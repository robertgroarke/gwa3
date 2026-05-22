"""Shared tool execution for planner and executor bridge roles."""

from __future__ import annotations

import asyncio
import json
import time
import uuid
from dataclasses import dataclass, field
from typing import Awaitable, Callable

import httpx

from . import farming_knowledge
from .event_bus import BridgeEventBus
from .ipc_client import IpcClient
from .kamadan_client import KamadanClient
from .llm_client import LLMResponse, ToolCall


ACTION_RESULT_TIMEOUT_SECONDS = 2.0
CONTROL_ACTION_RESULT_TIMEOUT_SECONDS = 180.0
PLANNER_ACTION_RESULT_TIMEOUT_SECONDS = 3600.0
LOCAL_WAIT_POLL_SECONDS = 0.25
POST_ACTION_OBSERVATION_SECONDS = 0.75
MAP_TRANSITION_OBSERVATION_SECONDS = 2.0
ADVISORY_CONTROL_ACTION = "set_bot_state"
ADVISORY_STATUS_ACTIONS = {"query_state"}
MAP_TRANSITION_ACTIONS = {
    "froggy_prepare_tekks_dungeon_entry",
    "froggy_run_dungeon_loop",
    "froggy_travel_to_sparkfly",
    "froggy_travel_to_gadds",
    "return_to_outpost",
}
ADVISORY_ASSUMED_CONTROL_ACTIONS = MAP_TRANSITION_ACTIONS | {
    "froggy_refresh_combat_skillbar",
    "froggy_run_full_maintenance",
    "froggy_run_sparkfly_route_to_tekks",
    "froggy_run_town_setup",
}
RETURN_TO_OUTPOST_ALLOWED_MAPS = {615, 616}
TOOL_RESULT_EVENT_FIELDS = (
    "action",
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
    "blocked_tool",
    "bot_state",
    "bot_phase",
    "route_progress",
    "connection_state",
    "connection_disconnected",
    "disconnect_detected",
    "disconnect_code",
    "screenshot_path",
)


def build_tool_result_event_payload(
    request_id: str,
    result: dict,
    *,
    orphan: bool = False,
) -> dict:
    """Build the compact `tool.result` payload exposed to UI state."""
    payload = {
        "request_id": result.get("request_id", request_id),
        "success": bool(result.get("success", False)),
        "error": result.get("error"),
    }
    if orphan:
        payload["orphan"] = True
    for key in TOOL_RESULT_EVENT_FIELDS:
        if result.get(key) is not None:
            payload[key] = result.get(key)
    return payload


@dataclass
class ToolExecutionSummary:
    had_tool_call: bool = False
    had_game_action: bool = False
    results: list[dict] = field(default_factory=list)
    idle_reason: str = ""
    counts_as_stall: bool = False


class ActionDispatcher:
    """Executes LLM tool calls and emits the shared UI event contract."""

    def __init__(
        self,
        ipc: IpcClient,
        event_bus: BridgeEventBus | None = None,
        kamadan_client: KamadanClient | None = None,
        latest_snapshot: Callable[[], dict | None] | None = None,
        collect_observations: Callable[[], Awaitable[None]] | None = None,
        on_tool_result: Callable[[dict], Awaitable[None]] | None = None,
        autonomy: str = "advisory",
    ):
        self.ipc = ipc
        self.event_bus = event_bus
        self.kamadan = kamadan_client or KamadanClient()
        self.latest_snapshot = latest_snapshot or (lambda: None)
        self.collect_observations = collect_observations
        self.on_tool_result = on_tool_result
        self.autonomy = autonomy
        self._assume_llm_control_until = 0.0

    async def emit(self, event: str, data: dict | None = None) -> None:
        if self.event_bus is not None:
            await self.event_bus.emit(event, data or {})

    async def execute_response(
        self,
        response: LLMResponse,
        role: str,
        allowed_tool_names: set[str] | None = None,
        history: list[dict] | None = None,
    ) -> ToolExecutionSummary:
        summary = ToolExecutionSummary(had_tool_call=bool(response.tool_calls))
        for tool_call in response.tool_calls:
            result = await self.execute_tool_call(
                tool_call,
                role=role,
                allowed_tool_names=allowed_tool_names,
                history=history,
            )
            summary.results.append(result)
            summary.had_game_action = summary.had_game_action or result.get("game_action", False)
        return summary

    async def execute_tool_call(
        self,
        tool_call: ToolCall,
        role: str,
        allowed_tool_names: set[str] | None = None,
        history: list[dict] | None = None,
        request_id: str | None = None,
    ) -> dict:
        req_id = request_id or str(uuid.uuid4())[:8]
        try:
            params = tool_call.parsed_arguments
        except (json.JSONDecodeError, TypeError, ValueError) as exc:
            result = {
                "success": False,
                "error": f"invalid_tool_arguments:{type(exc).__name__}",
                "action": tool_call.name,
                "request_id": req_id,
            }
            await self._emit_result(req_id, result)
            self._append_history(history, tool_call, result)
            return result

        await self.emit("tool.call", {
            "role": role,
            "name": tool_call.name,
            "args": params,
            "request_id": req_id,
        })

        if allowed_tool_names is not None and tool_call.name not in allowed_tool_names:
            result = {
                "success": False,
                "error": "tool_not_permitted_for_role",
                "action": tool_call.name,
                "request_id": req_id,
            }
            await self._emit_result(req_id, result)
            self._append_history(history, tool_call, result)
            return result

        local = await self._execute_local_tool(tool_call.name, params)
        if local is not None:
            local.setdefault("action", tool_call.name)
            local.setdefault("request_id", req_id)
            await self._emit_result(req_id, local)
            self._append_history(history, tool_call, local)
            return local

        blocked = self._advisory_block_reason(tool_call.name, params)
        if blocked is not None:
            result = {
                "success": False,
                "error": blocked,
                "action": tool_call.name,
                "request_id": req_id,
            }
            await self._emit_result(req_id, result)
            self._append_history(history, tool_call, result)
            return result

        timeout = self._action_result_timeout(tool_call.name, role)
        result = await self._execute_game_action(tool_call.name, params, req_id, timeout)
        result["game_action"] = True
        self._remember_control_result(tool_call.name, params, result)
        await self._emit_result(req_id, result)
        self._append_history(history, tool_call, result)
        return result

    async def force_game_action(
        self,
        action_name: str,
        params: dict | None,
        role: str,
        request_id: str | None = None,
    ) -> dict:
        """Send a non-LLM tool action, used for health handoff."""
        req_id = request_id or str(uuid.uuid4())[:8]
        await self.emit("tool.call", {
            "role": role,
            "name": action_name,
            "args": params or {},
            "request_id": req_id,
        })
        result = await self._execute_game_action(
            action_name,
            params or {},
            req_id,
            self._action_result_timeout(action_name, role),
        )
        result["game_action"] = True
        self._remember_control_result(action_name, params or {}, result)
        await self._emit_result(req_id, result)
        return result

    async def _execute_local_tool(self, name: str, params: dict) -> dict | None:
        if name == "wait":
            ms = params.get("milliseconds", 500)
            await self._wait_locally(ms)
            return {"success": True, "waited_ms": ms}

        if name == "search_trade_prices":
            query = params.get("query", "")
            count = min(params.get("count", 10), 25)
            try:
                data = await self.kamadan.search_for_llm(query, count=count)
                return {"success": True, "data": data}
            except (asyncio.TimeoutError, httpx.HTTPError, OSError, ValueError) as exc:
                return {"success": False, "error": str(exc)}

        lookup = {
            "get_recipe": lambda: farming_knowledge.get_recipe(params.get("consumable_model_id", 0)),
            "get_outpost_info": lambda: farming_knowledge.get_outpost_info(params.get("map_id", 0)),
            "get_material_info": lambda: farming_knowledge.get_material_info(params.get("model_id", 0)),
            "get_dungeon_info": lambda: farming_knowledge.get_dungeon_info(params.get("name", "")),
            "get_blessing_info": lambda: farming_knowledge.get_blessing_info(params.get("blessing_type", "")),
            "get_hero_build": lambda: farming_knowledge.get_hero_build(params.get("hero_name", "")),
            "get_quest_info": lambda: farming_knowledge.get_quest_info(params.get("key", "")),
        }.get(name)
        if lookup is None:
            return None
        data = lookup()
        return {"success": "error" not in data, "data": data, "error": data.get("error")}

    async def _execute_game_action(
        self,
        name: str,
        params: dict,
        request_id: str,
        timeout: float = ACTION_RESULT_TIMEOUT_SECONDS,
    ) -> dict:
        pending_result = None
        expect_result = self._pending_result_registrar()
        if expect_result is not None:
            pending_result = expect_result(request_id)
        try:
            await self.ipc.send_action(name, params, request_id)
        except Exception as exc:
            forget_result = getattr(self.ipc, "forget_action_result", None)
            if forget_result is not None:
                forget_result(request_id, pending_result)
            return {
                "success": False,
                "error": f"send_failed:{type(exc).__name__}",
                "action": name,
                "request_id": request_id,
            }
        if self.collect_observations is not None:
            await asyncio.sleep(0.1)
            await self.collect_observations()
        result = await self._await_action_result(name, request_id, pending_result, timeout)
        if self.collect_observations is not None:
            await self.collect_observations()
            await self._request_fresh_snapshot_after_action(name)
        return result

    def _pending_result_registrar(self):
        expect_result = getattr(self.ipc, "expect_action_result", None)
        wait_for_result = getattr(self.ipc, "wait_for_action_result", None)
        if not callable(expect_result) or not asyncio.iscoroutinefunction(wait_for_result):
            return None
        return expect_result

    async def _await_action_result(
        self,
        action_name: str,
        request_id: str,
        pending_result=None,
        timeout: float = ACTION_RESULT_TIMEOUT_SECONDS,
    ) -> dict:
        wait_for_result = getattr(self.ipc, "wait_for_action_result", None)
        if not asyncio.iscoroutinefunction(wait_for_result):
            return {
                "success": False,
                "error": "no_reply",
                "action": action_name,
                "request_id": request_id,
            }
        try:
            result = await wait_for_result(
                request_id,
                timeout=timeout,
                future=pending_result,
            )
        except asyncio.TimeoutError:
            return {
                "success": False,
                "error": "no_reply",
                "action": action_name,
                "request_id": request_id,
            }
        return {
            "success": bool(result.get("success", False)),
            "error": result.get("error"),
            "action": action_name,
            "request_id": result.get("request_id", request_id),
        }

    async def _wait_locally(self, milliseconds: int | float) -> None:
        try:
            seconds = max(0.0, float(milliseconds) / 1000.0)
        except (TypeError, ValueError):
            seconds = 0.5
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if self.collect_observations is not None:
                await self.collect_observations()
            remaining = deadline - time.monotonic()
            await asyncio.sleep(min(LOCAL_WAIT_POLL_SECONDS, max(0.0, remaining)))

    async def _request_fresh_snapshot_after_action(self, action_name: str) -> None:
        if action_name == "query_state":
            return
        try:
            await self.ipc.send_action("query_state", {"wait_ms": 250}, "")
            # Map-changing helpers often emit dialog, heartbeat, unload/load,
            # and stale periodic snapshots in a burst. Drain long enough for
            # the post-action map snapshot to reach the planner before it
            # chooses the next route helper.
            deadline = time.monotonic() + (
                MAP_TRANSITION_OBSERVATION_SECONDS
                if action_name in MAP_TRANSITION_ACTIONS
                else POST_ACTION_OBSERVATION_SECONDS
            )
            while time.monotonic() < deadline:
                await asyncio.sleep(0.15)
                if self.collect_observations is not None:
                    await self.collect_observations()
        except Exception:
            return

    def _advisory_block_reason(self, action_name: str, params: dict | None = None) -> str | None:
        if action_name == "return_to_outpost":
            unsafe = self._unsafe_return_to_outpost_reason()
            if unsafe is not None:
                return unsafe
        if self.autonomy != "advisory":
            return None
        snapshot = self.latest_snapshot() or {}
        bot = snapshot.get("bot", {}) or {}
        state = bot.get("state")
        phase = bot.get("phase") or state or "unknown"
        if action_name in ADVISORY_STATUS_ACTIONS:
            return None
        if action_name == ADVISORY_CONTROL_ACTION:
            target_state = (params or {}).get("state")
            if target_state == "llm_controlled":
                if state == "llm_controlled":
                    return None
                if (
                    bot.get("safe_to_enter_llm_control", False)
                    or self._snapshot_implies_safe_llm_control(snapshot, bot)
                    or self._snapshot_implies_safe_route_override(snapshot, bot)
                ):
                    return None
                return f"bot_busy:state={state or 'unknown'}:phase={phase}"
            if target_state in {"idle", "in_town"} and self._snapshot_implies_safe_town_override(snapshot, bot):
                return None
            if state == "llm_controlled":
                return None
            return f"advisory_defers_to_froggy:bot_state={state or 'unknown'}"
        assumed_control = time.monotonic() < self._assume_llm_control_until
        if assumed_control and action_name in ADVISORY_ASSUMED_CONTROL_ACTIONS:
            return None
        if (state == "llm_controlled" or assumed_control) and bot.get("safe_for_llm_game_action", True):
            return None
        if state == "llm_controlled":
            return f"bot_busy:state={state}:phase={phase}"
        return f"advisory_defers_to_froggy:bot_state={state or 'unknown'}"

    def _unsafe_return_to_outpost_reason(self) -> str | None:
        snapshot = self.latest_snapshot()
        if not snapshot:
            return "unsafe_return_to_outpost:no_snapshot"
        map_state = snapshot.get("map") or {}
        try:
            map_id = int(map_state.get("map_id") or 0)
        except (TypeError, ValueError):
            return "unsafe_return_to_outpost:map_id=unknown"
        if map_id not in RETURN_TO_OUTPOST_ALLOWED_MAPS:
            return f"unsafe_return_to_outpost:map_id={map_id}"
        loading = map_state.get("loading_state")
        if loading not in (None, 1):
            return f"unsafe_return_to_outpost:loading_state={loading}"
        return None

    def _remember_control_result(self, action_name: str, params: dict, result: dict) -> None:
        if action_name != ADVISORY_CONTROL_ACTION or not result.get("success"):
            return
        target_state = params.get("state")
        if target_state == "llm_controlled":
            self._assume_llm_control_until = time.monotonic() + 30.0
        elif target_state:
            self._assume_llm_control_until = 0.0

    @staticmethod
    def _snapshot_implies_safe_llm_control(snapshot: dict, bot: dict) -> bool:
        """Fallback safety check for snapshots that predate explicit safe flags."""
        state = bot.get("state")
        phase = bot.get("phase") or state
        if state not in (None, "idle", "in_town"):
            if state == "traveling" and ActionDispatcher._snapshot_implies_safe_town_override(snapshot, bot):
                return True
            return False
        if phase not in (None, "unknown", state, "idle", "in_town"):
            return False
        me = snapshot.get("me", {}) or {}
        if me.get("is_moving") or me.get("is_casting"):
            return False
        map_state = snapshot.get("map", {}) or {}
        loading = map_state.get("loading_state")
        return loading in (None, 1)

    @staticmethod
    def _snapshot_implies_safe_town_override(snapshot: dict, bot: dict) -> bool:
        state = bot.get("state")
        if state not in (None, "idle", "in_town", "traveling"):
            return False
        map_state = snapshot.get("map", {}) or {}
        try:
            map_id = int(map_state.get("map_id") or 0)
        except (TypeError, ValueError):
            return False
        if map_id != 638:
            return False
        loading = map_state.get("loading_state")
        if loading not in (None, 1):
            return False
        me = snapshot.get("me", {}) or {}
        if me.get("is_moving") or me.get("is_casting"):
            return False
        party = snapshot.get("party", {}) or {}
        return not bool(party.get("is_defeated"))

    @staticmethod
    def _snapshot_implies_safe_route_override(snapshot: dict, bot: dict) -> bool:
        state = bot.get("state")
        if state not in (None, "idle", "in_town", "traveling", "in_dungeon"):
            return False
        map_state = snapshot.get("map", {}) or {}
        try:
            map_id = int(map_state.get("map_id") or 0)
        except (TypeError, ValueError):
            return False
        if map_id not in {558, 615, 616}:
            return False
        loading = map_state.get("loading_state")
        if loading not in (None, 1):
            return False
        me = snapshot.get("me", {}) or {}
        if me.get("is_moving") or me.get("is_casting"):
            return False
        party = snapshot.get("party", {}) or {}
        return not bool(party.get("is_defeated"))

    @staticmethod
    def _action_result_timeout(action_name: str, role: str) -> float:
        if action_name == ADVISORY_CONTROL_ACTION:
            return CONTROL_ACTION_RESULT_TIMEOUT_SECONDS
        if role == "planner":
            return PLANNER_ACTION_RESULT_TIMEOUT_SECONDS
        return ACTION_RESULT_TIMEOUT_SECONDS

    async def _emit_result(self, request_id: str, result: dict) -> None:
        await self.emit("tool.result", {
            "request_id": result.get("request_id", request_id),
            "success": bool(result.get("success", False)),
            "error": result.get("error"),
        })
        if self.on_tool_result is None:
            return
        try:
            await self.on_tool_result(result)
        except Exception as exc:
            await self.emit("degradation", {
                "reason": "tool_result_callback_error",
                "action": result.get("action"),
                "error": str(exc),
            })

    @staticmethod
    def _append_history(
        history: list[dict] | None,
        tool_call: ToolCall,
        result: dict,
    ) -> None:
        if history is None:
            return
        history.append({
            "role": "tool",
            "tool_call_id": tool_call.id,
            "content": json.dumps(result),
        })
