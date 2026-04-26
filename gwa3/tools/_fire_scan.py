import asyncio, os, sys
os.environ["GWA3_PIPE_NAME"] = r"\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase

async def main():
    tc = BridgeTestCase()
    await tc.setUp()
    r = await tc.send_action("scan_ui_labels", {})
    print(f"scan: {r.get('success')} {r.get('error')}")
    await asyncio.sleep(3.0)
    snap = await tc.wait_for_snapshot(tier=2, timeout=8.0)
    q = snap["quests"]
    aq = q.get("active_quest") or {}
    decoded = {k: v for k, v in aq.items() if not k.endswith("_enc") and isinstance(v, str)}
    print(f"active decoded fields: {list(decoded.keys())}")
    for k, v in decoded.items():
        print(f"  {k}: {v[:120]}")
    log = q.get("quest_log", [])
    named = [e for e in log if any(k in e for k in ("name","location","npc"))]
    print(f"\nlog entries with decoded: {len(named)}/{len(log)}")
    for e in named[:13]:
        print(f"  id={e['quest_id']} name={e.get('name')!r}")
    await tc.tearDown()

asyncio.run(main())
