"""Trigger add_hero from the LLM bridge, then dump recent DLL log
lines so we can see the exact call path."""

import asyncio
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.ipc_client import IpcClient


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=10.0):
        print("CONNECT_FAIL")
        return 1

    # Use a hero id from Froggy's confirmed-working set.
    for hero_id in (4, 14, 21, 24, 15):
        print(f"[*] add_hero {hero_id}")
        await ipc.send_action("add_hero", {"hero_id": hero_id}, f"h{hero_id}")
        await asyncio.sleep(3.0)

    print("[*] waiting 6s for party state to settle")
    await asyncio.sleep(6)

    # Drain to latest tier-2 snapshot.
    latest = None
    for _ in range(20):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=1.0)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest = m
    ipc.disconnect()

    if not latest:
        print("NO_TIER2")
        return 2

    me = latest.get("me", {})
    party = latest.get("party", {})
    heroes = latest.get("heroes", [])
    members = party.get("members", [])
    print(f"\nme.agent_id={me.get('agent_id')}")
    print(f"party.size={party.get('size')} dead={party.get('dead_count')}")
    print(f"party.members count={len(members)}")
    for mem in members:
        print(f"  member agent_id={mem.get('agent_id')} "
              f"prof={mem.get('primary')} "
              f"is_player={mem.get('is_player')} "
              f"is_hero={mem.get('is_hero')}")
    print(f"heroes[] count={len(heroes)}")
    for h in heroes:
        print(f"  hero agent_id={h.get('agent_id')} prof={h.get('primary')}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
