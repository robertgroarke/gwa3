"""Fire request_quest_info(active_quest_id) to trigger the one-shot
ProbeQuestStringLayout diagnostic log in the DLL."""
import asyncio
import os
import sys

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase


async def main():
    tc = BridgeTestCase()
    await tc.setUp()
    snap = await tc.wait_for_snapshot(tier=2, timeout=8.0)
    aid = snap["quests"]["active_quest_id"]
    print(f"active_quest_id: {aid}")
    r = await tc.send_action("request_quest_info", {"quest_id": aid})
    print(f"result: success={r.get('success')} error={r.get('error')}")
    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
