"""Re-plan trigger policy for the planner/executor bridge split."""

from __future__ import annotations

import time
from dataclasses import dataclass

from .plan import Plan


HARD_EVENTS = {
    "party_defeated",
    "inventory_full",
    "map_changed",
    "dialog_opened",
    "merchant_opened",
    "trade_request_received",
}


@dataclass
class ReplanDecision:
    should_replan: bool
    reason: str = ""


class PlanController:
    """Debounced event/snapshot watcher for planner wake-ups."""

    def __init__(self, min_interval_seconds: float = 5.0):
        self.min_interval_seconds = min_interval_seconds
        self._last_replan_at = 0.0
        self._idle_ticks = 0

    def observe_executor_tick(self, had_tool_call: bool) -> None:
        self._idle_ticks = 0 if had_tool_call else self._idle_ticks + 1

    def evaluate(
        self,
        plan: Plan,
        snapshot: dict | None,
        events: list[dict],
    ) -> ReplanDecision:
        now = time.time()
        if now - self._last_replan_at < self.min_interval_seconds:
            return ReplanDecision(False)

        reason = self._reason(plan, snapshot or {}, events)
        if not reason:
            return ReplanDecision(False)

        self._last_replan_at = now
        self._idle_ticks = 0
        return ReplanDecision(True, reason)

    def _reason(self, plan: Plan, snapshot: dict, events: list[dict]) -> str:
        for event in events:
            name = str(event.get("event") or event.get("type") or "")
            if name in HARD_EVENTS:
                return name

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

        if self._idle_ticks >= 5:
            return "executor_stalled"

        return ""

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
