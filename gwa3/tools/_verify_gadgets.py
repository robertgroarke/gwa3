"""Travel to the Embark Beach Xunlai chest, wait, then dump gadgets.

The Xunlai chest itself is a gadget and there are portal signposts
clustered around it — a dense spot for verifying the gadget name
resolution path (AgentContext.agent_summary_info -> gadget_name_enc
fallback to GadgetContext.GadgetInfo[gadget_id]).
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


# Embark Beach, Xunlai chest position from farming_knowledge.py.
XUNLAI_X = 2283.0
XUNLAI_Y = -2134.0


async def drain_until(ipc, predicate, timeout=8.0):
    end = asyncio.get_event_loop().time() + timeout
    while asyncio.get_event_loop().time() < end:
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=0.5)
        except asyncio.TimeoutError:
            continue
        if m and predicate(m):
            return m
    return None


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=10.0):
        print("CONNECT_FAIL")
        return 1

    # Keep re-issuing the move every 15s — GW snaps to the next
    # waypoint and sometimes a single move_to stalls partway. Three
    # retries cover the ~5500-unit trip from the default spawn.
    for i in range(3):
        print(f"[*] move_to({XUNLAI_X}, {XUNLAI_Y}) attempt {i+1}")
        await ipc.send_action("move_to",
                              {"x": XUNLAI_X, "y": XUNLAI_Y},
                              f"mv{i+1}")
        await asyncio.sleep(15.0)

    # Grab the freshest tier-2 snapshot we can.
    print("[*] reading snapshot...")
    latest_t2 = None
    for _ in range(30):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=1.0)
        except asyncio.TimeoutError:
            break
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest_t2 = m
    ipc.disconnect()

    if not latest_t2:
        print("NO_TIER2")
        return 2

    agents = latest_t2.get("agents", [])
    gadgets = [a for a in agents if a.get("agent_type") == "gadget"]
    living  = [a for a in agents if a.get("agent_type") == "living"]
    items   = [a for a in agents if a.get("agent_type") == "item"]

    named_g = [g for g in gadgets if "name" in g and g["name"]]
    named_l = [l for l in living  if "name" in l and l["name"]]

    me = latest_t2.get("me", {})
    print(f"\npos=({me.get('x', 0):.0f}, {me.get('y', 0):.0f}) "
          f"map={latest_t2.get('map', {}).get('map_id')}")
    print(f"living={len(living)} named={len(named_l)}")
    print(f"gadget={len(gadgets)} named={len(named_g)}")
    print(f"item={len(items)}")

    print("\n[gadgets]")
    for g in sorted(gadgets, key=lambda a: a.get("distance", 99999))[:15]:
        print(f"  id={g['id']} dist={g.get('distance', 0):.0f} "
              f"gadget_id={g.get('gadget_id')} "
              f"name={g.get('name', '<no-name>')!r}")

    print("\n[named living]")
    for l in sorted(named_l, key=lambda a: a.get("distance", 99999))[:10]:
        print(f"  id={l['id']} dist={l.get('distance', 0):.0f} "
              f"pn={l.get('player_number')} "
              f"name={l.get('name')!r}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
