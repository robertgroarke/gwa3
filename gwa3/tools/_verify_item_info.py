"""Force an item tooltip render so we can verify info_string decoding.

Flow:
  1. Travel to Embark Beach merchant cluster.
  2. Find Ozem (merchant NPC) in the snapshot.
  3. Open his merchant window — that causes GW to render tooltips for
     every merchant item, which triggers ValidateAsyncDecodeStr and
     populates the cache.
  4. Snapshot the merchant items and dump their info_string fields.
"""

import asyncio
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.ipc_client import IpcClient


MERCHANT_X = 2233.0
MERCHANT_Y = -2009.0


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=10.0):
        print("CONNECT_FAIL")
        return 1

    # Walk to merchant (several retries for longer trips).
    for i in range(3):
        print(f"[*] move_to merchant attempt {i+1}")
        await ipc.send_action("move_to",
                              {"x": MERCHANT_X, "y": MERCHANT_Y},
                              f"mv{i+1}")
        await asyncio.sleep(12.0)

    # Drain to a fresh tier-2 snapshot so we can find Ozem's agent_id.
    latest_t2 = None
    for _ in range(30):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=1.0)
        except asyncio.TimeoutError:
            break
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest_t2 = m

    if not latest_t2:
        print("NO_TIER2")
        ipc.disconnect()
        return 2

    ozem_id = None
    for a in latest_t2.get("agents", []):
        if a.get("agent_type") == "living" and "Ozem" in (a.get("name") or ""):
            ozem_id = a["id"]
            break
    if not ozem_id:
        print("OZEM_NOT_FOUND — walk closer or increase move retries")
        ipc.disconnect()
        return 3
    print(f"[*] Ozem agent_id={ozem_id}")

    print("[*] open_merchant")
    await ipc.send_action("open_merchant", {"agent_id": ozem_id}, "merch1")
    await asyncio.sleep(5.0)

    # Grab the freshest tier-2 snapshot with merchant open.
    latest_t2 = None
    for _ in range(20):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=1.0)
        except asyncio.TimeoutError:
            break
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest_t2 = m
    ipc.disconnect()

    if not latest_t2:
        print("NO_SECOND_TIER2")
        return 4

    merch = latest_t2.get("merchant", {})
    if not merch.get("is_open"):
        print(f"merchant is_open={merch.get('is_open')} — open_merchant may "
              f"have been rejected; item_count={merch.get('item_count')}")

    items = merch.get("items", [])
    with_name = [it for it in items if "name" in it and it["name"]]
    with_info = [it for it in items if "info_string" in it and it["info_string"]]

    print(f"\nmerchant item_count={len(items)} "
          f"with_name={len(with_name)} with_info_string={len(with_info)}")

    for it in items[:8]:
        print(f"  item_id={it.get('item_id')} model={it.get('model_id')} "
              f"name={it.get('name', '<no-name>')!r}")
        info = it.get("info_string", "")
        if info:
            # Strip control chars for readability
            info = info.replace("\u0001", " | ").replace("\u0002", " ").replace(
                "\u0104", "").replace("\u010a", "")
            print(f"    info_string={info[:160]!r}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
