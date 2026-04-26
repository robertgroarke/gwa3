"""Fire set_window_visible_raw(window_id, visible)."""
import asyncio, os, sys
os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase

async def main():
    window_id = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x52
    visible = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    tc = BridgeTestCase()
    await tc.setUp()
    r = await tc.send_action("set_window_visible_raw",
                             {"window_id": window_id, "visible": visible})
    print(f"window_id=0x{window_id:X} visible={visible} success={r.get('success')} error={r.get('error')}")
    await tc.tearDown()

if __name__ == "__main__":
    asyncio.run(main())
