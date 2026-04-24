"""Repeated fresh-session soak runner for the isolated MARVIN bridge lane."""

from __future__ import annotations

import asyncio
import os
import time
from pathlib import Path
from typing import Any

from .marvin_harness import (
    MARVIN_LANE,
    _dll_name,
    _pipe_name,
    run_marvin_bridge_smoke,
)

BUILD_DIR = Path(os.environ.get("GWA3_BUILD_DIR", MARVIN_LANE.default_build_dir))
DLL_NAME = _dll_name(MARVIN_LANE)
PIPE_NAME = _pipe_name(MARVIN_LANE)


def _status_for_run(summary: dict[str, Any], run_events: list[str]) -> str:
    if not summary.get("ok"):
        return "FAIL"
    if not summary.get("action_success"):
        return "FAIL"
    if not summary.get("target_action_success"):
        return "FAIL"
    if run_events:
        return "WARN"
    return "PASS"


async def run_marvin_soak(
    iterations: int = 3,
    cooldown_seconds: float = 5.0,
) -> int:
    """Run the isolated MARVIN smoke harness repeatedly in serial."""
    print("=== GWA3 MARVIN Fresh-Session Soak Runner ===")
    print(f"Iterations: {iterations}")
    print(f"Build dir: {BUILD_DIR}")
    print(f"DLL: {DLL_NAME}")
    print(f"Pipe: {PIPE_NAME}")
    print(f"Launcher: {MARVIN_LANE.launcher}")
    print("")

    runs: list[dict[str, Any]] = []
    for run_index in range(1, iterations + 1):
        started = time.monotonic()
        run_events: list[str] = []
        summary: dict[str, Any] = {"ok": False}

        print(f"--- Run {run_index}/{iterations} ---")
        try:
            summary = await run_marvin_bridge_smoke(cleanup=True)
        except Exception as exc:
            run_events.append(type(exc).__name__)
            print(f"[run {run_index}] exception: {exc}")

        elapsed = time.monotonic() - started
        status = _status_for_run(summary, run_events)
        run_record = {
            "run": run_index,
            "status": status,
            "elapsed": elapsed,
            "events": run_events,
            "pid": summary.get("pid"),
            "initial_tick": summary.get("initial_tick"),
            "post_action_tick": summary.get("post_action_tick"),
            "action_success": summary.get("action_success", False),
            "target_action_success": summary.get("target_action_success", False),
            "target_agent_id": summary.get("target_agent_id"),
            "map_id": summary.get("map_id"),
            "loading_state": summary.get("loading_state"),
            "agent_id": summary.get("agent_id"),
        }
        runs.append(run_record)

        print(
            f"[run {run_index}] status={status} pid={run_record['pid']} "
            f"action_success={run_record['action_success']} "
            f"target_action_success={run_record['target_action_success']} "
            f"target_agent_id={run_record['target_agent_id']} "
            f"ticks={run_record['initial_tick']}->{run_record['post_action_tick']} "
            f"map_id={run_record['map_id']} loading_state={run_record['loading_state']} "
            f"agent_id={run_record['agent_id']} elapsed={elapsed:.1f}s "
            f"events={run_events or ['none']}"
        )
        print("")

        if cooldown_seconds > 0 and run_index < iterations:
            print(f"[run {run_index}] cooling down for {cooldown_seconds:.1f}s")
            await asyncio.sleep(cooldown_seconds)

    pass_count = sum(1 for run in runs if run["status"] == "PASS")
    warn_count = sum(1 for run in runs if run["status"] == "WARN")
    fail_count = sum(1 for run in runs if run["status"] == "FAIL")

    print("=== MARVIN Soak Summary ===")
    for run in runs:
        print(
            f"Run {run['run']}: status={run['status']} pid={run['pid']} "
            f"action_success={run['action_success']} "
            f"target_action_success={run['target_action_success']} "
            f"target_agent_id={run['target_agent_id']} "
            f"ticks={run['initial_tick']}->{run['post_action_tick']} "
            f"map_id={run['map_id']} loading_state={run['loading_state']} "
            f"agent_id={run['agent_id']} elapsed={run['elapsed']:.1f}s "
            f"events={run['events'] or ['none']}"
        )
    print("")
    print(f"Pass={pass_count} Warn={warn_count} Fail={fail_count} Total={len(runs)}")

    return 0 if fail_count == 0 else 1
