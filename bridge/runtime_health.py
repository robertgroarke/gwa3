"""Bridge runtime health helpers."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class MapLoadHealthResult:
    should_continue: bool
    should_warn: bool = False
    elapsed_seconds: float = 0.0


class MapLoadHealthMonitor:
    """Track how long snapshots report a non-loaded map state."""

    def __init__(
        self,
        *,
        warning_seconds: float,
        stop_seconds: float,
    ) -> None:
        self.warning_seconds = warning_seconds
        self.stop_seconds = stop_seconds
        self.not_loaded_since = 0.0
        self.warned = False

    def check(self, loading_state: int | None, now: float) -> MapLoadHealthResult:
        if loading_state == 1:
            self.not_loaded_since = 0.0
            self.warned = False
            return MapLoadHealthResult(should_continue=True)

        if self.not_loaded_since <= 0.0:
            self.not_loaded_since = now
            self.warned = False
            return MapLoadHealthResult(should_continue=True)

        elapsed = now - self.not_loaded_since
        should_warn = elapsed >= self.warning_seconds and not self.warned
        if should_warn:
            self.warned = True
        return MapLoadHealthResult(
            should_continue=elapsed < self.stop_seconds,
            should_warn=should_warn,
            elapsed_seconds=elapsed,
        )


def connection_disconnect_reason(snapshot: dict | None) -> str | None:
    snap = snapshot or {}
    connection = snap.get("connection", {}) or {}
    if connection.get("disconnected"):
        reason = connection.get("reason") or connection.get("state") or "unknown"
        return f"gw_disconnected:{reason}"

    # Backward-compatible fallback for older injected DLLs that only expose
    # MapMgr's loading state.
    map_state = snap.get("map", {}) or {}
    if map_state.get("loading_state") == 2:
        return "gw_disconnected:loading_state_disconnected"
    return None
