"""Telemetry aggregation and benchmark artifact helpers."""

from __future__ import annotations

import json
import statistics
import time
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


def _percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round((pct / 100.0) * (len(ordered) - 1)))))
    return ordered[index]


@dataclass
class RoleTelemetry:
    calls: int = 0
    prompt_tokens: int = 0
    completion_tokens: int = 0
    total_tokens: int = 0
    latencies: list[float] = field(default_factory=list)
    tool_calls: int = 0
    tool_errors: int = 0

    def record_call(self, latency: float, usage: dict[str, Any] | None = None) -> None:
        self.calls += 1
        self.latencies.append(latency)
        usage = usage or {}
        self.prompt_tokens += int(usage.get("prompt_tokens", 0) or 0)
        self.completion_tokens += int(usage.get("completion_tokens", 0) or 0)
        self.total_tokens += int(usage.get("total_tokens", 0) or 0)

    def snapshot(self, elapsed_seconds: float) -> dict[str, Any]:
        return {
            "calls": self.calls,
            "calls_per_min": self.calls * 60.0 / elapsed_seconds if elapsed_seconds > 0 else 0.0,
            "prompt_tokens": self.prompt_tokens,
            "completion_tokens": self.completion_tokens,
            "total_tokens": self.total_tokens,
            "tokens_per_call": self.total_tokens / self.calls if self.calls else 0.0,
            "mean_latency_seconds": statistics.fmean(self.latencies) if self.latencies else 0.0,
            "p95_latency_seconds": _percentile(self.latencies, 95),
            "tool_calls": self.tool_calls,
            "tool_errors": self.tool_errors,
        }


class BridgeTelemetry:
    """Small in-memory telemetry store for benchmark JSON capture."""

    def __init__(self):
        self.started_at = time.time()
        self.roles: dict[str, RoleTelemetry] = defaultdict(RoleTelemetry)
        self.replan_reasons: Counter[str] = Counter()
        self.degradations: Counter[str] = Counter()
        self.ui: dict[str, dict[str, Any]] = {
            "desktop": {"chat_round_trip_ms": [], "sse_dropped_events": 0, "launch_to_connected_ms": None},
            "web": {"chat_round_trip_ms": [], "sse_dropped_events": 0, "launch_to_connected_ms": None},
        }
        self.runs: list[dict[str, Any]] = []

    def record_llm_call(self, role: str, latency: float, usage: dict[str, Any] | None = None) -> None:
        self.roles[role].record_call(latency, usage)

    def record_tool_result(self, role: str, success: bool) -> None:
        self.roles[role].tool_calls += 1
        if not success:
            self.roles[role].tool_errors += 1

    def record_replan(self, reason: str) -> None:
        self.replan_reasons[reason or "unknown"] += 1

    def record_degradation(self, reason: str) -> None:
        self.degradations[reason or "unknown"] += 1

    def record_run_summary(self, summary: dict[str, Any]) -> None:
        self.runs.append(summary)

    def snapshot(self, phase: str, notes: str = "") -> dict[str, Any]:
        elapsed = max(0.001, time.time() - self.started_at)
        return {
            "phase": phase,
            "captured_at": time.time(),
            "elapsed_seconds": elapsed,
            "notes": notes,
            "roles": {
                role: telemetry.snapshot(elapsed)
                for role, telemetry in sorted(self.roles.items())
            },
            "replan_reasons": dict(self.replan_reasons),
            "degradations": dict(self.degradations),
            "ui": self.ui,
            "runs": self.runs[-20:],
            "runs_per_hour": len(self.runs) * 3600.0 / elapsed if elapsed > 0 else 0.0,
        }

    def write_benchmark(self, path: str | Path, phase: str, notes: str = "") -> Path:
        output = Path(path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(self.snapshot(phase, notes), indent=2), encoding="utf-8")
        return output
