"""Live check: do nearby living agents carry a decoded `name` field now?

Prints a summary: number of living agents, how many have a `name`
field populated, and samples the first few with their name, allegiance,
distance, and model. Run after injecting gwa3.dll with --llm."""

import asyncio
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                              errors="replace", line_buffering=True)

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.ipc_client import IpcClient


ALLEGIANCE_NAME = {
    0: "neutral", 1: "ally", 3: "foe", 6: "spirit",
}


async def main():
    ipc = IpcClient(os.environ["GWA3_PIPE_NAME"])
    if not await ipc.connect(timeout=30.0):
        print("CONNECT_FAIL")
        return 1
    print("connected")

    tier2 = None
    for _ in range(40):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=3.0)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            tier2 = m
            break
    ipc.disconnect()

    if not tier2:
        print("NO_TIER2")
        return 2

    map_id = tier2.get("map", {}).get("map_id")
    print(f"\n=== Nearby agents (map_id={map_id}) ===")
    agents = tier2.get("agents", [])
    living = [a for a in agents if a.get("agent_type") == "living"]
    named = [a for a in living if "name" in a and a["name"]]
    print(f"living={len(living)}, with decoded name={len(named)} "
          f"({(len(named)/len(living)*100) if living else 0:.0f}%)")

    for a in sorted(living, key=lambda x: x.get("distance", 99999))[:20]:
        alleg = ALLEGIANCE_NAME.get(a.get("allegiance", 0), str(a.get("allegiance")))
        name = a.get("name", "<no-name>")
        print(f"  id={a['id']} dist={a.get('distance', 0):.0f} "
              f"alleg={alleg} hp={a.get('hp', 0):.0%} "
              f"pn={a.get('player_number')} "
              f"name={name!r}")

    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
