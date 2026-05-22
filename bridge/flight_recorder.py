"""Durable compact telemetry recorder for unattended LLM bridge runs."""

from __future__ import annotations

import json
import time
from collections import deque
from pathlib import Path
from typing import Any


RECORDED_EVENTS = frozenset({
    "bridge.status",
    "degradation",
    "disconnect_detected",
    "llm.telemetry",
    "run.summary",
    "snapshot.summary",
    "tool.call",
    "tool.result",
})

COMPACT_KEYS = frozenset({
    "action",
    "autonomy",
    "bot",
    "bot_phase",
    "bot_state",
    "bridge",
    "code",
    "connection",
    "connection_disconnected",
    "connection_reason",
    "connection_state",
    "disconnect",
    "error",
    "event_type",
    "heartbeat_age_s",
    "heartbeat_age_seconds",
    "lane",
    "loading_state",
    "map",
    "map_id",
    "model",
    "model_latency_ms",
    "name",
    "prompt_bytes",
    "reason",
    "request_id",
    "route",
    "route_last_outcome",
    "route_next_step",
    "route_progress",
    "screenshot_path",
    "stats",
    "status",
    "success",
    "tool",
    "tool_calls",
})


def compact_payload(payload: dict[str, Any]) -> dict[str, Any]:
    compact: dict[str, Any] = {}
    for key, value in payload.items():
        if key in COMPACT_KEYS:
            compact[key] = value
    return compact


class FlightRecorder:
    """Append-only JSONL recorder plus a small in-memory timeline."""

    def __init__(
        self,
        directory: str | Path,
        *,
        lane: str,
        max_recent_events: int = 100,
        clock=time.time,
    ) -> None:
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        stamp = time.strftime("%Y%m%d_%H%M%S", time.localtime(clock()))
        safe_lane = "".join(ch for ch in lane.lower() if ch.isalnum() or ch in {"_", "-"})
        self.path = self.directory / f"{safe_lane or 'bridge'}_flight_{stamp}.jsonl"
        self.latest_path = self.directory / f"{safe_lane or 'bridge'}_flight_latest.json"
        self._recent: deque[dict[str, Any]] = deque(maxlen=max_recent_events)
        self._clock = clock
        self._summary: dict[str, Any] = {
            "lane": lane,
            "events": 0,
            "tool_calls": 0,
            "tool_errors": 0,
            "disconnects": 0,
            "crashes": 0,
            "run_summaries": 0,
            "last_outcome": None,
            "last_screenshot_path": None,
        }

    def record(self, event: str, payload: dict[str, Any]) -> dict[str, Any] | None:
        if event not in RECORDED_EVENTS:
            return None
        entry = {
            "ts": round(float(self._clock()), 3),
            "event": event,
            "payload": compact_payload(payload),
        }
        self._recent.append(entry)
        self._update_summary(event, entry["payload"])
        with self.path.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(entry, separators=(",", ":"), default=str) + "\n")
        self.latest_path.write_text(
            json.dumps(self.snapshot(), indent=2, default=str),
            encoding="utf-8",
        )
        return entry

    def snapshot(self) -> dict[str, Any]:
        return {
            "path": str(self.path),
            "latest_path": str(self.latest_path),
            "summary": dict(self._summary),
            "recent": list(self._recent),
        }

    def _update_summary(self, event: str, payload: dict[str, Any]) -> None:
        self._summary["events"] = int(self._summary.get("events") or 0) + 1
        if event == "tool.call":
            self._summary["tool_calls"] = int(self._summary.get("tool_calls") or 0) + 1
        if event == "tool.result" and not bool(payload.get("success", False)):
            self._summary["tool_errors"] = int(self._summary.get("tool_errors") or 0) + 1
        if event == "disconnect_detected":
            self._summary["disconnects"] = int(self._summary.get("disconnects") or 0) + 1
        if event == "run.summary":
            self._summary["run_summaries"] = int(self._summary.get("run_summaries") or 0) + 1
            self._summary["last_outcome"] = payload.get("event_type") or payload.get("reason")
        reason = str(payload.get("reason") or payload.get("error") or "")
        if "crash" in reason.lower() or payload.get("screenshot_path"):
            self._summary["crashes"] = int(self._summary.get("crashes") or 0) + 1
        screenshot = payload.get("screenshot_path")
        if screenshot:
            self._summary["last_screenshot_path"] = screenshot
