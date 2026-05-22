"""Deterministic Froggy HM supervisor for low-LLM launch profiles."""

from __future__ import annotations

import json
import time
from dataclasses import dataclass, field
from typing import Any

from .llm_client import ToolCall
from .plan import Plan


GADDS_MAP_ID = 638
SPARKFLY_MAP_ID = 558
BOGROOT_MAP_IDS = {615, 616}
TEKKS_STAGE_X = 12061.0
TEKKS_STAGE_Y = 22485.0
TEKKS_SEARCH_X = 12396.0
TEKKS_SEARCH_Y = 22407.0
TEKKS_READY_DISTANCE = 1200.0
MINIMUM_FREE_SLOTS = 5
ACTIVE_FROGGY_STATES = {
    "traveling",
    "in_dungeon",
    "combat",
    "returning",
    "maintenance",
    "merchant",
}

SUPERVISOR_TOOL_NAMES = {
    "froggy_run_full_maintenance",
    "froggy_run_town_setup",
    "froggy_travel_to_sparkfly",
    "froggy_run_sparkfly_route_to_tekks",
    "froggy_prepare_tekks_dungeon_entry",
    "froggy_run_dungeon_loop",
    "froggy_travel_to_gadds",
    "return_to_outpost",
    "wait",
    "query_state",
}


@dataclass(frozen=True)
class SupervisorDecision:
    action: str
    args: dict[str, Any] = field(default_factory=dict)
    reason: str = ""
    phase: str = "idle"
    phase_kind: str = "route"
    intent: str = ""
    next_step: str = ""
    needs_planner: bool = False
    control_required: bool = True

    @property
    def signature(self) -> str:
        return f"{self.phase}:{self.action}:{self.reason}"

    def to_plan(self) -> Plan:
        return Plan(
            phase=self.phase,
            phase_kind=self.phase_kind,
            intent=self.intent or self.reason or f"Supervisor selected {self.action}.",
            next_step=self.next_step or f"supervisor:{self.action}",
            constraints=[
                "configured_lane_only",
                "froggy_native_route_control",
                "executor_must_not_freelance",
            ],
            abort_conditions=[
                {"kind": "party_defeated"},
                {"kind": "inventory_full"},
            ],
            fallback="handoff_to_froggy",
            trace_id=f"supervisor-{int(time.time())}-{self.phase}-{self.action}",
        )

    def to_tool_call(self) -> ToolCall:
        return ToolCall(
            id=f"supervisor-{self.action}",
            name=self.action,
            arguments=json.dumps(self.args, separators=(",", ":")),
        )


class FroggySupervisor:
    """Pure deterministic phase chooser with duplicate suppression state."""

    def __init__(self, min_repeat_seconds: float = 3.0):
        self.min_repeat_seconds = min_repeat_seconds
        self._last_action_at: dict[str, float] = {}
        self._last_success_at: dict[str, float] = {}
        self._last_result: dict[str, Any] | None = None

    def observe_tool_result(self, result: dict[str, Any]) -> None:
        action = str(result.get("action") or "")
        if not action:
            return
        self._last_result = result
        if result.get("success"):
            self._last_success_at[action] = time.monotonic()

    def should_execute(self, decision: SupervisorDecision) -> bool:
        if decision.action in {"wait", "query_state"}:
            return True
        last_at = self._last_action_at.get(decision.signature, 0.0)
        if time.monotonic() - last_at < self.min_repeat_seconds:
            return False
        self._last_action_at[decision.signature] = time.monotonic()
        return True

    def decide(self, snapshot: dict | None, events: list[dict] | None = None) -> SupervisorDecision | None:
        if not snapshot:
            return SupervisorDecision(
                action="query_state",
                args={"wait_ms": 250},
                reason="missing_snapshot",
                phase="await_snapshot",
                intent="Request a fresh snapshot before choosing a route phase.",
                next_step="query_state",
                control_required=False,
            )
        map_state = snapshot.get("map") or {}
        loading_state = map_state.get("loading_state")
        if loading_state not in (None, 1):
            return SupervisorDecision(
                action="wait",
                args={"milliseconds": 500},
                reason="map_loading",
                phase="loading",
                intent="Wait for the map transition to settle.",
                next_step="wait",
                control_required=False,
            )

        if self._party_defeated(snapshot, events):
            return SupervisorDecision(
                action="return_to_outpost",
                reason="wipe_recovery",
                phase="wipe_recovery",
                intent="Recover from a wipe by returning to an outpost.",
                next_step="supervisor:return_to_outpost",
            )

        active_state = self._active_froggy_state(snapshot)
        if active_state is not None:
            return SupervisorDecision(
                action="wait",
                args={"milliseconds": 500},
                reason=f"native_{active_state}_in_progress",
                phase=active_state,
                intent="Observe while the native Froggy route controller is active.",
                next_step="wait_for_froggy_native_route",
                control_required=False,
            )

        map_id = self._map_id(snapshot)
        free_slots = self._free_slots(snapshot)
        if map_id == GADDS_MAP_ID:
            if free_slots is not None and free_slots < MINIMUM_FREE_SLOTS:
                return SupervisorDecision(
                    action="froggy_run_full_maintenance",
                    reason="inventory_low_slots",
                    phase="maintenance",
                    phase_kind="merchant",
                    intent="Run Froggy maintenance before another Bogroot cycle.",
                    next_step="supervisor:froggy_run_full_maintenance",
                )
            if self._recent_success("froggy_run_town_setup") and not self._recent_success(
                "froggy_travel_to_sparkfly",
                newer_than="froggy_run_town_setup",
            ):
                return SupervisorDecision(
                    action="froggy_travel_to_sparkfly",
                    reason="town_setup_complete",
                    phase="travel_to_sparkfly",
                    phase_kind="travel",
                    intent="Travel from Gadd's Encampment to Sparkfly Swamp.",
                    next_step="supervisor:froggy_travel_to_sparkfly",
                )
            return SupervisorDecision(
                action="froggy_run_town_setup",
                reason="gadds_ready",
                phase="town_setup",
                intent="Prepare hard mode, heroes, and route state at Gadd's Encampment.",
                next_step="supervisor:froggy_run_town_setup",
            )

        if map_id == SPARKFLY_MAP_ID:
            if self._near_tekks(snapshot) or self._recent_success("froggy_run_sparkfly_route_to_tekks"):
                return SupervisorDecision(
                    action="froggy_prepare_tekks_dungeon_entry",
                    reason="tekks_entry_ready",
                    phase="tekks_entry",
                    phase_kind="dialog",
                    intent="Use Tekks entry handling to enter Bogroot Growths.",
                    next_step="supervisor:froggy_prepare_tekks_dungeon_entry",
                )
            return SupervisorDecision(
                action="froggy_run_sparkfly_route_to_tekks",
                reason="sparkfly_route",
                phase="route_to_tekks",
                phase_kind="long_walk",
                intent="Run the native Sparkfly route toward Tekks.",
                next_step="supervisor:froggy_run_sparkfly_route_to_tekks",
            )

        if map_id in BOGROOT_MAP_IDS:
            return SupervisorDecision(
                action="froggy_run_dungeon_loop",
                reason="bogroot_dungeon",
                phase="dungeon_loop",
                phase_kind="boss" if map_id == 616 else "long_walk",
                intent="Let Froggy native dungeon loop own routing, boss, reward, and return.",
                next_step="supervisor:froggy_run_dungeon_loop",
            )

        if map_id > 0:
            return SupervisorDecision(
                action="froggy_travel_to_gadds",
                reason=f"unexpected_map:{map_id}",
                phase="recover_to_gadds",
                phase_kind="travel",
                intent="Recover to Gadd's Encampment before resuming the Bogroot cycle.",
                next_step="supervisor:froggy_travel_to_gadds",
                needs_planner=True,
            )
        return SupervisorDecision(
            action="query_state",
            args={"wait_ms": 250},
            reason="unknown_map",
            phase="unknown",
            intent="Ask for a fresh snapshot because the map id is unknown.",
            next_step="query_state",
            needs_planner=True,
            control_required=False,
        )

    @staticmethod
    def _map_id(snapshot: dict) -> int:
        try:
            return int((snapshot.get("map") or {}).get("map_id") or 0)
        except (TypeError, ValueError):
            return 0

    @staticmethod
    def _free_slots(snapshot: dict) -> int | None:
        try:
            return int((snapshot.get("inventory") or {}).get("free_slots_total"))
        except (TypeError, ValueError):
            return None

    @staticmethod
    def _party_defeated(snapshot: dict, events: list[dict] | None) -> bool:
        if (snapshot.get("party") or {}).get("is_defeated"):
            return True
        for event in events or []:
            name = str(event.get("event") or event.get("type") or "")
            if name == "party_defeated":
                return True
        return False

    def _active_froggy_state(self, snapshot: dict) -> str | None:
        bot = snapshot.get("bot") or {}
        state = str(bot.get("state") or "").strip()
        if not state or state in {"idle", "in_town", "llm_controlled"}:
            return None
        if self._pending_sparkfly_handoff(snapshot, state):
            return None
        if bot.get("busy"):
            return state
        if state in ACTIVE_FROGGY_STATES and self._player_busy(snapshot):
            return state
        if bot.get("safe_for_llm_game_action", True):
            return None
        phase = str(bot.get("phase") or "").strip()
        if phase in ACTIVE_FROGGY_STATES and self._player_busy(snapshot):
            return phase
        return None

    def _pending_sparkfly_handoff(self, snapshot: dict, state: str) -> bool:
        if state != "traveling":
            return False
        if self._map_id(snapshot) != GADDS_MAP_ID:
            return False
        if not self._recent_success("froggy_run_town_setup"):
            return False
        if self._recent_success("froggy_travel_to_sparkfly", newer_than="froggy_run_town_setup"):
            return False

        route = snapshot.get("route") or {}
        next_step = route.get("next_step") or {}
        if route.get("progress") != "preparing":
            return False
        if route.get("phase") not in (None, "town"):
            return False
        if next_step.get("kind") != "travel":
            return False
        try:
            if int(next_step.get("map_id") or 0) != SPARKFLY_MAP_ID:
                return False
        except (TypeError, ValueError):
            return False

        me = snapshot.get("me") or {}
        if me.get("is_moving") or me.get("is_casting"):
            return False
        party = snapshot.get("party") or {}
        return not bool(party.get("is_defeated"))

    @staticmethod
    def _player_busy(snapshot: dict) -> bool:
        me = snapshot.get("me") or {}
        return bool(me.get("is_moving") or me.get("is_casting"))

    @staticmethod
    def _near_tekks(snapshot: dict) -> bool:
        me = snapshot.get("me") or {}
        try:
            x = float(me.get("x"))
            y = float(me.get("y"))
        except (TypeError, ValueError):
            return False
        stage = ((x - TEKKS_STAGE_X) ** 2 + (y - TEKKS_STAGE_Y) ** 2) ** 0.5
        search = ((x - TEKKS_SEARCH_X) ** 2 + (y - TEKKS_SEARCH_Y) ** 2) ** 0.5
        return min(stage, search) <= TEKKS_READY_DISTANCE

    def _recent_success(self, action: str, newer_than: str | None = None, seconds: float = 900.0) -> bool:
        timestamp = self._last_success_at.get(action)
        if timestamp is None:
            return False
        if time.monotonic() - timestamp > seconds:
            return False
        if newer_than is not None:
            return timestamp > self._last_success_at.get(newer_than, 0.0)
        return True
