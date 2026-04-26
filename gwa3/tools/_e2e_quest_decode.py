"""End-to-end: open Quest Log, wait, scan UI labels, dump snapshot to
see if decoded quest names show up."""
import asyncio, json, os, sys
os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.base import BridgeTestCase


async def main():
    tc = BridgeTestCase()
    await tc.setUp()

    print("[1/4] open_quest_log")
    r = await tc.send_action("open_quest_log", {})
    print(f"  -> success={r.get('success')} error={r.get('error')}")
    await asyncio.sleep(4.0)  # let GW render the Quest Log panel

    print("[2/4] scan_ui_labels")
    r = await tc.send_action("scan_ui_labels", {})
    print(f"  -> success={r.get('success')} error={r.get('error')}")
    await asyncio.sleep(3.0)

    print("[3/4] snapshot tier-2")
    snap = await tc.wait_for_snapshot(tier=2, timeout=8.0)
    q = snap["quests"]
    aq = q.get("active_quest") or {}
    log = q.get("quest_log", [])

    print(f"\nactive_quest_id: {q.get('active_quest_id')}  log_size={len(log)}")

    decoded_active = {k: v for k, v in aq.items()
                      if not k.endswith("_enc") and isinstance(v, str)}
    print(f"\nactive_quest decoded fields: {list(decoded_active.keys())}")
    for k, v in decoded_active.items():
        snippet = v if len(v) <= 160 else v[:160] + "..."
        print(f"  {k}: {snippet}")

    print(f"\n[4/4] quest_log decoded rollup:")
    n_decoded = 0
    for e in log:
        for k in ("name", "location", "npc"):
            if k in e:
                n_decoded += 1
                print(f"  quest_id={e['quest_id']} {k}: {e[k]}")
    print(f"\ntotal decoded fields found: {n_decoded}")
    await tc.tearDown()


if __name__ == "__main__":
    asyncio.run(main())
