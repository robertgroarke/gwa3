"""Snapshot event payload builders shared by bridge loops."""

from __future__ import annotations

from typing import Any


def _dict_field(payload: dict[str, Any], key: str) -> dict[str, Any]:
    value = payload.get(key)
    return value if isinstance(value, dict) else {}


def build_snapshot_summary(snapshot: dict[str, Any]) -> dict[str, Any]:
    """Build the HTTP-facing snapshot summary from a raw gwa3 snapshot frame."""
    me = _dict_field(snapshot, "me")
    party = _dict_field(snapshot, "party")
    inventory = _dict_field(snapshot, "inventory")
    bot = _dict_field(snapshot, "bot")
    route = _dict_field(snapshot, "route")
    map_state = _dict_field(snapshot, "map")
    connection = _dict_field(snapshot, "connection")
    bot_phase = bot.get("phase") or bot.get("state")

    return {
        "tier": snapshot.get("tier"),
        "map": map_state.get("map_id"),
        "map_name": map_state.get("name"),
        "loading_state": map_state.get("loading_state"),
        "instance_time": map_state.get("instance_time"),
        "hp": me.get("hp"),
        "energy": me.get("energy"),
        "position": {"x": me.get("x"), "y": me.get("y")},
        "party": {
            "size": party.get("size"),
            "dead": party.get("dead_count"),
            "defeated": party.get("is_defeated"),
        },
        "free_slots": inventory.get("free_slots_total"),
        "current_phase": bot_phase,
        "bot": {
            "state": bot.get("state"),
            "phase": bot_phase,
            "busy": bot.get("busy"),
            "safe_to_enter_llm_control": bot.get("safe_to_enter_llm_control"),
            "safe_for_llm_game_action": bot.get("safe_for_llm_game_action"),
            "unsafe_reason": bot.get("unsafe_reason"),
        },
        "route": {
            "next_step": route.get("next_step"),
            "progress": route.get("progress"),
            "deviation": route.get("deviation"),
            "last_outcome": route.get("last_outcome"),
        },
        "connection": {
            "state": connection.get("state"),
            "disconnected": bool(connection.get("disconnected", False)),
            "reason": connection.get("reason"),
            "likely_disconnect_code": connection.get("likely_disconnect_code"),
        },
    }


def build_live_telemetry_payload(
    *,
    reason: str,
    model: str | None,
    autonomy: str,
    prompt_bytes: int,
    model_latency_ms: int,
    heartbeat_age_seconds: float | None,
    outbound_snapshots_dropped: Any,
    snapshot: dict[str, Any] | None,
    stop_error: str | None,
    extra: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Build the live telemetry payload emitted by the single-model loop."""
    snapshot = snapshot or {}
    bot = _dict_field(snapshot, "bot")
    route = _dict_field(snapshot, "route")
    connection = _dict_field(snapshot, "connection")
    payload = {
        "reason": reason,
        "model": model,
        "autonomy": autonomy,
        "prompt_bytes": prompt_bytes,
        "model_latency_ms": model_latency_ms,
        "heartbeat_age_s": heartbeat_age_seconds,
        "outbound_snapshots_dropped": outbound_snapshots_dropped,
        "bot_state": bot.get("state"),
        "bot_phase": bot.get("phase") or bot.get("state"),
        "route_progress": route.get("progress"),
        "route_next_step": route.get("next_step"),
        "route_last_outcome": route.get("last_outcome"),
        "connection_state": connection.get("state"),
        "connection_disconnected": bool(connection.get("disconnected", False)),
        "stop_error": stop_error,
    }
    if extra:
        payload.update(extra)
    return payload
