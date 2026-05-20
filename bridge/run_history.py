"""Persistent run-summary history for planner context."""

from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any


class RunHistoryStore:
    def __init__(self, path: str | Path | None = None, max_items: int = 20):
        self.path = Path(path) if path is not None else Path(__file__).with_name("benchmarks") / "run_summaries.json"
        self.max_items = max_items
        self._items = self._load()

    def _load(self) -> list[dict[str, Any]]:
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return []
        if not isinstance(data, list):
            return []
        return self._dedupe(
            [item for item in data if isinstance(item, dict)]
        )[-self.max_items:]

    def list(self) -> list[dict[str, Any]]:
        return list(self._items)

    def append(self, summary: dict[str, Any]) -> dict[str, Any]:
        item = {"captured_at": time.time(), **summary}
        self._items.append(item)
        self._items = self._dedupe(self._items)[-self.max_items:]
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(self._items, indent=2), encoding="utf-8")
        return item

    @classmethod
    def _dedupe(cls, items: list[dict[str, Any]]) -> list[dict[str, Any]]:
        compact: list[dict[str, Any]] = []
        index_by_key: dict[str, int] = {}
        for item in items:
            key = cls._summary_key(item)
            if key is None:
                compact.append(item)
                continue
            prior = index_by_key.get(key)
            if prior is None:
                index_by_key[key] = len(compact)
                compact.append(item)
            else:
                compact[prior] = item
        return compact

    @staticmethod
    def _summary_key(item: dict[str, Any]) -> str | None:
        last = item.get("last_outcome")
        if not isinstance(last, dict):
            return None
        waypoint_iterations = last.get("waypoint_iterations")
        if waypoint_iterations is None:
            return None
        reason = str(item.get("reason", ""))
        return "|".join([
            reason,
            str(item.get("route_phase") or ""),
            str(last.get("entered_lvl2", "")),
            str(waypoint_iterations),
            str(last.get("step", "")),
        ])
