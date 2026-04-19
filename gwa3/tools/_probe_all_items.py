"""After adding heroes, dump all items with their (bag, slot, agent_id)
so we can find which bag/agent_id tag marks hero equipment."""

import asyncio
import io
import os
import sys
from collections import defaultdict

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

    # Add a couple of heroes first (ignore failures).
    for hid in (1, 4):
        await ipc.send_action("add_hero", {"hero_id": hid}, f"h{hid}")
        await asyncio.sleep(2.0)
    print("[*] waiting 10s after add_hero...")
    await asyncio.sleep(10)

    # Drain to latest tier-2 snapshot.
    latest = None
    for _ in range(20):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=1.5)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest = m
    ipc.disconnect()
    if not latest:
        print("NO_TIER2")
        return 2

    me = latest.get("me", {})
    my_agent = me.get("agent_id")
    heroes = latest.get("heroes", [])
    hero_ids = {h.get("agent_id") for h in heroes}

    items = me.get("_all_items", [])
    print(f"me.agent_id={my_agent} hero_ids={sorted(hero_ids)}")
    print(f"total items reported: {len(items)}\n")

    # Group by bag, then show any bag that contains items tagged with a
    # hero agent_id or any bag > 22 (unmapped territory).
    by_bag = defaultdict(list)
    for it in items:
        by_bag[it["bag"]].append(it)

    for bag in sorted(by_bag.keys()):
        bag_items = by_bag[bag]
        # Count tags
        my_count = sum(1 for it in bag_items if it["agent_id"] == my_agent)
        hero_count = sum(1 for it in bag_items if it["agent_id"] in hero_ids
                         and it["agent_id"] != 0)
        zero_count = sum(1 for it in bag_items if it["agent_id"] == 0)
        other_count = len(bag_items) - my_count - hero_count - zero_count
        print(f"bag {bag}: n={len(bag_items)} "
              f"me={my_count} heroes={hero_count} "
              f"zero_agent={zero_count} other={other_count}")
        if hero_count > 0 or bag > 22:
            # Show details for interesting bags.
            for it in bag_items[:15]:
                tag = "me" if it["agent_id"] == my_agent else (
                    f"hero-{it['agent_id']}" if it["agent_id"] in hero_ids
                    else f"agent-{it['agent_id']}")
                print(f"  item_id={it['item_id']} slot={it['slot']} "
                      f"model={it['model_id']} equipped={it['equipped']} "
                      f"tag={tag}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
