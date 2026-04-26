"""Fire perform_ui_action_slot with the given action id and slot index."""
import asyncio
import os
import sys

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase


async def main():
    action = int(sys.argv[1]) if len(sys.argv) > 1 else 0x8E
    slot = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    tc = BridgeTestCase()
    await tc.setUp()
    r = await tc.send_action("perform_ui_action_slot", {"action": action, "slot": slot})
    print(f"action=0x{action:X} slot=+0x{slot*4:X} success={r.get('success')} error={r.get('error')}")
    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
