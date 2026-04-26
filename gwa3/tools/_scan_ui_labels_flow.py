"""Full flow: open the Quest Log, wait for GW to render it, fire the UI
label walker, wait for the next snapshot, dump any decoded quest names."""
import asyncio
import json
import os
import sys

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase


async def main():
    tc = BridgeTestCase()
    await tc.setUp()

    print("=== step 1: open_quest_log ===")
    r = await tc.send_action("open_quest_log", {})
    print(f"  result: {r.get('success')} error={r.get('error')}")
    await asyncio.sleep(5.0)  # let GW render the window

    print("=== step 2: scan_ui_labels ===")
    r = await tc.send_action("scan_ui_labels", {})
    print(f"  result: {r.get('success')} error={r.get('error')}")
    await asyncio.sleep(3.0)  # allow a snapshot cycle

    print("=== step 3: read snapshot ===")
    snap = await tc.wait_for_snapshot(tier=2, timeout=8.0)
    q = snap["quests"]
    aq = q.get("active_quest") or {}
    log = q.get("quest_log", [])
    print(f"active_quest_id: {q.get('active_quest_id')}")
    decoded_active = {k: v for k, v in aq.items()
                      if not k.endswith("_enc") and isinstance(v, str)}
    print(f"active_quest decoded fields: {list(decoded_active.keys())}")
    for k, v in decoded_active.items():
        snippet = v if len(v) < 160 else v[:160] + "..."
        print(f"  {k}: {snippet}")

    named = [e for e in log if "name" in e or "location" in e or "npc" in e]
    print(f"\nquest_log entries with ANY decoded field: {len(named)} / {len(log)}")
    for e in named[:10]:
        print(f"  quest_id={e['quest_id']} name={e.get('name')!r} location={e.get('location')!r} npc={e.get('npc')!r}")

    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
