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
    prompt_samples: int = 0
    prompt_bytes: int = 0
    prompt_section_bytes: Counter[str] = field(default_factory=Counter)
    last_prompt_sections: dict[str, int] = field(default_factory=dict)
    idle_ticks: int = 0
    stalled_ticks: int = 0
    idle_reasons: Counter[str] = field(default_factory=Counter)

    def record_call(self, latency: float, usage: dict[str, Any] | None = None) -> None:
        self.calls += 1
        self.latencies.append(latency)
        usage = usage or {}
        self.prompt_tokens += int(usage.get("prompt_tokens", 0) or 0)
        self.completion_tokens += int(usage.get("completion_tokens", 0) or 0)
        self.total_tokens += int(usage.get("total_tokens", 0) or 0)

    def record_prompt(self, prompt_bytes: int, sections: dict[str, int] | None = None) -> None:
        self.prompt_samples += 1
        self.prompt_bytes += max(0, int(prompt_bytes))
        sections = sections or {}
        self.last_prompt_sections = {key: max(0, int(value)) for key, value in sections.items()}
        self.prompt_section_bytes.update(self.last_prompt_sections)

    def record_idle(self, reason: str, *, counts_as_stall: bool = False) -> None:
        self.idle_ticks += 1
        if counts_as_stall:
            self.stalled_ticks += 1
        self.idle_reasons[reason or "unknown"] += 1

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
            "prompt_samples": self.prompt_samples,
            "prompt_bytes": self.prompt_bytes,
            "prompt_bytes_per_sample": self.prompt_bytes / self.prompt_samples if self.prompt_samples else 0.0,
            "estimated_prompt_tokens_per_sample": (self.prompt_bytes / 4.0) / self.prompt_samples if self.prompt_samples else 0.0,
            "prompt_section_bytes": dict(self.prompt_section_bytes),
            "last_prompt_sections": self.last_prompt_sections,
            "idle_ticks": self.idle_ticks,
            "stalled_ticks": self.stalled_ticks,
            "idle_reasons": dict(self.idle_reasons),
        }


class BridgeTelemetry:
    """Small in-memory telemetry store for benchmark JSON capture."""

    def __init__(self, profile: dict[str, Any] | None = None):
        self.started_at = time.time()
        self.profile = profile or {}
        self.roles: dict[str, RoleTelemetry] = defaultdict(RoleTelemetry)
        self.replan_reasons: Counter[str] = Counter()
        self.replan_suppressions: Counter[str] = Counter()
        self.degradations: Counter[str] = Counter()
        self.prompt_sections: dict[str, list[dict[str, Any]]] = defaultdict(list)
        self.executor_ticks: Counter[str] = Counter()
        self.ui: dict[str, dict[str, Any]] = {
            "desktop": {"chat_round_trip_ms": [], "sse_dropped_events": 0, "launch_to_connected_ms": None},
            "web": {"chat_round_trip_ms": [], "sse_dropped_events": 0, "launch_to_connected_ms": None},
        }
        self.ipc: dict[str, Any] = {
            "outbound_snapshots_dropped": 0,
        }
        self.runs: list[dict[str, Any]] = []

    def set_profile(self, profile: dict[str, Any]) -> None:
        self.profile = dict(profile)

    def record_llm_call(self, role: str, latency: float, usage: dict[str, Any] | None = None) -> None:
        self.roles[role].record_call(latency, usage)

    def record_prompt(self, role: str, messages: list[dict], sections: dict[str, int] | None = None) -> None:
        encoded = json.dumps(messages, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        self.roles[role].record_prompt(len(encoded), sections)

    def record_idle_tick(self, role: str, reason: str, *, counts_as_stall: bool = False) -> None:
        self.roles[role].record_idle(reason, counts_as_stall=counts_as_stall)

    def record_tool_result(self, role: str, success: bool) -> None:
        self.roles[role].tool_calls += 1
        if not success:
            self.roles[role].tool_errors += 1

    def record_replan(self, reason: str) -> None:
        self.replan_reasons[reason or "unknown"] += 1

    def record_replan_suppressed(self, reason: str) -> None:
        self.replan_suppressions[reason or "unknown"] += 1

    def record_degradation(self, reason: str) -> None:
        self.degradations[reason or "unknown"] += 1

    def record_run_summary(self, summary: dict[str, Any]) -> None:
        self.runs.append(summary)

    def record_heartbeat(self, heartbeat: dict[str, Any]) -> None:
        if "outbound_snapshots_dropped" in heartbeat:
            self.ipc["outbound_snapshots_dropped"] = int(
                heartbeat.get("outbound_snapshots_dropped") or 0
            )

    def record_prompt_sections(
        self,
        role: str,
        mode: str,
        context_hash: str,
        sections: dict[str, int],
    ) -> None:
        bucket = self.prompt_sections[role]
        normalized = {key: max(0, int(value)) for key, value in sections.items()}
        bucket.append({
            "mode": mode,
            "context_hash": context_hash,
            "sections": normalized,
            "total_bytes": sum(normalized.values()),
            "captured_at": time.time(),
        })
        if len(bucket) > 40:
            del bucket[:-40]

    def record_executor_tick(
        self,
        mode: str,
        outcome: str,
        *,
        llm_call: bool = False,
        useful: bool = False,
    ) -> None:
        self.executor_ticks[f"{mode}:{outcome or 'unknown'}"] += 1
        if llm_call:
            self.executor_ticks[f"{mode}:llm_calls"] += 1
        if useful:
            self.executor_ticks[f"{mode}:useful"] += 1

    def snapshot(self, phase: str, notes: str = "") -> dict[str, Any]:
        elapsed = max(0.001, time.time() - self.started_at)
        return {
            "phase": phase,
            "captured_at": time.time(),
            "elapsed_seconds": elapsed,
            "notes": notes,
            "profile": self.profile,
            "roles": {
                role: telemetry.snapshot(elapsed)
                for role, telemetry in sorted(self.roles.items())
            },
            "replan_reasons": dict(self.replan_reasons),
            "replan_suppressions": dict(self.replan_suppressions),
            "degradations": dict(self.degradations),
            "prompt_sections": {
                role: list(items)
                for role, items in sorted(self.prompt_sections.items())
            },
            "executor_ticks": dict(self.executor_ticks),
            "ipc": dict(self.ipc),
            "ui": self.ui,
            "runs": self.runs[-20:],
            "runs_per_hour": len(self.runs) * 3600.0 / elapsed if elapsed > 0 else 0.0,
        }

    def write_benchmark(self, path: str | Path, phase: str, notes: str = "") -> Path:
        output = Path(path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(self.snapshot(phase, notes), indent=2), encoding="utf-8")
        return output
