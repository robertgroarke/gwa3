"""Persistent run-summary memory for the bridge agent loop."""

from __future__ import annotations

from collections import deque
import json
import os
from pathlib import Path
import re
from typing import Any


DEFAULT_RUN_SUMMARY_PATH = Path(__file__).resolve().parent / "runtime" / "run_summaries.jsonl"


class RunSummaryMemory:
    """Store and render the last N run summaries.

    The backing file is a runtime artifact and is intentionally ignored by git.
    Summaries are short operational notes, not raw snapshots.
    """

    def __init__(self, path: str | os.PathLike[str] | None = None, max_entries: int = 20):
        env_path = os.environ.get("GWA3_RUN_SUMMARY_PATH")
        self.path = Path(path or env_path or DEFAULT_RUN_SUMMARY_PATH)
        self.max_entries = max_entries
        self._entries: deque[dict[str, Any]] = deque(maxlen=max_entries)
        self._load()

    @property
    def entries(self) -> list[dict[str, Any]]:
        return list(self._entries)

    def add(self, summary: str, event: dict[str, Any] | None = None) -> dict[str, Any] | None:
        text = self._normalize(summary)
        if not text:
            return None
        entry = {
            "summary": text,
            "event_type": (event or {}).get("event_type", "run_event"),
        }
        self._entries.append(entry)
        self._persist()
        return entry

    def format_for_prompt(self) -> str:
        if not self._entries:
            return ""
        lines = [
            "[RUN SUMMARY MEMORY]",
            "Use these recent run summaries as durable context. They are compact; current snapshots still win.",
        ]
        for idx, entry in enumerate(self._entries, start=1):
            lines.append(f"{idx}. {entry['summary']}")
        return "\n".join(lines)

    @staticmethod
    def _normalize(summary: str) -> str:
        text = re.sub(r"\s+", " ", str(summary or "")).strip()
        if len(text) > 700:
            text = text[:697].rstrip() + "..."
        return text

    def _load(self) -> None:
        try:
            if not self.path.exists():
                return
            entries: list[dict[str, Any]] = []
            for line in self.path.read_text(encoding="utf-8").splitlines():
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(data, dict) and isinstance(data.get("summary"), str):
                    entries.append({
                        "summary": self._normalize(data["summary"]),
                        "event_type": data.get("event_type", "run_event"),
                    })
            for entry in entries[-self.max_entries:]:
                self._entries.append(entry)
        except OSError:
            self._entries.clear()

    def _persist(self) -> None:
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            content = "\n".join(
                json.dumps(entry, separators=(",", ":")) for entry in self._entries
            )
            if content:
                content += "\n"
            tmp_path = self.path.with_suffix(self.path.suffix + ".tmp")
            tmp_path.write_text(content, encoding="utf-8")
            tmp_path.replace(self.path)
        except OSError:
            pass
