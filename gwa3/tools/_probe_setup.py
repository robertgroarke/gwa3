import asyncio
import os
import sys
import traceback

os.environ.setdefault("GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm_biscuit")

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase


async def main():
    tc = BridgeTestCase()
    print("pipe:", tc.ipc.pipe_name)
    try:
        await asyncio.wait_for(tc.setUp(), timeout=15.0)
        print("setUp OK")
    except Exception as e:
        print("setUp FAIL:", type(e).__name__, "-", repr(e))
        traceback.print_exc()
        return
    try:
        snap = await tc.wait_for_snapshot(tier=2, timeout=5.0)
        print("tier-2 snapshot keys:", list(snap.keys()))
        q = snap.get("quests", {})
        print("quests:", {k: (type(v).__name__ if not isinstance(v, (int, float, str, bool)) else v) for k, v in q.items()})
    except Exception as e:
        print("snapshot FAIL:", e)
    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
