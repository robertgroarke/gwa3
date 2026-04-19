"""Dump me.equipment and heroes[i].equipment from a tier-2 snapshot."""

import asyncio
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.ipc_client import IpcClient


def _dump_equipment(label: str, eq: dict):
    slots = {k: v for k, v in (eq or {}).items() if isinstance(v, dict)}
    if not slots:
        print(f"  {label}: <no equipment>")
        return
    print(f"  {label}: {len(slots)} slot(s)")
    for slot, it in slots.items():
        nm = it.get("name", "<no-name>")
        full = it.get("full_name")
        info = (it.get("info_string") or "").replace("\u0001", " | ")
        print(f"    {slot}: item_id={it.get('item_id')} model={it.get('model_id')} "
              f"name={nm!r}")
        if full and full != nm:
            print(f"      full_name={full!r}")
        if info:
            print(f"      info={info[:160]!r}")


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=10.0):
        print("CONNECT_FAIL")
        return 1

    tier2 = None
    for _ in range(30):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=2.0)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            tier2 = m
            break
    ipc.disconnect()

    if not tier2:
        print("NO_TIER2")
        return 2

    me = tier2.get("me", {})
    heroes = tier2.get("heroes", [])
    print(f"map={tier2.get('map', {}).get('map_id')} "
          f"me.agent_id={me.get('agent_id')} "
          f"pos=({me.get('x', 0):.0f}, {me.get('y', 0):.0f})")
    print(f"\n[me]")
    _dump_equipment("me.equipment", me.get("equipment", {}))

    print(f"\n[heroes] count={len(heroes)}")
    for i, h in enumerate(heroes):
        print(f"  hero {i} agent_id={h.get('agent_id')} "
              f"prof={h.get('primary')}/{h.get('secondary')}")
        _dump_equipment(f"hero[{i}].equipment", h.get("equipment", {}))

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
