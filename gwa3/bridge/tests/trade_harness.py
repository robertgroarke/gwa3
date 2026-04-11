"""Helpers for two-client player-trade bridge tests."""

from __future__ import annotations

import asyncio
import ctypes
import json
import os
import re
import subprocess
import time
from pathlib import Path

from .helpers import TestFailure, assert_true

REPO_ROOT = Path(__file__).resolve().parents[3]
AUTOIT_EXE = Path(r"C:\Program Files (x86)\AutoIt3\AutoIt3.exe")
DEFAULT_MAIN_LAUNCHER = REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_disco_panic_via_gwlauncher.au3"
DEFAULT_MAIN_LOG = REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_disco_panic_via_gwlauncher.log"
DEFAULT_HELPER_LAUNCHER = REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_blumpkins_via_gwlauncher.au3"
DEFAULT_HELPER_LOG = REPO_ROOT / "GWA Censured" / "debug_scripts" / "launch_blumpkins_via_gwlauncher.log"
HELPER_NAME = "B L U M P K I N S"
DISCO_NAME = "D I S C O P A N I C"
TRADE_TEST_MAP_ID = 650
TRADE_TEST_REGION = 4
TRADE_TEST_DISTRICT = 99

_helper_pid: int | None = None
_disco_pid: int | None = None
_MIN_HEALTHY_MEM_KB = 100_000


def _build_dir() -> Path:
    return Path(os.environ.get("GWA3_BUILD_DIR", REPO_ROOT / "gwa3" / "build_trade"))


def _env_path(name: str, default: Path) -> Path:
    value = os.environ.get(name)
    return Path(value) if value else default


def _main_name() -> str:
    return os.environ.get("GWA3_TRADE_MAIN_NAME", DISCO_NAME)


def _helper_name() -> str:
    return os.environ.get("GWA3_TRADE_HELPER_NAME", HELPER_NAME)


def _main_launcher() -> Path:
    return _env_path("GWA3_TRADE_MAIN_LAUNCHER", DEFAULT_MAIN_LAUNCHER)


def _main_log() -> Path:
    return _env_path("GWA3_TRADE_MAIN_LOG", DEFAULT_MAIN_LOG)


def _helper_launcher() -> Path:
    return _env_path("GWA3_TRADE_HELPER_LAUNCHER", DEFAULT_HELPER_LAUNCHER)


def _helper_log() -> Path:
    return _env_path("GWA3_TRADE_HELPER_LOG", DEFAULT_HELPER_LOG)


def _trade_lane_env_overrides() -> dict[str, str]:
    keys = [
        "GWA3_BUILD_DIR",
        "GWA3_DLL_NAME",
        "GWA3_PIPE_NAME",
        "GWA3_TRADE_MAIN_NAME",
        "GWA3_TRADE_MAIN_LAUNCHER",
        "GWA3_TRADE_MAIN_LOG",
        "GWA3_TRADE_HELPER_NAME",
        "GWA3_TRADE_HELPER_LAUNCHER",
        "GWA3_TRADE_HELPER_LOG",
    ]
    return {key: value for key in keys if (value := os.environ.get(key))}


def _using_default_trade_lane() -> bool:
    return (
        _build_dir() == REPO_ROOT / "gwa3" / "build_trade"
        and _dll_name() == "gwa3_trade.dll"
        and _pipe_name() == r"\\.\pipe\gwa3_llm_trade"
        and _main_name() == DISCO_NAME
        and _main_launcher() == DEFAULT_MAIN_LAUNCHER
        and _main_log() == DEFAULT_MAIN_LOG
        and _helper_name() == HELPER_NAME
        and _helper_launcher() == DEFAULT_HELPER_LAUNCHER
        and _helper_log() == DEFAULT_HELPER_LOG
    )


def _validate_trade_lane_config() -> None:
    helper_overridden = any(
        os.environ.get(key)
        for key in ("GWA3_TRADE_HELPER_NAME", "GWA3_TRADE_HELPER_LAUNCHER", "GWA3_TRADE_HELPER_LOG")
    )
    main_or_lane_overridden = not _using_default_trade_lane()
    if main_or_lane_overridden and not helper_overridden:
        raise TestFailure(
            "Player-trade harness lane override is incomplete: main/build/pipe settings were customized "
            "without helper overrides. Set GWA3_TRADE_HELPER_NAME, GWA3_TRADE_HELPER_LAUNCHER, and "
            "GWA3_TRADE_HELPER_LOG explicitly to avoid crossing into the default trade helper lane. "
            f"overrides={_trade_lane_env_overrides()}"
        )


def _injector_exe() -> Path:
    return _build_dir() / "bin" / "Release" / "injector.exe"


def _dll_name() -> str:
    return os.environ.get("GWA3_DLL_NAME", "gwa3_trade.dll")


def _pipe_name() -> str:
    return os.environ.get("GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm_trade")


def _list_injected_trade_clients() -> list[int]:
    injector = _injector_exe()
    if not injector.exists():
        return []
    try:
        proc = subprocess.run(
            [str(injector), "--list", "--dll", _dll_name()],
            capture_output=True,
            text=True,
            check=False,
            timeout=15,
        )
    except Exception:
        return []
    if proc.returncode != 0:
        return []
    pids: list[int] = []
    for line in (proc.stdout or "").splitlines():
        m = re.match(r"^\s*(\d+)\s+yes\b", line.strip(), re.IGNORECASE)
        if m:
            pids.append(int(m.group(1)))
    return pids


def _helper_status_path() -> Path:
    return _build_dir() / "bin" / "Release" / "trade_helper_status.json"


def _helper_config_path() -> Path:
    return _build_dir() / "bin" / "Release" / "trade_helper_config.json"


def write_trade_helper_config(*, submit_gold: int = 0, auto_submit: bool = False, offer_item_model_id: int = 0) -> None:
    path = _helper_config_path()
    path.write_text(
        json.dumps({
            "submit_gold": int(submit_gold),
            "auto_submit": bool(auto_submit),
            "offer_item_model_id": int(offer_item_model_id),
        }),
        encoding="utf-8",
    )


def _list_character_gw_processes(character_name: str) -> list[dict]:
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


def _wait_for_pid_exit(pid: int, timeout: float = 15.0) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not _pid_is_running(pid):
            return True
        time.sleep(0.25)
    return not _pid_is_running(pid)


def _log_cleanup_target(reason: str, character_name: str, pid: int) -> None:
    print(f"[trade_harness] cleanup {reason}: character='{character_name}' pid={pid}", flush=True)


def _expected_log_path(pid: int | None) -> Path | None:
    if not pid or pid <= 0:
        return None
    return _build_dir() / "bin" / "Release" / f"gwa3_log_{int(pid)}.txt"


def trade_runtime_debug() -> dict:
    disco_log = _expected_log_path(_disco_pid)
    helper_log = _expected_log_path(_helper_pid)
    return {
        "main_name": _main_name(),
        "helper_name": _helper_name(),
        "disco_pid": _disco_pid or 0,
        "helper_pid": _helper_pid or 0,
        "main_launcher": str(_main_launcher()),
        "helper_launcher": str(_helper_launcher()),
        "main_launcher_log": str(_main_log()),
        "helper_launcher_log": str(_helper_log()),
        "disco_log": str(disco_log) if disco_log else None,
        "helper_log": str(helper_log) if helper_log else None,
        "disco_log_exists": bool(disco_log and disco_log.exists()),
        "helper_log_exists": bool(helper_log and helper_log.exists()),
        "pipe_name": _pipe_name(),
        "build_dir": str(_build_dir()),
        "dll_name": _dll_name(),
    }


def _select_healthy_character_pid(character_name: str) -> int | None:
    candidates = []
    for row in _list_character_gw_processes(character_name):
        pid = int(row["pid"])
        mem_kb = _pid_memory_kb(pid)
        if mem_kb >= _MIN_HEALTHY_MEM_KB and _pid_is_running(pid):
            candidates.append((mem_kb, pid))
    if not candidates:
        return None
    candidates.sort()
    return candidates[-1][1]


def _cleanup_stale_character_processes(character_name: str, keep_pid: int | None = None) -> None:
    for row in _list_character_gw_processes(character_name):
        pid = int(row["pid"])
        if keep_pid and pid == keep_pid:
            continue
        mem_kb = _pid_memory_kb(pid)
        if mem_kb < _MIN_HEALTHY_MEM_KB:
            _log_cleanup_target("stale_failed_boot", character_name, pid)
            _kill_pid(pid)


def cleanup_trade_clients() -> None:
    """Clean up only the trade-suite characters, never unrelated GW clients."""
    global _helper_pid, _disco_pid
    named_pids = (
        (_helper_name(), _helper_pid),
        (_main_name(), _disco_pid),
    )
    for character_name, pid in named_pids:
        if pid and _pid_is_running(pid):
            _log_cleanup_target("current_run_pid", character_name, pid)
            _kill_pid(pid)
            _wait_for_pid_exit(pid)
    for row in _list_character_gw_processes(_helper_name()):
        pid = int(row["pid"])
        _log_cleanup_target("same_character_sweep", _helper_name(), pid)
        _kill_pid(pid)
        _wait_for_pid_exit(pid)
    for row in _list_character_gw_processes(_main_name()):
        pid = int(row["pid"])
        _log_cleanup_target("same_character_sweep", _main_name(), pid)
        _kill_pid(pid)
        _wait_for_pid_exit(pid)
    _helper_pid = None
    _disco_pid = None
    try:
        _helper_config_path().unlink()
    except OSError:
        pass


async def _wait_for_trade_lane_quiet(timeout: float = 15.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        helper_alive = any(_pid_is_running(int(row["pid"])) for row in _list_character_gw_processes(_helper_name()))
        main_alive = any(_pid_is_running(int(row["pid"])) for row in _list_character_gw_processes(_main_name()))
        pipe_alive = await _pipe_exists()
        injected_trade_pids = _list_injected_trade_clients()
        if not helper_alive and not main_alive and (not pipe_alive or not injected_trade_pids):
            return
        await asyncio.sleep(0.5)
    raise TestFailure(
        "Trade harness cleanup did not quiesce the lane before relaunch "
        f"(main_alive={main_alive}, helper_alive={helper_alive}, pipe_alive={pipe_alive}, "
        f"injected_trade_pids={injected_trade_pids})"
    )


def _parse_pid_from_log(log_path: Path) -> int:
    text = log_path.read_text(encoding="utf-8", errors="ignore")
    m = re.search(r"GWLAUNCHER_PID=(\d+)", text)
    if not m:
        raise TestFailure(f"Could not parse helper PID from launcher log: {log_path}")
    return int(m.group(1))


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
        wait_object_0 = 0x00000000
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
    parts = next(__import__("csv").reader([line]), [])
    if len(parts) < 5:
        return 0
    mem = parts[4].replace(",", "").replace(" K", "").replace("K", "").strip()
    try:
        return int(mem)
    except ValueError:
        return 0


async def _wait_for_launcher_health(pid: int, label: str, timeout: float = 30.0) -> None:
    deadline = time.monotonic() + timeout
    best_mem = 0
    while time.monotonic() < deadline:
        if not _pid_is_running(pid):
            raise TestFailure(f"{label} launcher failure: GW PID {pid} exited before reaching a healthy loaded state")
        mem_kb = _pid_memory_kb(pid)
        if mem_kb > best_mem:
            best_mem = mem_kb
        if mem_kb >= _MIN_HEALTHY_MEM_KB:
            return
        await asyncio.sleep(0.5)
    raise TestFailure(
        f"{label} launcher failure: GW PID {pid} never reached healthy memory threshold "
        f"({_MIN_HEALTHY_MEM_KB} KB, best={best_mem} KB)"
    )


def _helper_status_ready(pid: int | None) -> bool:
    helper_status = _helper_status_path()
    if not helper_status.exists():
        return False
    try:
        payload = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
    except (OSError, json.JSONDecodeError):
        return False

    if int(payload.get("map_id", 0) or 0) != TRADE_TEST_MAP_ID:
        return False
    if int(payload.get("region", -1) or -1) != TRADE_TEST_REGION:
        return False
    district = int(payload.get("district", -1) or -1)
    if district < 0 or district == 0xFFFFFFFF:
        return False
    my_id = int(payload.get("my_id", 0) or 0)
    if my_id <= 0:
        return False
    x = float(payload.get("x", 0.0) or 0.0)
    y = float(payload.get("y", 0.0) or 0.0)
    if abs(x) < 1.0 and abs(y) < 1.0:
        return False
    return _pid_is_running(pid)


async def ensure_trade_helper_running(_attempt: int = 0) -> int:
    """Launch BLUMPKINS with GWLauncher and inject helper mode once per test process."""
    global _helper_pid
    _validate_trade_lane_config()
    helper_name = _helper_name()
    helper_launcher = _helper_launcher()
    helper_log = _helper_log()

    _cleanup_stale_character_processes(helper_name)
    _helper_pid = None

    assert_true(AUTOIT_EXE.exists(), f"AutoIt executable not found: {AUTOIT_EXE}")
    assert_true(helper_launcher.exists(), f"{helper_name} launcher script not found: {helper_launcher}")
    injector_exe = _injector_exe()
    helper_status = _helper_status_path()
    assert_true(injector_exe.exists(), f"Injector not found: {injector_exe}")

    if helper_log.exists():
        try:
            helper_log.unlink()
        except OSError:
            pass
    if helper_status.exists():
        try:
            helper_status.unlink()
        except OSError:
            pass
    helper_config = _helper_config_path()
    if not helper_config.exists():
        write_trade_helper_config(submit_gold=0, auto_submit=False)

    launch = await asyncio.create_subprocess_exec(
        str(AUTOIT_EXE),
        str(helper_launcher),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(REPO_ROOT),
    )
    stdout, stderr = await launch.communicate()
    if launch.returncode != 0:
        raise TestFailure(
            "BLUMPKINS launcher failed: "
            f"rc={launch.returncode} stdout={stdout.decode(errors='ignore')} "
            f"stderr={stderr.decode(errors='ignore')}"
        )

    deadline = time.monotonic() + 20.0
    while time.monotonic() < deadline:
        if helper_log.exists():
            try:
                pid = _parse_pid_from_log(helper_log)
                break
            except Exception:
                pass
        await asyncio.sleep(0.25)
    else:
        raise TestFailure(f"Timed out waiting for {helper_name} launcher log at {helper_log}")

    await _wait_for_launcher_health(pid, helper_name)

    inject = await asyncio.create_subprocess_exec(
        str(injector_exe),
        "--pid",
        str(pid),
        "--test-trade-helper",
        "--dll",
        _dll_name(),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(injector_exe.parent),
    )
    inject_stdout, inject_stderr = await inject.communicate()
    if inject.returncode != 0:
        raise TestFailure(
            "Helper injection failed: "
            f"rc={inject.returncode} stdout={inject_stdout.decode(errors='ignore')} "
            f"stderr={inject_stderr.decode(errors='ignore')}"
        )

    _helper_pid = pid

    deadline = time.monotonic() + 60.0
    while time.monotonic() < deadline:
        if _helper_status_ready(pid):
            break
        await asyncio.sleep(0.5)
    else:
        _kill_pid(pid)
        _helper_pid = None
        if _attempt < 1:
            return await ensure_trade_helper_running(_attempt + 1)
        raise TestFailure(f"Timed out waiting for helper-ready status at {helper_status}")

    return pid


async def _pipe_exists() -> bool:
    try:
        handle = ctypes.windll.kernel32.CreateFileW(
            _pipe_name(),
            0x80000000 | 0x40000000,  # GENERIC_READ | GENERIC_WRITE
            0,
            None,
            3,  # OPEN_EXISTING
            0,
            None,
        )
        if handle == ctypes.c_void_p(-1).value:
            return False
        ctypes.windll.kernel32.CloseHandle(handle)
        return True
    except Exception:
        return False


async def ensure_trade_main_running() -> int:
    """Launch a fresh Disco Panic client for this trade run and inject --llm."""
    global _disco_pid
    _validate_trade_lane_config()
    main_name = _main_name()
    main_launcher = _main_launcher()
    main_log = _main_log()

    cleanup_trade_clients()
    await _wait_for_trade_lane_quiet()

    assert_true(AUTOIT_EXE.exists(), f"AutoIt executable not found: {AUTOIT_EXE}")
    assert_true(main_launcher.exists(), f"{main_name} launcher script not found: {main_launcher}")
    injector_exe = _injector_exe()
    assert_true(injector_exe.exists(), f"Injector not found: {injector_exe}")

    if main_log.exists():
        try:
            main_log.unlink()
        except OSError:
            pass

    launch = await asyncio.create_subprocess_exec(
        str(AUTOIT_EXE),
        str(main_launcher),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(REPO_ROOT),
    )
    stdout, stderr = await launch.communicate()
    if launch.returncode != 0:
        raise TestFailure(
            "Disco Panic launcher failed: "
            f"rc={launch.returncode} stdout={stdout.decode(errors='ignore')} "
            f"stderr={stderr.decode(errors='ignore')}"
        )

    deadline = time.monotonic() + 20.0
    while time.monotonic() < deadline:
        if main_log.exists():
            try:
                pid = _parse_pid_from_log(main_log)
                break
            except Exception:
                pass
        await asyncio.sleep(0.25)
    else:
        raise TestFailure(f"Timed out waiting for {main_name} launcher log at {main_log}")

    await _wait_for_launcher_health(pid, main_name)

    inject = await asyncio.create_subprocess_exec(
        str(injector_exe),
        "--pid",
        str(pid),
        "--llm",
        "--dll",
        _dll_name(),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(injector_exe.parent),
    )
    inject_stdout, inject_stderr = await inject.communicate()
    if inject.returncode != 0:
        combined = (inject_stdout.decode(errors='ignore') + "\n" + inject_stderr.decode(errors='ignore'))
        if "already loaded" in combined:
            _log_cleanup_target("stale_already_loaded", main_name, pid)
            _kill_pid(pid)
            await asyncio.sleep(1.0)
            return await ensure_trade_main_running()
        raise TestFailure(
            "Disco injection failed: "
            f"rc={inject.returncode} stdout={inject_stdout.decode(errors='ignore')} "
            f"stderr={inject_stderr.decode(errors='ignore')}"
        )

    _disco_pid = pid

    deadline = time.monotonic() + 45.0
    while time.monotonic() < deadline:
        if not _pid_is_running(pid):
            raise TestFailure(f"{main_name} launcher failure: GW PID {pid} exited after injection before bridge pipe appeared")
        if await _pipe_exists() and _pid_is_running(pid):
            return pid
        await asyncio.sleep(0.5)

    raise TestFailure(f"Timed out waiting for bridge pipe {_pipe_name()} after launching {main_name}")
