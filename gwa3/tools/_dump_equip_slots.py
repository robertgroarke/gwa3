"""Dump every Equipment slot + contents of the structs they point to
so we can find where Reforged hid the 32-bit item_ids."""

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

    diag = (tier2.get("me", {}).get("equipment", {}) or {}).get("_drift_diag")
    if not diag:
        print("no _drift_diag")
        return 3

    print(f"equip_ptr=0x{diag['equip_ptr']:08x}")
    print(f"weapon_id16={diag['weapon_id16']} offhand_id16={diag['offhand_id16']}")

    header = diag.get("header_dump", [])
    print(f"\n[header dump of Equipment ({len(header)} uint32s)]")
    for i, v in enumerate(header):
        off = i * 4
        notes = ""
        if _plausible_item_id(v):
            notes = "<- plausible item_id"
        elif 0x01000000 < v < 0x7FFFFFFF:
            notes = "(heap ptr)"
        elif v == 0xFFFFFFFF:
            notes = "(sentinel -1)"
        print(f"  0x{off:04x}  0x{v:08x}  {notes}")

    slots = diag.get("slots", [])
    # Map slot index to the header offset we probed (mirrors the C++
    # kCandidateSlotPtrs table in GameSnapshot.cpp).
    header_offs = [0x18, 0x2C, 0x3C, 0x54, 0x68, 0x78, 0x8C, 0xA4, 0xB8, 0xC8]
    print(f"\n[slot sub-struct dumps ({len(slots)} slots probed)]")
    for s, slot in enumerate(slots):
        ptr = slot.get("ptr", 0)
        item_ptr = slot.get("item_ptr", 0)
        body = slot.get("body", [])
        item_body = slot.get("item_body", [])
        print(f"\n  slot[{s}] header-off=+0x{header_offs[s]:02x} "
              f"ptr=0x{ptr:08x} item_ptr=0x{item_ptr:08x}")
        for i, v in enumerate(body):
            off = i * 4
            notes = ""
            if _plausible_item_id(v):
                notes = "<- PLAUSIBLE ITEM_ID"
            elif 0x01000000 < v < 0x7FFFFFFF:
                notes = "(heap ptr)"
            elif v == 0xFFFFFFFF:
                notes = "(sentinel -1)"
            print(f"    +0x{off:02x}  0x{v:08x}  {notes}")
        if item_body:
            print(f"    --- deref item_ptr ---")
            for i, v in enumerate(item_body):
                off = i * 4
                notes = ""
                if _plausible_item_id(v) and off in (0x00,):
                    notes = "<- item_id (Item+0x00)?"
                elif _plausible_item_id(v):
                    notes = "<- plausible"
                elif 0x01000000 < v < 0x7FFFFFFF:
                    notes = "(heap ptr)"
                print(f"    item+0x{off:02x}  0x{v:08x}  {notes}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
