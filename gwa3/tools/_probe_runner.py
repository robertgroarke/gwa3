"""Simulate exactly what the bridge test runner does for a single test."""
import asyncio
import os
import sys
import time
import traceback

os.environ.setdefault("GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm_biscuit")

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase
from bridge.tests.test_m_quest_log import test_set_active_quest_missing_id

TIMEOUT = 30.0


async def main():
    start = time.monotonic()
    try:
        tc = BridgeTestCase()
        t0 = time.monotonic()
        print("[0.0s] setUp start")
        await asyncio.wait_for(tc.setUp(), timeout=TIMEOUT)
        t1 = time.monotonic()
        print(f"[{t1-t0:.2f}s] setUp done")
        await asyncio.wait_for(test_set_active_quest_missing_id(tc), timeout=TIMEOUT)
        t2 = time.monotonic()
        print(f"[{t2-t1:.2f}s] test body done")
        await asyncio.wait_for(tc.tearDown(), timeout=5.0)
        print(f"[{time.monotonic()-t2:.2f}s] tearDown done")
        print("PASS")
    except asyncio.TimeoutError:
        print(f"TIMEOUT after {time.monotonic()-start:.2f}s")
    except Exception as e:
        print(f"FAIL: {type(e).__name__}: {e}")
        traceback.print_exc()


if __name__ == "__main__":
    asyncio.run(main())
