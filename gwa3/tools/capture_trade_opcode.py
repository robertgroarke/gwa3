"""
Capture trade offer CtoS opcode.

Launches both clients, opens a trade via the test harness,
runs AutoIt to click an inventory item, captures the packet tap output.
"""

import asyncio
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

from bridge.tests.base import BridgeTestCase
from bridge.tests.trade_harness import (
    ensure_trade_helper_running,
    ensure_trade_main_running,
    cleanup_trade_clients,
    _pipe_name,
)
from bridge.tests.test_f_player_trade import _open_trade_with_helper

AUTOIT_EXE = Path(r"C:\Program Files (x86)\AutoIt3\AutoIt3.exe")
CLICKER_SCRIPT = REPO_ROOT.parent / "GWA Censured" / "debug_scripts" / "click_trade_offer_item.au3"


async def main():
    print("=== Trade Opcode Capture Tool ===\n")

    # Launch clients first (the trade harness handles this)
    print("[1/6] Cleaning up stale clients...")
    cleanup_trade_clients()
    await asyncio.sleep(2)

    print("[2/6] Launching DISCO PANIC...")
    disco_pid = await ensure_trade_main_running()
    print(f"       PID={disco_pid}")

    print("[3/6] Launching BLUMPKINS helper (auto_submit=True)...")
    from bridge.tests.trade_harness import write_trade_helper_config
    write_trade_helper_config(auto_submit=True, submit_gold=0)
    helper_pid = await ensure_trade_helper_running()
    print(f"       PID={helper_pid}")

    # Now create BridgeTestCase and connect using the trade pipe
    print("[4/6] Connecting and opening trade...")
    import os
    os.environ["GWA3_PIPE_NAME"] = _pipe_name()
    tc = BridgeTestCase()
    try:
        await tc.setUp()
    except Exception as e:
        print(f"       setUp failed: {e}")
        return

    try:
        helper_id, item_id = await _open_trade_with_helper(tc)
        print(f"       Trade OPEN! helper={helper_id} item={item_id}")
    except Exception as e:
        print(f"       Trade open failed: {e}")
        await tc.tearDown()
        return

    # Wait for helper to accept and trade window to fully open
    print("       Waiting for helper to accept trade...")
    await asyncio.sleep(8.0)

    # Run AutoIt clicker
    print("[5/6] Running AutoIt clicker to offer item via mouse...")
    if AUTOIT_EXE.exists() and CLICKER_SCRIPT.exists():
        proc = subprocess.run(
            [str(AUTOIT_EXE), str(CLICKER_SCRIPT)],
            capture_output=True, text=True, timeout=60,
        )
        print(f"       exit={proc.returncode}")
        for line in (proc.stdout or "").strip().split("\n"):
            if line.strip():
                print(f"       {line}")
    else:
        print(f"       SKIP: AutoIt={AUTOIT_EXE.exists()} Script={CLICKER_SCRIPT.exists()}")

    await asyncio.sleep(5.0)

    # Cancel and cleanup
    print("[6/6] Canceling trade...")
    try:
        await tc.send_action("cancel_trade", {}, timeout=5.0)
    except Exception:
        pass
    await asyncio.sleep(2.0)
    await tc.tearDown()

    # Dump packet tap
    print("\n=== PACKET TAP RESULTS ===")
    log_dir = REPO_ROOT / "build_trade" / "bin" / "Release"
    for lp in sorted(log_dir.glob("gwa3_log_*.txt"), key=lambda p: p.stat().st_mtime, reverse=True)[:3]:
        try:
            text = lp.read_text(errors="ignore")
        except Exception:
            continue
        if "llm=1" not in text[:500]:
            continue
        print(f"  Log: {lp.name}")
        for line in text.split("\n"):
            if "[PACKET-TAP]" in line:
                print(f"    {line.rstrip()}")
        break

    print("\nKnown: 0x0D=ping 0x28=cancel_action 0x49=trade_initiate 0x01=trade_cancel 0xC1=extended 0xB1=map_travel")
    print("NEW opcodes between trade-open and trade-cancel are the trade offer packets.")


if __name__ == "__main__":
    asyncio.run(main())
