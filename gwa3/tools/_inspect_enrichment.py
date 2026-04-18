"""One-shot snapshot inspector for task #2 (snapshot enrichment).

Connects to the biscuit lane pipe, waits for a tier-2 snapshot, and
prints a summary of the enriched fields added in commit 6418e1b:

- dialog.buttons[].label (now prefers cached decoded text)
- inventory.bags[].items[].name (new — from Item.name_enc via cache)
- merchant.items[].name (new — same path)
- agents[agent_type=item].name + model_id (new — resolved via
  ItemMgr::GetItemById from the dropped-item item_id)
"""

import asyncio
import io
import os
import sys

# Force stdout to UTF-8 so decoded item names with accents don't choke cp1252.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.ipc_client import IpcClient


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=30.0):
        print("CONNECT_FAIL")
        return 1
    print("connected")

    tier2 = None
    tier3 = None
    # Snapshot cadence: tier-1 ~500ms, tier-2 ~1500ms, tier-3 ~6s
    # Drain 60 messages or ~90s, keeping the latest tier-2 and tier-3.
    for _ in range(60):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=3.0)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot":
            t = m.get("tier")
            if t == 2:
                tier2 = m
            elif t == 3:
                tier3 = m
        if tier2 and tier3:
            break
    ipc.disconnect()

    if not tier2:
        print("NO_TIER2")
        return 2

    print(f"\n=== Tier-2 snapshot ===")
    print(f"map_id={tier2.get('map', {}).get('map_id')}")

    # Quest log (tier-2) — proves L02/L03 prerequisites
    q = tier2.get("quests", {}) or {}
    log = q.get("quest_log", [])
    print(f"\n[quests] active_quest_id={q.get('active_quest_id')} "
          f"log_size={q.get('quest_log_size')}")
    decoded_names = [e for e in log if "name" in e and e["name"]]
    print(f"  log entries with decoded 'name': {len(decoded_names)}/{len(log)}")
    for e in log[:5]:
        print(f"    quest_id={e.get('quest_id')} "
              f"name={e.get('name', '<enc-only>')!r}")

    # Dialog button labels
    dialog = tier2.get("dialog", {}) or {}
    print(f"\n[dialog] is_open={dialog.get('is_open')}")
    if dialog.get("is_open"):
        for btn in dialog.get("buttons", []):
            print(f"  button dialog_id=0x{btn.get('dialog_id', 0):x} "
                  f"label={btn.get('label')!r}")

    # Inventory item names (tier-3)
    print(f"\n[inventory] (from tier-3; tier3_received={tier3 is not None})")
    inv = (tier3 or {}).get("inventory", {}) or {}
    bags = inv.get("bags", [])
    print(f"  bags={len(bags)} "
          f"items_total={sum(b.get('item_count', 0) for b in bags)}")
    with_name = []
    without_name = 0
    for bag in bags:
        for item in bag.get("items", []):
            if "name" in item and item["name"]:
                with_name.append((item.get("model_id"), item["name"],
                                  item.get("quantity", 1)))
            else:
                without_name += 1
    print(f"  items with decoded name: {len(with_name)}")
    print(f"  items WITHOUT name (not yet cached by decoder): {without_name}")
    for mid, nm, qty in with_name[:8]:
        print(f"    model={mid} qty={qty} name={nm!r}")
    with_full = []
    for bag in bags:
        for item in bag.get("items", []):
            if "full_name" in item and item["full_name"]:
                with_full.append((item.get("model_id"), item["full_name"],
                                  item.get("quantity", 1)))
    print(f"  items with full_name (modifier-decorated): {len(with_full)}")
    for mid, fn, qty in with_full[:8]:
        print(f"    model={mid} qty={qty} full_name={fn!r}")

    # Merchant items
    merch = tier2.get("merchant", {}) or {}
    print(f"\n[merchant] is_open={merch.get('is_open')}")
    if merch.get("is_open"):
        items = merch.get("items", [])
        named = [it for it in items if "name" in it and it["name"]]
        print(f"  items={len(items)}, with decoded name={len(named)}")
        for it in items[:8]:
            print(f"    item_id={it.get('item_id')} "
                  f"model={it.get('model_id')} "
                  f"name={it.get('name', '<none>')!r}")

    # Dropped items on the ground (agent_type=item)
    agents = tier2.get("agents", [])
    dropped = [a for a in agents if a.get("agent_type") == "item"]
    named = [a for a in dropped if "name" in a and a["name"]]
    print(f"\n[dropped items] total={len(dropped)}, "
          f"with decoded name={len(named)}")
    for a in dropped[:8]:
        print(f"  agent_id={a.get('id')} item_id={a.get('item_id')} "
              f"model_id={a.get('model_id')} "
              f"name={a.get('name', '<none>')!r} "
              f"dist={a.get('distance'):.0f}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
