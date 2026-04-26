"""Call UIMgr::ActionKeyPress via a lightweight path — use the
perform_ui_action_slot action with a known-good slot to probe."""
import asyncio, os, sys
os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase

async def main():
    # We need a fresh bridge action that calls UIMgr::ActionKeyPress(action).
    # Use perform_ui_action_slot as a shortcut — but that goes through
    # PerformUiActionAtSlot, not ActionKeyPress. We need a different path.
    # For now, use the existing scan_ui_labels hook as a marker that the
    # bridge works, then check if there's a key-press handler.
    tc = BridgeTestCase()
    await tc.setUp()
    print("bridge connected")
    await tc.tearDown()

if __name__ == "__main__":
    asyncio.run(main())
