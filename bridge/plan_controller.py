"""Re-plan trigger policy for the planner/executor bridge split."""

from __future__ import annotations

import time
from dataclasses import dataclass

from .plan import Plan


HARD_EVENTS = {
    "party_defeated",
    "inventory_full",
    "map_changed",
    "instance_load",
    "dialog_opened",
    "merchant_opened",
    "maintenance_complete",
    "maintenance_completed",
    "dungeon_complete",
    "dungeon_completed",
    "reward_complete",
    "reward_claimed",
    "return_to_outpost",
    "wipe_recovery",
    "trade_request_received",
}

BYPASS_DEBOUNCE_REASONS = {
    "map_changed",
    "instance_load",
    "dungeon_complete",
    "dungeon_completed",
    "reward_complete",
    "reward_claimed",
    "return_to_outpost",
    "wipe_recovery",
}


@dataclass
class ReplanDecision:
    should_replan: bool
    reason: str = ""
    suppression_reason: str = ""


class PlanController:
    """Debounced event/snapshot watcher for planner wake-ups."""

    def __init__(self, min_interval_seconds: float = 5.0, policy: str = "balanced"):
        self.min_interval_seconds = min_interval_seconds
        self.policy = policy
        self._last_replan_at = 0.0
        self._stalled_ticks = 0
        self._last_map_id: int | None = None
        self._last_signature: str | None = None
        self.last_suppression_reason = ""

    def observe_executor_tick(self, counts_as_stall: bool) -> None:
        self._stalled_ticks = self._stalled_ticks + 1 if counts_as_stall else 0

    def evaluate(
        self,
        plan: Plan,
        snapshot: dict | None,
        events: list[dict],
    ) -> ReplanDecision:
        now = time.time()
        reason = self._reason(plan, snapshot or {}, events)
        if not reason:
            return ReplanDecision(False)
        suppression = self._suppression_reason(plan, snapshot or {}, reason, now)
        if suppression:
            self.last_suppression_reason = suppression
            return ReplanDecision(False, suppression_reason=suppression)

        self._last_replan_at = now
        self._last_signature = self._signature(plan, snapshot or {}, reason)
        self.last_suppression_reason = ""
        self._stalled_ticks = 0
        return ReplanDecision(True, reason)

    def _suppression_reason(self, plan: Plan, snapshot: dict, reason: str, now: float) -> str:
        if self.policy == "aggressive":
            return ""
        if reason not in BYPASS_DEBOUNCE_REASONS and now - self._last_replan_at < self.min_interval_seconds:
            return f"debounce:{reason}"
        if self.policy == "strict" and reason not in HARD_EVENTS:
            signature = self._signature(plan, snapshot, reason)
            if signature == self._last_signature:
                return f"duplicate:{reason}"
        return ""

    def _signature(self, plan: Plan, snapshot: dict, reason: str) -> str:
        return "|".join((
            str(self._snapshot_map_id(snapshot) or 0),
            str(plan.phase),
            str(plan.next_step),
            reason or "",
        ))

    def _reason(self, plan: Plan, snapshot: dict, events: list[dict]) -> str:
        for event in events:
            name = str(event.get("event") or event.get("type") or "")
            if name in HARD_EVENTS:
                return name

        map_id = self._snapshot_map_id(snapshot)
        if map_id is not None:
            if self._last_map_id is not None and map_id != self._last_map_id:
                self._last_map_id = map_id
                return "map_changed"
            self._last_map_id = map_id

        if plan.expired:
            return "plan_expired"

        party = snapshot.get("party") or {}
        if party.get("is_defeated"):
            return "party_defeated"

        inventory = snapshot.get("inventory") or {}
        if inventory.get("free_slots_total") == 0:
            return "inventory_full"

        abort_reason = self._abort_reason(plan, snapshot)
        if abort_reason:
            return abort_reason

        if self._stalled_ticks >= 5:
            return "executor_stalled"

        return ""

    @staticmethod
    def _snapshot_map_id(snapshot: dict) -> int | None:
        try:
            value = (snapshot.get("map") or {}).get("map_id")
            if value is None:
                return None
            return int(value)
        except (TypeError, ValueError):
            return None

    @staticmethod
    def _abort_reason(plan: Plan, snapshot: dict) -> str:
        for condition in plan.abort_conditions:
            if not isinstance(condition, dict):
                continue
            kind = str(condition.get("kind", ""))
            if kind == "party_defeated" and (snapshot.get("party") or {}).get("is_defeated"):
                return "abort:party_defeated"
            if kind == "inventory_full" and (snapshot.get("inventory") or {}).get("free_slots_total") == 0:
                return "abort:inventory_full"
            if kind == "off_route_distance":
                route = snapshot.get("route") or {}
                distance = route.get("deviation_distance") or route.get("off_route_distance")
                try:
                    if float(distance) > float(condition.get("units", 2500)):
                        return "abort:off_route_distance"
                except (TypeError, ValueError):
                    pass
        return ""
