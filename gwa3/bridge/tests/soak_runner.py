"""Fresh-session soak runner for the launcher-based Disco Panic bridge suite."""

from __future__ import annotations

import asyncio
import ctypes
import os
import re
import time
from pathlib import Path
from typing import Any

from .helpers import TestFailure, assert_true
from .runner import run_suite

REPO_ROOT = Path(__file__).resolve().parents[3]
BUILD_DIR = Path(os.environ.get("GWA3_BUILD_DIR", REPO_ROOT / "gwa3" / "build"))
AUTOIT_EXE = Path(r"C:\Program Files (x86)\AutoIt3\AutoIt3.exe")
DISCO_LAUNCHER = REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_disco_panic_via_gwlauncher.au3"
DISCO_LOG = REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_disco_panic_via_gwlauncher.log"
INJECTOR_EXE = BUILD_DIR / "bin" / "Release" / "injector.exe"
DLL_NAME = os.environ.get("GWA3_DLL_NAME", "gwa3.dll")
PIPE_NAME = os.environ.get("GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm")
ORCHESTRATED_MODULES = ["test_e_orchestrated"]
DEFAULT_SOAK_FILTER = "test_orchestrated*"


def _is_running_as_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def _parse_pid_from_text(text: str) -> int | None:
    match = re.search(r"GWLAUNCHER_PID=(\d+)", text)
    if not match:
        return None
    return int(match.group(1))


async def _launch_disco_panic() -> tuple[int, str]:
    assert_true(
        _is_running_as_admin(),
        "Fresh-session soak requires an elevated shell because the GWLauncher AutoIt script is marked #RequireAdmin",
    )
    assert_true(AUTOIT_EXE.exists(), f"AutoIt executable not found: {AUTOIT_EXE}")
    assert_true(DISCO_LAUNCHER.exists(), f"Disco Panic launcher script not found: {DISCO_LAUNCHER}")
    assert_true(INJECTOR_EXE.exists(), f"Injector not found: {INJECTOR_EXE}")

    if DISCO_LOG.exists():
        try:
            DISCO_LOG.unlink()
        except OSError:
            pass

    launch = await asyncio.create_subprocess_exec(
        str(AUTOIT_EXE),
        str(DISCO_LAUNCHER),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(REPO_ROOT),
    )
    pid: int | None = None
    stdout_text = ""
    stderr_text = ""
    deadline = time.monotonic() + 30.0

    while time.monotonic() < deadline:
        if DISCO_LOG.exists():
            try:
                stdout_text = DISCO_LOG.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                stdout_text = ""
            pid = _parse_pid_from_text(stdout_text)
            if pid is not None:
                break

        try:
            await asyncio.wait_for(launch.wait(), timeout=0.25)
        except asyncio.TimeoutError:
            pass

        if launch.returncode is not None:
            if launch.stdout is not None:
                stdout_bytes = await launch.stdout.read()
                stdout_text = stdout_bytes.decode(errors="ignore")
            if launch.stderr is not None:
                stderr_bytes = await launch.stderr.read()
                stderr_text = stderr_bytes.decode(errors="ignore")
            break

    if pid is None and launch.returncode is None:
        launch.terminate()
        try:
            await asyncio.wait_for(launch.wait(), timeout=2.0)
        except asyncio.TimeoutError:
            launch.kill()
            await launch.wait()

    if pid is None and launch.returncode is not None:
        raise TestFailure(
            "Disco Panic launcher failed: "
            f"rc={launch.returncode} stdout={stdout_text} stderr={stderr_text}"
        )

    if pid is None:
        raise TestFailure(
            f"Could not parse GWLAUNCHER_PID from launcher output or log at {DISCO_LOG}"
        )

    if launch.returncode is None:
        launch.terminate()
        try:
            await asyncio.wait_for(launch.wait(), timeout=2.0)
        except asyncio.TimeoutError:
            launch.kill()
            await launch.wait()

    return pid, stdout_text


async def _inject_bridge(pid: int) -> dict[str, Any]:
    inject = await asyncio.create_subprocess_exec(
        str(INJECTOR_EXE),
        "--pid",
        str(pid),
        "--llm",
        "--dll",
        DLL_NAME,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(INJECTOR_EXE.parent),
    )
    stdout, stderr = await inject.communicate()
    return {
        "returncode": inject.returncode,
        "stdout": stdout.decode(errors="ignore"),
        "stderr": stderr.decode(errors="ignore"),
    }


async def _is_process_running(pid: int) -> bool:
    probe = await asyncio.create_subprocess_exec(
        "tasklist",
        "/FI",
        f"PID eq {pid}",
        "/FO",
        "CSV",
        "/NH",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, _stderr = await probe.communicate()
    text = stdout.decode(errors="ignore").strip()
    if not text or "No tasks are running" in text:
        return False
    return str(pid) in text


async def _terminate_process(pid: int) -> bool:
    if not await _is_process_running(pid):
        return False
    kill = await asyncio.create_subprocess_exec(
        "taskkill",
        "/PID",
        str(pid),
        "/T",
        "/F",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    await kill.communicate()
    await asyncio.sleep(1.0)
    return True


def _aggregate_status(summary: dict[str, Any], run_events: list[str], inject_returncode: int) -> str:
    if inject_returncode != 0:
        return "FAIL"
    if summary["failed"] > 0:
        return "FAIL"
    if "process_gone" in run_events:
        return "FAIL"
    if summary["skipped"] > 0:
        return "WARN"
    return "PASS"


async def run_soak(
    iterations: int = 3,
    filter_pattern: str | None = None,
    settle_seconds: float = 12.0,
    cooldown_seconds: float = 5.0,
) -> int:
    """Run the orchestrated bridge suite repeatedly on fresh Disco Panic sessions."""
    filter_pattern = filter_pattern or DEFAULT_SOAK_FILTER
    print("=== GWA3 Bridge Fresh-Session Soak Runner ===")
    print(f"Iterations: {iterations}")
    print(f"Filter: {filter_pattern}")
    print(f"Build dir: {BUILD_DIR}")
    print(f"Injector: {INJECTOR_EXE}")
    print(f"DLL: {DLL_NAME}")
    print(f"Pipe: {PIPE_NAME}")
    print(f"Launcher: {DISCO_LAUNCHER}")
    print("")

    runs: list[dict[str, Any]] = []
    for run_index in range(1, iterations + 1):
        run_started = time.monotonic()
        run_events: list[str] = []
        pid: int | None = None
        summary: dict[str, Any] = {
            "exit_code": 1,
            "passed": 0,
            "failed": 0,
            "skipped": 0,
            "total": 0,
            "results": [],
        }
        inject_result = {"returncode": 1, "stdout": "", "stderr": ""}
        cleanup_forced = False
        process_alive_after_suite = False

        print(f"--- Run {run_index}/{iterations} ---")
        try:
            pid, _launch_stdout = await _launch_disco_panic()
            print(f"[run {run_index}] launcher PID={pid}")

            if settle_seconds > 0:
                print(f"[run {run_index}] settling for {settle_seconds:.1f}s before injection")
                await asyncio.sleep(settle_seconds)

            inject_result = await _inject_bridge(pid)
            print(f"[run {run_index}] injector rc={inject_result['returncode']}")
            if inject_result["returncode"] != 0:
                run_events.append("inject_failed")
                raise TestFailure(
                    "Injector failed: "
                    f"stdout={inject_result['stdout']} stderr={inject_result['stderr']}"
                )

            summary = await run_suite(
                filter_pattern=filter_pattern,
                module_names=ORCHESTRATED_MODULES,
                emit_output=True,
            )
            process_alive_after_suite = await _is_process_running(pid)
            if not process_alive_after_suite:
                run_events.append("process_gone")
        except Exception as exc:
            run_events.append(type(exc).__name__)
            print(f"[run {run_index}] exception: {exc}")
        finally:
            if pid is not None:
                cleanup_forced = await _terminate_process(pid)
            if cooldown_seconds > 0 and run_index < iterations:
                print(f"[run {run_index}] cooling down for {cooldown_seconds:.1f}s")
                await asyncio.sleep(cooldown_seconds)

        elapsed = time.monotonic() - run_started
        status = _aggregate_status(summary, run_events, int(inject_result["returncode"]))
        run_record = {
            "run": run_index,
            "pid": pid,
            "status": status,
            "elapsed": elapsed,
            "passed": summary["passed"],
            "failed": summary["failed"],
            "skipped": summary["skipped"],
            "total": summary["total"],
            "events": run_events,
            "process_alive_after_suite": process_alive_after_suite,
            "cleanup_forced": cleanup_forced,
        }
        runs.append(run_record)
        print(
            f"[run {run_index}] status={status} passed={summary['passed']} "
            f"failed={summary['failed']} skipped={summary['skipped']} "
            f"elapsed={elapsed:.1f}s events={run_events or ['none']}"
        )
        print("")

    pass_count = sum(1 for run in runs if run["status"] == "PASS")
    warn_count = sum(1 for run in runs if run["status"] == "WARN")
    fail_count = sum(1 for run in runs if run["status"] == "FAIL")

    print("=== Soak Summary ===")
    for run in runs:
        print(
            f"Run {run['run']}: status={run['status']} pid={run['pid']} "
            f"passed={run['passed']}/{run['total']} skipped={run['skipped']} "
            f"elapsed={run['elapsed']:.1f}s process_alive_after_suite={run['process_alive_after_suite']} "
            f"cleanup_forced={run['cleanup_forced']} events={run['events'] or ['none']}"
        )
    print("")
    print(f"Pass={pass_count} Warn={warn_count} Fail={fail_count} Total={len(runs)}")

    return 0 if fail_count == 0 else 1
