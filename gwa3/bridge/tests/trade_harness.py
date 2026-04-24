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

from ..ipc_client import IpcClient
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
_helper_chat_seq = 0
_helper_whisper_seq = 0


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


def write_trade_helper_config(
    *,
    submit_gold: int = 0,
    auto_submit: bool = False,
    auto_accept: bool | None = None,
    offer_item_model_id: int = 0,
    offer_item_quantity: int = 0,
    offer_item_model_id_2: int = 0,
    offer_item_quantity_2: int = 0,
    move_x: float = 0.0,
    move_y: float = 0.0,
    move_seq: int = 0,
    chat_send_seq: int = 0,
    chat_send_channel: str = "",
    chat_send_message: str = "",
    whisper_send_seq: int = 0,
    whisper_send_recipient: str = "",
    whisper_send_message: str = "",
) -> None:
    resolved_auto_accept = auto_submit if auto_accept is None else auto_accept
    path = _helper_config_path()
    path.write_text(
        json.dumps({
            "submit_gold": int(submit_gold),
            "auto_submit": bool(auto_submit),
            "auto_accept": bool(resolved_auto_accept),
            "offer_item_model_id": int(offer_item_model_id),
            "offer_item_quantity": int(offer_item_quantity),
            "offer_item_model_id_2": int(offer_item_model_id_2),
            "offer_item_quantity_2": int(offer_item_quantity_2),
            "move_x": float(move_x),
            "move_y": float(move_y),
            "move_seq": int(move_seq),
            "chat_send_seq": int(chat_send_seq),
            "chat_send_channel": str(chat_send_channel),
            "chat_send_message": str(chat_send_message),
            "whisper_send_seq": int(whisper_send_seq),
            "whisper_send_recipient": str(whisper_send_recipient),
            "whisper_send_message": str(whisper_send_message),
        }),
        encoding="utf-8",
    )


def _read_trade_helper_config() -> dict:
    path = _helper_config_path()
    if not path.exists():
        return {}
    try:
        payload = json.loads(path.read_text(encoding="utf-8", errors="ignore"))
    except Exception:
        return {}
    return payload if isinstance(payload, dict) else {}


def _write_trade_helper_config_merged(**overrides) -> None:
    payload = _read_trade_helper_config()
    payload.update(overrides)
    write_trade_helper_config(
        submit_gold=int(payload.get("submit_gold", 0) or 0),
        auto_submit=bool(payload.get("auto_submit", False)),
        auto_accept=(bool(payload.get("auto_accept")) if "auto_accept" in payload else None),
        offer_item_model_id=int(payload.get("offer_item_model_id", 0) or 0),
        offer_item_quantity=int(payload.get("offer_item_quantity", 0) or 0),
        offer_item_model_id_2=int(payload.get("offer_item_model_id_2", 0) or 0),
        offer_item_quantity_2=int(payload.get("offer_item_quantity_2", 0) or 0),
        move_x=float(payload.get("move_x", 0.0) or 0.0),
        move_y=float(payload.get("move_y", 0.0) or 0.0),
        move_seq=int(payload.get("move_seq", 0) or 0),
        chat_send_seq=int(payload.get("chat_send_seq", 0) or 0),
        chat_send_channel=str(payload.get("chat_send_channel", "") or ""),
        chat_send_message=str(payload.get("chat_send_message", "") or ""),
        whisper_send_seq=int(payload.get("whisper_send_seq", 0) or 0),
        whisper_send_recipient=str(payload.get("whisper_send_recipient", "") or ""),
        whisper_send_message=str(payload.get("whisper_send_message", "") or ""),
    )


def request_helper_send_chat(*, channel: str, message: str) -> int:
    global _helper_chat_seq
    _helper_chat_seq += 1
    _write_trade_helper_config_merged(
        chat_send_seq=_helper_chat_seq,
        chat_send_channel=str(channel),
        chat_send_message=str(message),
    )
    return _helper_chat_seq


def request_helper_send_whisper(*, recipient: str, message: str) -> int:
    global _helper_whisper_seq
    _helper_whisper_seq += 1
    _write_trade_helper_config_merged(
        whisper_send_seq=_helper_whisper_seq,
        whisper_send_recipient=str(recipient),
        whisper_send_message=str(message),
    )
    return _helper_whisper_seq


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


def _read_runtime_log(pid: int | None) -> str:
    log_path = _expected_log_path(pid)
    if not log_path or not log_path.exists():
        return ""
    try:
        return log_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return ""


def _bootstrap_stuck_reason(pid: int | None) -> str | None:
    text = _read_runtime_log(pid)
    if not text:
        return None
    play_clicks = len(re.findall(r"Bootstrap: clicking Play", text))
    map_ids = [int(m.group(1)) for m in re.finditer(r"MapID=(\d+)", text)]
    max_map_id = max(map_ids) if map_ids else 0
    if play_clicks >= 5 and max_map_id == 0:
        return f"pre-game bootstrap stuck at Play (clicks={play_clicks}, max_map_id={max_map_id})"
    return None


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


async def _wait_for_trade_lane_quiet(timeout: float = 15.0, *, preserve_helper: bool = False) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        helper_alive = any(_pid_is_running(int(row["pid"])) for row in _list_character_gw_processes(_helper_name()))
        main_alive = any(_pid_is_running(int(row["pid"])) for row in _list_character_gw_processes(_main_name()))
        pipe_alive = await _pipe_exists()
        injected_trade_pids = _list_injected_trade_clients()
        helper_ok = helper_alive if preserve_helper else not helper_alive
        if helper_ok and not main_alive and (not pipe_alive or not injected_trade_pids):
            return
        await asyncio.sleep(0.5)
    raise TestFailure(
        "Trade harness cleanup did not quiesce the lane before relaunch "
        f"(main_alive={main_alive}, helper_alive={helper_alive}, preserve_helper={preserve_helper}, pipe_alive={pipe_alive}, "
        f"injected_trade_pids={injected_trade_pids})"
    )


def _parse_pid_from_log(log_path: Path) -> int:
    text = log_path.read_text(encoding="utf-8", errors="ignore")
    matches = re.findall(r"GWLAUNCHER_PID=(\d+)", text)
    if not matches:
        raise TestFailure(f"Could not parse helper PID from launcher log: {log_path}")
    return int(matches[-1])


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

    if _helper_pid and _pid_is_running(_helper_pid) and _helper_status_ready(_helper_pid):
        return _helper_pid

    for row in _list_character_gw_processes(helper_name):
        pid = int(row["pid"])
        _log_cleanup_target("same_character_sweep", helper_name, pid)
        _kill_pid(pid)
        _wait_for_pid_exit(pid)
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
    helper_log.parent.mkdir(parents=True, exist_ok=True)
    helper_log.write_text("", encoding="utf-8")
    if helper_status.exists():
        try:
            helper_status.unlink()
        except OSError:
            pass
    helper_config = _helper_config_path()
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

    try:
        await _wait_for_launcher_health(pid, helper_name)
    except TestFailure:
        _log_cleanup_target("stale_failed_boot", helper_name, pid)
        _kill_pid(pid)
        _wait_for_pid_exit(pid)
        _helper_pid = None
        if _attempt < 1:
            await asyncio.sleep(1.0)
            return await ensure_trade_helper_running(_attempt + 1)
        raise

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
        reason = _bootstrap_stuck_reason(pid)
        if reason:
            _log_cleanup_target("bootstrap_stuck", helper_name, pid)
            _kill_pid(pid)
            _helper_pid = None
            if _attempt < 1:
                return await ensure_trade_helper_running(_attempt + 1)
            raise TestFailure(f"{helper_name} launcher failure: {reason}")
        await asyncio.sleep(0.5)
    else:
        _kill_pid(pid)
        _helper_pid = None
        if _attempt < 1:
            return await ensure_trade_helper_running(_attempt + 1)
        raise TestFailure(f"Timed out waiting for helper-ready status at {helper_status}")

    return pid


async def _pipe_exists() -> bool:
    client = IpcClient(_pipe_name())
    try:
        if await client.connect(timeout=0.25):
            client.disconnect()
            return True
    except Exception:
        pass
    return False


async def ensure_trade_main_running(*, preserve_helper: bool = False, _attempt: int = 0) -> int:
    """Launch a fresh Disco Panic client for this trade run and inject --llm."""
    global _disco_pid
    _validate_trade_lane_config()
    main_name = _main_name()
    main_launcher = _main_launcher()
    main_log = _main_log()

    if preserve_helper:
        if _disco_pid and _pid_is_running(_disco_pid):
            _log_cleanup_target("current_run_pid", main_name, _disco_pid)
            _kill_pid(_disco_pid)
            _wait_for_pid_exit(_disco_pid)
        for row in _list_character_gw_processes(main_name):
            pid = int(row["pid"])
            _log_cleanup_target("same_character_sweep", main_name, pid)
            _kill_pid(pid)
            _wait_for_pid_exit(pid)
        _disco_pid = None
    else:
        cleanup_trade_clients()
    await _wait_for_trade_lane_quiet(preserve_helper=preserve_helper)

    assert_true(AUTOIT_EXE.exists(), f"AutoIt executable not found: {AUTOIT_EXE}")
    assert_true(main_launcher.exists(), f"{main_name} launcher script not found: {main_launcher}")
    injector_exe = _injector_exe()
    assert_true(injector_exe.exists(), f"Injector not found: {injector_exe}")

    if main_log.exists():
        try:
            main_log.unlink()
        except OSError:
            pass
    main_log.parent.mkdir(parents=True, exist_ok=True)
    main_log.write_text("", encoding="utf-8")

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

    try:
        await _wait_for_launcher_health(pid, main_name)
    except TestFailure:
        _log_cleanup_target("stale_failed_boot", main_name, pid)
        _kill_pid(pid)
        _wait_for_pid_exit(pid)
        _disco_pid = None
        if _attempt < 1:
            await asyncio.sleep(1.0)
            return await ensure_trade_main_running(preserve_helper=preserve_helper, _attempt=_attempt + 1)
        raise

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
            return await ensure_trade_main_running(preserve_helper=preserve_helper, _attempt=_attempt)
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
        reason = _bootstrap_stuck_reason(pid)
        if reason:
            _log_cleanup_target("bootstrap_stuck", main_name, pid)
            _kill_pid(pid)
            _disco_pid = None
            if _attempt < 1:
                await asyncio.sleep(1.0)
                return await ensure_trade_main_running(preserve_helper=preserve_helper, _attempt=_attempt + 1)
            raise TestFailure(f"{main_name} launcher failure: {reason}")
        await asyncio.sleep(0.5)

    raise TestFailure(f"Timed out waiting for bridge pipe {_pipe_name()} after launching {main_name}")
