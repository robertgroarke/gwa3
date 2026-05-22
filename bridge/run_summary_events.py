"""Run-summary event builders shared by bridge loops."""

from __future__ import annotations

from . import farming_knowledge
from .route_spec import route_map_ids, route_recovery_map_ids


FROGGY_SUMMARY_STATS_FIELDS = (
    ("runs", "run_count"),
    ("fails", "fail_count"),
    ("wipes", "monitoring_wipes"),
    ("route_wipes", "route_wipe_count"),
    ("chests_opened", "chests_opened"),
    ("gold_items", "gold_items"),
    ("rare_skins", "rare_skins"),
    ("tomes", "tomes"),
)


def _froggy_maintenance_completion_map_ids() -> set[int]:
    route_maps = route_map_ids()
    recovery_maps = route_recovery_map_ids()
    return {
        map_id for map_id in (
            route_maps.get("gadds"),
            recovery_maps.get("embark"),
        )
        if isinstance(map_id, int)
    }


def build_froggy_summary_stats(monitoring: dict | None) -> dict:
    """Return the stable run-summary stats shape used by bridge prompts."""
    monitoring = monitoring or {}
    return {
        summary_key: monitoring.get(snapshot_key)
        for summary_key, snapshot_key in FROGGY_SUMMARY_STATS_FIELDS
    }


class SnapshotRunSummaryTracker:
    """Stateful snapshot-derived run-summary event builder."""

    def __init__(self) -> None:
        self.summary_keys: set[str] = set()
        self.last_bot_phase: str | None = None

    def build_event(self, snapshot: dict) -> dict | None:
        map_state = snapshot.get("map", {}) or {}
        inventory_state = snapshot.get("inventory", {}) or {}
        bot_state = snapshot.get("bot", {}) or {}
        party_state = snapshot.get("party", {}) or {}
        monitoring = bot_state.get("froggy_monitoring", {}) or {}
        loop = bot_state.get("froggy_dungeon_loop", {}) or {}

        map_id = int(map_state.get("map_id") or loop.get("final_map_id") or 0)
        map_name = map_state.get("name") or farming_knowledge.MAP_NAMES.get(map_id, "unknown")
        current_phase = str(bot_state.get("phase") or bot_state.get("state") or "")
        previous_phase = self.last_bot_phase
        self.last_bot_phase = current_phase or None

        def common_event(event_type: str, key: str) -> dict | None:
            if key in self.summary_keys:
                return None
            self.summary_keys.add(key)
            event: dict[str, object] = {
                "event_type": event_type,
                "tool": "snapshot",
                "map_id": map_id,
                "map_name": map_name,
                "reason": f"snapshot:{event_type}",
                "free_slots_total": inventory_state.get("free_slots_total"),
                "gold_character": inventory_state.get("gold_character"),
                "stats": build_froggy_summary_stats(monitoring),
            }
            if isinstance(loop, dict) and loop:
                event["last_outcome"] = {
                    "entered_lvl2": loop.get("entered_lvl2"),
                    "boss_started": loop.get("boss_started"),
                    "boss_completed": loop.get("boss_completed"),
                    "chest_attempts": loop.get("chest_attempts"),
                    "chest_successes": loop.get("chest_successes"),
                    "returned_to_sparkfly": loop.get("returned_to_sparkfly"),
                    "final_map_id": loop.get("final_map_id"),
                    "last_waypoint_label": loop.get("last_waypoint_label"),
                    "waypoint_iterations": loop.get("waypoint_iterations"),
                    "reward_dialog_latched": loop.get("reward_dialog_latched"),
                }
            return event

        boss_completed = bool(loop.get("boss_completed"))
        chest_successes = int(loop.get("chest_successes") or 0)
        run_count = int(monitoring.get("run_count") or 0)
        chests_opened = int(monitoring.get("chests_opened") or 0)
        final_map_id = int(loop.get("final_map_id") or 0)
        if boss_completed and chest_successes > 0 and run_count > 0:
            if final_map_id and map_id != final_map_id:
                return None
            key = "|".join([
                "dungeon_run_completed",
                str(run_count),
                str(chests_opened),
                str(final_map_id or map_id),
            ])
            event = common_event("dungeon_run_completed", key)
            if event is not None:
                event["reward_claimed"] = bool(loop.get("reward_dialog_latched"))
                event["next_action"] = "continue_froggy_loop"
            return event

        merchant_phases = {"maintenance", "merchant", "quote"}
        if (
            previous_phase in merchant_phases
            and current_phase
            and current_phase not in merchant_phases
            and map_id in _froggy_maintenance_completion_map_ids()
            and not bool(party_state.get("is_defeated"))
        ):
            key = "|".join([
                "maintenance_completed",
                str(run_count),
                str(chests_opened),
                str(inventory_state.get("free_slots_total") or ""),
                current_phase,
            ])
            event = common_event("maintenance_completed", key)
            if event is not None:
                event["next_action"] = "continue_froggy_loop"
            return event

        return None


def build_run_summary_event(
    tool_name: str,
    params: dict | None,
    result: dict,
    snapshot: dict | None,
) -> dict | None:
    if not result.get("success"):
        return None

    snap = snapshot or {}
    map_state = snap.get("map", {}) or {}
    party_state = snap.get("party", {}) or {}
    inventory_state = snap.get("inventory", {}) or {}
    params = params or {}

    map_id = int(result.get("current_map_id") or map_state.get("map_id") or 0)
    event: dict[str, object] = {
        "tool": tool_name,
        "map_id": map_id,
        "map_name": result.get("current_map_name")
        or farming_knowledge.MAP_NAMES.get(map_id, "unknown"),
    }
    if "free_slots_total" in result or "free_slots_total" in inventory_state:
        event["free_slots_total"] = result.get(
            "free_slots_total", inventory_state.get("free_slots_total")
        )
    if "gold_character" in result or "gold_character" in inventory_state:
        event["gold_character"] = result.get(
            "gold_character", inventory_state.get("gold_character")
        )

    if result.get("completed_dungeon_run") or result.get("quest_reward_claimed"):
        event["event_type"] = "dungeon_run_completed"
        event["reward_claimed"] = bool(result.get("quest_reward_claimed"))
        event["return_to_outpost_expected"] = bool(
            result.get("return_to_outpost_was_expected")
        )
        event["next_action"] = result.get("recommended_next_action")
        event["reason"] = result.get("reason")
        return event

    if result.get("completed_full_maintenance") or tool_name in {
        "froggy_run_full_maintenance",
        "froggy_run_maintenance_cycle",
    }:
        event["event_type"] = "maintenance_completed"
        event["next_action"] = result.get("recommended_next_action", "query_state")
        event["reason"] = result.get("reason", "maintenance_complete")
        maintenance = result.get("froggy_maintenance") or inventory_state.get(
            "froggy_maintenance"
        )
        if isinstance(maintenance, dict):
            event["stored_consets_total"] = maintenance.get("stored_consets_total")
            event["loose_consets_inventory_total"] = maintenance.get(
                "loose_consets_inventory_total"
            )
        return event

    if tool_name == "interact_signpost":
        agent_id = params.get("agent_id")
        for agent in snap.get("agents", []) or []:
            if agent.get("id") == agent_id and agent.get("is_chest"):
                event["event_type"] = "chest_interaction_completed"
                event["agent_id"] = agent_id
                return event

    party_defeated = bool(result.get("party_defeated") or party_state.get("is_defeated"))
    if tool_name == "return_to_outpost" and party_defeated:
        event["event_type"] = "party_defeated_returned_to_outpost"
        event["dead_count"] = result.get("dead_count", party_state.get("dead_count"))
        return event

    return None


def fallback_run_summary(event: dict) -> str:
    event_type = event.get("event_type")
    map_name = event.get("map_name", "unknown map")
    if event_type == "dungeon_run_completed":
        next_action = event.get("next_action", "refresh state")
        return (
            f"Dungeon run completed on {map_name}; reward was claimed and "
            f"the next safe step is {next_action}."
        )
    if event_type == "maintenance_completed":
        next_action = event.get("next_action", "refresh state")
        return (
            f"Town maintenance completed on {map_name}; refresh inventory "
            f"and continue with {next_action} if pressure is clear."
        )
    if event_type == "chest_interaction_completed":
        return f"Reward chest interaction completed on {map_name}; verify loot and proceed."
    if event_type == "party_defeated_returned_to_outpost":
        return (
            f"Party defeat was recovered by returning to outpost from {map_name}; "
            "restart only after setup is safe."
        )
    return f"Run event completed on {map_name}; refresh state before continuing."
