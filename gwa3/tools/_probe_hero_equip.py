"""Add a hero, wait, then probe where the hero's equipment lives.

Approaches to test:
  1. Does bag 22 gain entries with Item.agent_id != me?
  2. Is there a higher bag index (>22) containing hero gear?
"""

import asyncio
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.ipc_client import IpcClient


async def grab_tier2(ipc, timeout=8.0):
    end = asyncio.get_event_loop().time() + timeout
    latest = None
    while asyncio.get_event_loop().time() < end:
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=0.5)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest = m
    return latest


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=10.0):
        print("CONNECT_FAIL")
        return 1

    # Hero IDs: 1=Koss is always available at low level if the first
    # mission is complete. Add whichever works.
    for hero_id in (1, 4, 6, 7, 8):
        print(f"[*] add_hero {hero_id}")
        await ipc.send_action("add_hero", {"hero_id": hero_id}, f"h{hero_id}")
        await asyncio.sleep(2.5)

    print("[*] waiting 10s for hero equipment to settle...")
    await asyncio.sleep(10)

    t2 = await grab_tier2(ipc)
    ipc.disconnect()
    if not t2:
        print("NO_TIER2")
        return 2

    heroes = t2.get("heroes", [])
    me = t2.get("me", {})
    print(f"\nme.agent_id={me.get('agent_id')}")
    print(f"heroes count={len(heroes)}")
    for h in heroes:
        eq = h.get("equipment", {})
        slots = sum(1 for k, v in eq.items()
                    if not k.startswith("_") and isinstance(v, dict))
        print(f"  hero agent_id={h.get('agent_id')} prof={h.get('primary')} "
              f"equipment_slots={slots}")
        for slot, it in eq.items():
            if not isinstance(it, dict):
                continue
            print(f"    {slot}: item_id={it.get('item_id')} "
                  f"model={it.get('model_id')} "
                  f"name={it.get('name', '<no-name>')!r}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
