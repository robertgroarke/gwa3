"""Open Ozem's merchant window and dump item info_string fields."""

import asyncio
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.ipc_client import IpcClient


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=10.0):
        print("CONNECT_FAIL")
        return 1

    # Find Ozem's agent_id in the freshest tier-2 snapshot.
    latest = None
    for _ in range(15):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=1.0)
        except asyncio.TimeoutError:
            break
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest = m

    ozem = None
    for a in (latest or {}).get("agents", []):
        if "Ozem" in (a.get("name") or ""):
            ozem = a
            break
    if not ozem:
        print("Ozem not in current snapshot")
        ipc.disconnect()
        return 2
    print(f"[*] Ozem id={ozem['id']} dist={ozem.get('distance', 0):.0f}")

    await ipc.send_action("open_merchant", {"agent_id": ozem["id"]}, "m1")
    await asyncio.sleep(6.0)

    # Drain the next tier-2 snapshot with merchant open.
    snap = None
    for _ in range(20):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=1.0)
        except asyncio.TimeoutError:
            break
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            snap = m
    ipc.disconnect()
    if not snap:
        print("No tier-2 after open")
        return 3

    merch = snap.get("merchant", {})
    print(f"\nmerchant is_open={merch.get('is_open')} items={merch.get('item_count', 0)}")

    items = merch.get("items", [])
    with_name = sum(1 for it in items if it.get("name"))
    with_full = sum(1 for it in items if it.get("full_name"))
    with_info = sum(1 for it in items if it.get("info_string"))
    print(f"named={with_name} full_named={with_full} info_string={with_info}")

    for it in items[:10]:
        nm = it.get("name", "<no-name>")
        info = it.get("info_string") or ""
        # Clean up GW's separator glyphs for readability.
        info = (info.replace("\u0001", " | ")
                    .replace("\u0002", " ")
                    .strip())
        print(f"  item={it.get('item_id')} model={it.get('model_id')} "
              f"value={it.get('value')} name={nm!r}")
        if info:
            print(f"    info={info[:180]!r}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
