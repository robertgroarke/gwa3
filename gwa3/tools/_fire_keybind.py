"""Fire action_key_press_raw(action)."""
import asyncio, os, sys
os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase

async def main():
    action = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x8E
    tc = BridgeTestCase()
    await tc.setUp()
    r = await tc.send_action("action_key_press_raw", {"action": action})
    print(f"action=0x{action:X} success={r.get('success')} error={r.get('error')}")
    await tc.tearDown()

if __name__ == "__main__":
    asyncio.run(main())
