"""Live manipulation test: read quest log, set_active_quest to a different
quest, confirm active_quest_id flips in the next snapshot."""
import asyncio
import json
import os
import sys

os.environ.setdefault("GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm_biscuit")

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase


async def main():
    tc = BridgeTestCase()
    await tc.setUp()
    snap = await tc.wait_for_snapshot(tier=2, timeout=8.0)
    q = snap["quests"]
    prev_active = q["active_quest_id"]
    log = q["quest_log"]
    print(f"[before] active_quest_id={prev_active}, log_size={len(log)}")

    # pick a different quest_id from the log
    target = None
    for e in log:
        if e["quest_id"] != prev_active and e["quest_id"] > 0:
            target = e["quest_id"]
            break
    if target is None:
        print("no alternate quest in log")
        await tc.tearDown()
        return

    print(f"[action] set_active_quest({target})")
    r = await tc.send_action("set_active_quest", {"quest_id": target})
    print(f"  result: success={r.get('success')} error={r.get('error')}")

    # Wait for the flip
    def matches(s):
        return s.get("quests", {}).get("active_quest_id") == target

    try:
        new_snap = await tc.wait_for_state_change(matches, tier=2, timeout=6.0)
        new_active = new_snap["quests"]["active_quest_id"]
        new_log = new_snap["quests"]["quest_log"]
        active_entry = next((x for x in new_log if x["quest_id"] == new_active), None)
        print(f"[after] active_quest_id={new_active}  (flipped={new_active==target})")
        print(f"  is_active flag in log: {active_entry and active_entry.get('is_active')}")
    except Exception as e:
        print(f"[after] flip did not happen: {e}")

    # Restore the previous active quest if we changed it
    if prev_active and prev_active != target:
        print(f"[restore] set_active_quest({prev_active})")
        await tc.send_action("set_active_quest", {"quest_id": prev_active})

    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
