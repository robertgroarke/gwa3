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
    # Get a tier-3 snapshot so we have inventory/effects too, but tier-2 is enough for quests
    snap = await tc.wait_for_snapshot(tier=2, timeout=8.0)
    q = snap.get("quests", {})
    print(f"=== Quest state (map_id={snap.get('map', {}).get('map_id')}) ===")
    print(f"active_quest_id: {q.get('active_quest_id')}")
    print(f"quest_log_size: {q.get('quest_log_size')}")

    ac = q.get("active_quest") or {}
    if ac:
        print("\n-- active_quest --")
        print(json.dumps(ac, indent=2, ensure_ascii=False))

    print("\n-- quest_log --")
    for i, entry in enumerate(q.get("quest_log", [])):
        print(f"[{i}] {json.dumps(entry, ensure_ascii=False)}")

    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
