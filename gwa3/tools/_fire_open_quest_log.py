"""Fire the new open_quest_log bridge action."""
import asyncio
import os
import sys

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase


async def main():
    tc = BridgeTestCase()
    await tc.setUp()
    r = await tc.send_action("open_quest_log", {})
    print(f"open_quest_log: success={r.get('success')} error={r.get('error')}")
    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
