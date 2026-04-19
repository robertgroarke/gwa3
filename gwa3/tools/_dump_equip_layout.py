"""Dump the Equipment struct bytes so we can find where Reforged moved
the item_ids. Highlights plausible item_ids — anything in the range of
valid ItemMgr ids (typically 1..1_000_000 give or take)."""

import asyncio
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.ipc_client import IpcClient


def _plausible_item_id(v: int) -> bool:
    return 1 <= v <= 1_000_000


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=10.0):
        print("CONNECT_FAIL")
        return 1

    tier2 = None
    for _ in range(30):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=2.0)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            tier2 = m
            break
    ipc.disconnect()

    if not tier2:
        print("NO_TIER2")
        return 2

    me = tier2.get("me", {})
    eq = me.get("equipment", {}) or {}
    print(f"equip_ptr=0x{eq.get('_equip_ptr', 0):08x}")
    print(f"weapon_id16={eq.get('_weapon_id16')} "
          f"offhand_id16={eq.get('_offhand_id16')}")

    dump = eq.get("_dump", [])
    if not dump:
        print("no dump")
        return 3

    print(f"\nHex dump of Equipment struct ({len(dump)} uint32s, "
          f"{len(dump)*4:#x} bytes):")
    print(f"{'offset':>6}  {'hex':>10}  {'dec':>12}  {'notes':<30}")
    for i, v in enumerate(dump):
        off = i * 4
        notes = ""
        if _plausible_item_id(v):
            notes = "<- plausible item_id"
        elif 0x01000000 < v < 0x7FFFFFFF:
            notes = "(heap pointer?)"
        print(f"  0x{off:04x}  0x{v:08x}  {v:12}  {notes}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
