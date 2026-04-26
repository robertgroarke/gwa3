"""Trigger request_quest_info on the active quest, wait for the
async decoder to fill the cache, then dump the resulting snapshot
so we can see human-readable quest text."""
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
    active_id = q["active_quest_id"]
    print(f"[before] active_quest_id={active_id}  log_size={q['quest_log_size']}")
    aq = q.get("active_quest") or {}
    print("  active_quest keys:", sorted(aq.keys()))

    print(f"\n[action] request_quest_info({active_id})")
    r = await tc.send_action("request_quest_info", {"quest_id": active_id})
    print(f"  result: success={r.get('success')} error={r.get('error')}")

    # Let the worker drain. With 8 s timeout per decode + 50 ms gap,
    # 5 strings takes up to ~40 s worst case.
    deltas = (5, 10, 20, 30, 45, 60)
    for wait_s in deltas:
        await asyncio.sleep(wait_s if wait_s == deltas[0] else wait_s - deltas[deltas.index(wait_s) - 1])
        snap = await tc.wait_for_snapshot(tier=2, timeout=6.0)
        aq = snap["quests"].get("active_quest") or {}
        decoded = {k: v for k, v in aq.items() if not k.endswith("_enc") and isinstance(v, str)}
        print(f"\n[after {wait_s}s] decoded fields on active_quest: {list(decoded.keys())}")
        for k, v in decoded.items():
            snippet = v if len(v) < 160 else v[:160] + "..."
            print(f"  {k}: {snippet}")
        if decoded:
            # Also show the full log's decoded names if any arrived
            log = snap["quests"].get("quest_log", [])
            named = [e for e in log if "name" in e]
            if named:
                print("\n  quest_log entries with decoded name:")
                for e in named:
                    print(f"   - {e['quest_id']}: {e['name']}")
            break

    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
