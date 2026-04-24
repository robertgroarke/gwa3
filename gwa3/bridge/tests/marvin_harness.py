"""Lane-aware launcher/injection smoke harness helpers.

This module retains the historical MARVIN smoke entrypoint and now also
supports BEASTRIT using the same isolated-launch rules.
"""

from __future__ import annotations

import asyncio
import ctypes
import csv
import json
import os
import re
import subprocess
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from ..ipc_client import IpcClient
from .helpers import TestFailure, assert_true

REPO_ROOT = Path(__file__).resolve().parents[3]
AUTOIT_EXE = Path(r"C:\Program Files (x86)\AutoIt3\AutoIt3.exe")
_MIN_HEALTHY_MEM_KB = 100_000


@dataclass(frozen=True)
class SmokeLane:
    slug: str
    display_name: str
    character_name: str
    launcher: Path
    log_path: Path
    default_build_dir: Path
    default_dll_name: str
    default_pipe_name: str
    lock_filename: str


MARVIN_LANE = SmokeLane(
    slug="marvin",
    display_name="MARVIN",
    character_name="Starvin M A R V I N",
    launcher=REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_marvin_via_gwlauncher.au3",
    log_path=REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_marvin_via_gwlauncher.log",
    default_build_dir=REPO_ROOT / "gwa3" / "build_marvin",
    default_dll_name="gwa3_marvin.dll",
    default_pipe_name=r"\\.\pipe\gwa3_llm_marvin",
    lock_filename="marvin_bridge_smoke.lock",
)

BEASTRIT_LANE = SmokeLane(
    slug="beastrit",
    display_name="BEASTRIT",
    character_name="B E A S T R I T",
    launcher=REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_beastrit_via_gwlauncher.au3",
    log_path=REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_beastrit_via_gwlauncher.log",
    default_build_dir=REPO_ROOT / "gwa3" / "build_beastrit",
    default_dll_name="gwa3_beastrit.dll",
    default_pipe_name=r"\\.\pipe\gwa3_llm_beastrit",
    lock_filename="beastrit_bridge_smoke.lock",
)

LANES_BY_SLUG = {
    MARVIN_LANE.slug: MARVIN_LANE,
    BEASTRIT_LANE.slug: BEASTRIT_LANE,
}


def get_smoke_lane(slug: str) -> SmokeLane:
    lane = LANES_BY_SLUG.get((slug or "").strip().lower())
    if lane is None:
        known = ", ".join(sorted(LANES_BY_SLUG))
        raise TestFailure(f"Unknown smoke lane '{slug}'. Expected one of: {known}")
    return lane


def _build_dir(lane: SmokeLane = MARVIN_LANE) -> Path:
    return Path(os.environ.get("GWA3_BUILD_DIR", lane.default_build_dir))


def _injector_exe(lane: SmokeLane = MARVIN_LANE) -> Path:
    return _build_dir(lane) / "bin" / "Release" / "injector.exe"


def _dll_name(lane: SmokeLane = MARVIN_LANE) -> str:
    return os.environ.get("GWA3_DLL_NAME", lane.default_dll_name)


def _pipe_name(lane: SmokeLane = MARVIN_LANE) -> str:
    return os.environ.get("GWA3_PIPE_NAME", lane.default_pipe_name)


def _lock_path(lane: SmokeLane = MARVIN_LANE) -> Path:
    return _build_dir(lane) / "bin" / "Release" / lane.lock_filename


def _read_lock_payload(path: Path) -> dict[str, Any] | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return None


def _acquire_run_lock(lane: SmokeLane = MARVIN_LANE) -> Path:
    path = _lock_path(lane)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps({"pid": os.getpid(), "time": time.time(), "lane": lane.slug})
    try_count = 0
    while True:
        try:
            fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            break
        except FileExistsError as exc:
            try_count += 1
            lock_payload = _read_lock_payload(path)
            owner_pid = int((lock_payload or {}).get("pid", 0) or 0)
            if owner_pid > 0 and not _pid_is_running(owner_pid):
                try:
                    path.unlink(missing_ok=True)
                except Exception:
                    pass
                if try_count < 2:
                    continue
            raise TestFailure(
                f"Another {lane.display_name} smoke run appears active; lock file already exists at {path}"
            ) from exc
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            handle.write(payload)
    except Exception:
        try:
            path.unlink(missing_ok=True)
        except Exception:
            pass
        raise
    return path


def _release_run_lock(path: Path | None) -> None:
    if path is None:
        return
    try:
        path.unlink(missing_ok=True)
    except Exception:
        pass


def _snapshot_world_state(snapshot: dict[str, Any]) -> dict[str, Any]:
    map_state = snapshot.get("map", {}) if isinstance(snapshot, dict) else {}
    me = snapshot.get("me", {}) if isinstance(snapshot, dict) else {}
    return {
        "snapshot_tier": int(snapshot.get("tier", 0) or 0),
        "tick": int(snapshot.get("tick", 0) or 0),
        "map_id": int(map_state.get("map_id", 0) or 0),
        "loading_state": int(map_state.get("loading_state", 0) or 0),
        "is_loaded": bool(map_state.get("is_loaded", False)),
        "agent_id": int(me.get("agent_id", 0) or 0),
    }


def _snapshot_is_world_ready(snapshot: dict[str, Any]) -> bool:
    state = _snapshot_world_state(snapshot)
    return (
        state["map_id"] > 0
        and state["loading_state"] == 1
        and state["is_loaded"]
        and state["agent_id"] > 0
    )


def _pick_target_candidate(snapshot: dict[str, Any]) -> int | None:
    me = snapshot.get("me", {}) if isinstance(snapshot, dict) else {}
    current_target = int(me.get("target_id", 0) or 0)
    agents = snapshot.get("agents", []) if isinstance(snapshot, dict) else []
    best: tuple[float, int] | None = None
    for agent in agents:
        try:
            agent_id = int(agent.get("id", 0) or 0)
            distance = float(agent.get("distance", 1e9) or 1e9)
        except Exception:
            continue
        if agent_id <= 0 or agent_id == current_target:
            continue
        if best is None or distance < best[0]:
            best = (distance, agent_id)
    return None if best is None else best[1]


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


def _parse_pid_from_log(log_path: Path) -> int:
    try:
        text = log_path.read_text(encoding="utf-8", errors="ignore")
    except OSError as exc:
        raise TestFailure(f"Could not read launcher log {log_path}: {exc}") from exc
    pid = _parse_pid_from_text(text)
    if pid is None:
        raise TestFailure(f"Could not parse GWLAUNCHER_PID from launcher log: {log_path}")
    return pid


def _list_character_gw_processes(character_name: str) -> list[dict[str, Any]]:
    script = rf"""
$procs = Get-CimInstance Win32_Process -Filter "name='Gw.exe'" | Where-Object {{
    $_.CommandLine -and $_.CommandLine -like '*-character "{character_name}"*'
}} | Select-Object ProcessId, ExecutablePath, CommandLine
$procs | ConvertTo-Json -Compress
"""
    try:
        proc = subprocess.run(
            ["powershell", "-NoProfile", "-Command", script],
            capture_output=True,
            text=True,
            check=False,
            timeout=15,
        )
    except Exception:
        return []
    if proc.returncode != 0:
        return []
    text = (proc.stdout or "").strip()
    if not text:
        return []
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        return []
    if isinstance(data, dict):
        data = [data]
    out = []
    for row in data:
        try:
            pid = int(row.get("ProcessId", 0) or 0)
        except Exception:
            pid = 0
        if pid > 0:
            out.append(
                {
                    "pid": pid,
                    "exe": row.get("ExecutablePath") or "",
                    "cmdline": row.get("CommandLine") or "",
                }
            )
    return out


def _kill_pid(pid: int) -> None:
    if pid <= 0:
        return
    subprocess.run(
        ["taskkill", "/PID", str(pid), "/T", "/F"],
        capture_output=True,
        text=True,
        check=False,
        timeout=15,
    )


def _pid_is_running(pid: int | None) -> bool:
    if not pid or pid <= 0:
        return False
    process_query_limited_information = 0x1000
    synchronize = 0x00100000
    handle = ctypes.windll.kernel32.OpenProcess(
        process_query_limited_information | synchronize,
        False,
        int(pid),
    )
    if not handle:
        return False
    try:
        wait_timeout = 0x00000102
        result = ctypes.windll.kernel32.WaitForSingleObject(handle, 0)
        return result == wait_timeout
    finally:
        ctypes.windll.kernel32.CloseHandle(handle)


def _pid_memory_kb(pid: int | None) -> int:
    if not pid or pid <= 0:
        return 0
    try:
        proc = subprocess.run(
            ["tasklist", "/FI", f"PID eq {int(pid)}", "/FO", "CSV", "/NH"],
            capture_output=True,
            text=True,
            check=False,
            timeout=10,
        )
    except Exception:
        return 0
    line = (proc.stdout or "").strip()
    if not line or "No tasks are running" in line:
        return 0
    parts = next(csv.reader([line]), [])
    if len(parts) < 5:
        return 0
    mem = parts[4].replace(",", "").replace(" K", "").replace("K", "").strip()
    try:
        return int(mem)
    except ValueError:
        return 0


def _choose_best_pid(
    process_rows: list[dict[str, Any]],
    min_memory_kb: int = _MIN_HEALTHY_MEM_KB,
    memory_fn=_pid_memory_kb,
    running_fn=_pid_is_running,
) -> int | None:
    candidates: list[tuple[int, int]] = []
    for row in process_rows:
        pid = int(row.get("pid", 0) or 0)
        if pid <= 0:
            continue
        mem_kb = memory_fn(pid)
        if mem_kb >= min_memory_kb and running_fn(pid):
            candidates.append((mem_kb, pid))
    if not candidates:
        return None
    candidates.sort()
    return candidates[-1][1]


def _select_healthy_character_pid(character_name: str) -> int | None:
    return _choose_best_pid(_list_character_gw_processes(character_name))


def _cleanup_stale_character_processes(character_name: str, keep_pid: int | None = None) -> None:
    for row in _list_character_gw_processes(character_name):
        pid = int(row["pid"])
        if keep_pid and pid == keep_pid:
            continue
        mem_kb = _pid_memory_kb(pid)
        if mem_kb < _MIN_HEALTHY_MEM_KB:
            _kill_pid(pid)


def cleanup_marvin_client() -> None:
    for row in _list_character_gw_processes(MARVIN_LANE.character_name):
        _kill_pid(int(row["pid"]))


def cleanup_beastrit_client() -> None:
    for row in _list_character_gw_processes(BEASTRIT_LANE.character_name):
        _kill_pid(int(row["pid"]))


async def _wait_for_launcher_health(pid: int, lane: SmokeLane, timeout: float = 30.0) -> None:
    deadline = time.monotonic() + timeout
    best_mem = 0
    while time.monotonic() < deadline:
        if not _pid_is_running(pid):
            raise TestFailure(
                f"{lane.display_name} launcher failure: GW PID {pid} exited before reaching a healthy loaded state"
            )
        mem_kb = _pid_memory_kb(pid)
        if mem_kb > best_mem:
            best_mem = mem_kb
        if mem_kb >= _MIN_HEALTHY_MEM_KB:
            return
        await asyncio.sleep(0.5)
    raise TestFailure(
        f"{lane.display_name} launcher failure: "
        f"GW PID {pid} never reached healthy memory threshold "
        f"({_MIN_HEALTHY_MEM_KB} KB, best={best_mem} KB)"
    )


async def _wait_for_character_pid(
    lane: SmokeLane,
    launcher_pid_hint: int,
    timeout: float = 45.0,
) -> int:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        pid = _select_healthy_character_pid(lane.character_name)
        if pid:
            return pid
        await asyncio.sleep(0.5)
    raise TestFailure(
        f"{lane.display_name} launch did not produce a healthy {lane.character_name} Gw.exe process "
        f"(launcher_pid_hint={launcher_pid_hint})"
    )


async def _pipe_exists(lane: SmokeLane = MARVIN_LANE) -> bool:
    try:
        handle = ctypes.windll.kernel32.CreateFileW(
            _pipe_name(lane),
            0x80000000 | 0x40000000,
            0,
            None,
            3,
            0,
            None,
        )
        if handle == ctypes.c_void_p(-1).value:
            return False
        ctypes.windll.kernel32.CloseHandle(handle)
        return True
    except Exception:
        return False


async def _launch_lane(lane: SmokeLane) -> int:
    assert_true(
        _is_running_as_admin(),
        f"{lane.display_name} smoke run requires an elevated shell because the GWLauncher AutoIt script is marked #RequireAdmin",
    )
    assert_true(AUTOIT_EXE.exists(), f"AutoIt executable not found: {AUTOIT_EXE}")
    assert_true(lane.launcher.exists(), f"{lane.display_name} launcher script not found: {lane.launcher}")
    assert_true(_injector_exe(lane).exists(), f"Injector not found: {_injector_exe(lane)}")

    _cleanup_stale_character_processes(lane.character_name)

    existing = _select_healthy_character_pid(lane.character_name)
    if existing:
        raise TestFailure(
            f"Refusing to launch {lane.display_name} smoke while a healthy {lane.character_name} client is already running (pid={existing})"
        )

    if lane.log_path.exists():
        try:
            lane.log_path.unlink()
        except OSError:
            pass

    launch = await asyncio.create_subprocess_exec(
        str(AUTOIT_EXE),
        str(lane.launcher),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(REPO_ROOT),
    )
    stdout, stderr = await launch.communicate()
    if launch.returncode != 0:
        raise TestFailure(
            f"{lane.display_name} launcher failed: "
            f"rc={launch.returncode} stdout={stdout.decode(errors='ignore')} "
            f"stderr={stderr.decode(errors='ignore')}"
        )

    deadline = time.monotonic() + 20.0
    while time.monotonic() < deadline:
        if lane.log_path.exists():
            try:
                launcher_pid = _parse_pid_from_log(lane.log_path)
                break
            except TestFailure:
                pass
        await asyncio.sleep(0.25)
    else:
        raise TestFailure(f"Timed out waiting for {lane.display_name} launcher log at {lane.log_path}")

    pid = await _wait_for_character_pid(lane, launcher_pid)
    await _wait_for_launcher_health(pid, lane)
    return pid


async def _inject_llm_bridge(pid: int, lane: SmokeLane) -> None:
    inject = await asyncio.create_subprocess_exec(
        str(_injector_exe(lane)),
        "--pid",
        str(pid),
        "--llm",
        "--dll",
        _dll_name(lane),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(_injector_exe(lane).parent),
    )
    stdout, stderr = await inject.communicate()
    if inject.returncode != 0:
        raise TestFailure(
            f"{lane.display_name} injection failed: "
            f"rc={inject.returncode} stdout={stdout.decode(errors='ignore')} "
            f"stderr={stderr.decode(errors='ignore')}"
        )


async def _wait_for_bridge_ready(pid: int, lane: SmokeLane, timeout: float = 45.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not _pid_is_running(pid):
            raise TestFailure(
                f"{lane.display_name} launcher failure: GW PID {pid} exited after injection before bridge pipe appeared"
            )
        if await _pipe_exists(lane):
            break
        await asyncio.sleep(0.5)
    else:
        raise TestFailure(f"Timed out waiting for bridge pipe {_pipe_name(lane)} after launching {lane.display_name}")

    ipc = IpcClient(_pipe_name(lane))
    if not await ipc.connect(timeout=15.0):
        raise TestFailure(f"Could not connect to {lane.display_name} bridge pipe {_pipe_name(lane)} ({ipc.last_error})")
    try:
        first = await ipc.read_message(timeout=10.0)
        if first is None:
            raise TestFailure(f"Connected to {lane.display_name} bridge pipe {_pipe_name(lane)} but received no bridge traffic")
        snapshot = first
        if first.get("type") != "snapshot":
            deadline = time.monotonic() + 10.0
            while time.monotonic() < deadline:
                msg = await ipc.read_message(timeout=1.0)
                if msg is not None and msg.get("type") == "snapshot":
                    snapshot = msg
                    break
            else:
                raise TestFailure(f"Connected to {lane.display_name} bridge but did not observe a snapshot")

        world_snapshot = snapshot
        if not _snapshot_is_world_ready(world_snapshot):
            deadline = time.monotonic() + 20.0
            while time.monotonic() < deadline:
                msg = await ipc.read_message(timeout=1.0)
                if msg is not None and msg.get("type") == "snapshot" and _snapshot_is_world_ready(msg):
                    world_snapshot = msg
                    break
            else:
                observed = _snapshot_world_state(snapshot)
                raise TestFailure(
                    f"Connected to {lane.display_name} bridge but did not observe a world-ready snapshot "
                    f"(observed={observed})"
                )

        world_state = _snapshot_world_state(world_snapshot)
        return {
            "ipc": ipc,
            "lane": lane.slug,
            "character_name": lane.character_name,
            "first_message_type": first.get("type"),
            "snapshot_tier": world_state["snapshot_tier"],
            "map_id": world_state["map_id"],
            "loading_state": world_state["loading_state"],
            "is_loaded": world_state["is_loaded"],
            "agent_id": world_state["agent_id"],
            "initial_tick": world_state["tick"],
            "pipe_name": _pipe_name(lane),
        }
    except Exception:
        ipc.disconnect()
        raise


async def _read_until_action_result(ipc: IpcClient, request_id: str, timeout: float = 5.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        msg = await ipc.read_message(timeout=max(0.1, deadline - time.monotonic()))
        if msg is None:
            continue
        if msg.get("type") == "action_result" and msg.get("request_id") == request_id:
            return msg
    raise TestFailure(f"Timed out waiting for action_result request_id={request_id}")


async def _read_until_snapshot_tick_gt(
    ipc: IpcClient,
    minimum_tick: int,
    timeout: float = 5.0,
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        msg = await ipc.read_message(timeout=max(0.1, deadline - time.monotonic()))
        if msg is None:
            continue
        if msg.get("type") == "snapshot" and int(msg.get("tick", 0) or 0) > minimum_tick:
            return msg
    raise TestFailure(f"Timed out waiting for snapshot tick > {minimum_tick}")


async def _read_until_tier2_with_candidate(
    ipc: IpcClient,
    timeout: float = 10.0,
) -> tuple[dict[str, Any], int]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        msg = await ipc.read_message(timeout=max(0.1, deadline - time.monotonic()))
        if msg is None or msg.get("type") != "snapshot":
            continue
        if int(msg.get("tier", 0) or 0) < 2:
            continue
        candidate = _pick_target_candidate(msg)
        if candidate:
            return msg, candidate
    raise TestFailure("Timed out waiting for a tier-2 snapshot with a targetable nearby agent")


async def _read_until_target_id(
    ipc: IpcClient,
    target_id: int,
    timeout: float = 5.0,
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        msg = await ipc.read_message(timeout=max(0.1, deadline - time.monotonic()))
        if msg is None or msg.get("type") != "snapshot":
            continue
        me = msg.get("me", {})
        if int(me.get("target_id", 0) or 0) == target_id:
            return msg
    raise TestFailure(f"Timed out waiting for me.target_id == {target_id}")


async def _exercise_cancel_action_roundtrip(ipc: IpcClient, initial_tick: int) -> dict[str, Any]:
    request_id = str(uuid.uuid4())[:8]
    await ipc.send_action("cancel_action", {}, request_id)
    result = await _read_until_action_result(ipc, request_id, timeout=5.0)
    if result.get("success") is not True:
        raise TestFailure(f"cancel_action roundtrip failed: {result}")
    post_snapshot = await _read_until_snapshot_tick_gt(ipc, initial_tick, timeout=5.0)
    return {
        "request_id": request_id,
        "action_name": "cancel_action",
        "action_success": bool(result.get("success")),
        "post_action_tick": int(post_snapshot.get("tick", 0) or 0),
    }


async def _exercise_change_target_roundtrip(ipc: IpcClient) -> dict[str, Any]:
    target_snapshot, target_id = await _read_until_tier2_with_candidate(ipc, timeout=10.0)
    request_id = str(uuid.uuid4())[:8]
    await ipc.send_action("change_target", {"agent_id": target_id}, request_id)
    result = await _read_until_action_result(ipc, request_id, timeout=5.0)
    if result.get("success") is not True:
        raise TestFailure(f"change_target roundtrip failed: {result}")
    confirmed_snapshot = await _read_until_target_id(ipc, target_id, timeout=5.0)
    return {
        "target_request_id": request_id,
        "target_action_name": "change_target",
        "target_action_success": bool(result.get("success")),
        "target_agent_id": target_id,
        "pre_target_tick": int(target_snapshot.get("tick", 0) or 0),
        "post_target_tick": int(confirmed_snapshot.get("tick", 0) or 0),
    }


async def run_bridge_smoke(lane: SmokeLane | str, cleanup: bool = True) -> dict[str, Any]:
    if isinstance(lane, str):
        lane = get_smoke_lane(lane)

    started = time.monotonic()
    pid: int | None = None
    ipc: IpcClient | None = None
    lock_path: Path | None = None
    try:
        lock_path = _acquire_run_lock(lane)
        pid = await _launch_lane(lane)
        await _inject_llm_bridge(pid, lane)
        bridge = await _wait_for_bridge_ready(pid, lane)
        ipc = bridge.pop("ipc")
        roundtrip = await _exercise_cancel_action_roundtrip(ipc, bridge["initial_tick"])
        target_roundtrip = await _exercise_change_target_roundtrip(ipc)
        return {
            "ok": True,
            "pid": pid,
            "elapsed_seconds": round(time.monotonic() - started, 2),
            **bridge,
            **roundtrip,
            **target_roundtrip,
        }
    finally:
        if ipc is not None:
            ipc.disconnect()
        if cleanup and pid and _pid_is_running(pid):
            _kill_pid(pid)
        _release_run_lock(lock_path)


async def run_marvin_bridge_smoke(cleanup: bool = True) -> dict[str, Any]:
    return await run_bridge_smoke(MARVIN_LANE, cleanup=cleanup)


async def run_beastrit_bridge_smoke(cleanup: bool = True) -> dict[str, Any]:
    return await run_bridge_smoke(BEASTRIT_LANE, cleanup=cleanup)
