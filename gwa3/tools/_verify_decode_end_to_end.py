"""End-to-end decode verification. Sends open_quest_log, waits, then
dumps quest_log entries to show decoded names landed."""

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

    print("[*] Sending open_quest_log...")
    await ipc.send_action("open_quest_log", {}, "qlo1")

    # Drain for 8s so GW has time to render the quest log UI
    # (which triggers ValidateAsyncDecodeStr for every visible label).
    latest_t2 = None
    for _ in range(40):
        try:
            m = await asyncio.wait_for(ipc.read_message(), timeout=0.5)
        except asyncio.TimeoutError:
            continue
        if m and m.get("type") == "snapshot" and m.get("tier") == 2:
            latest_t2 = m

    ipc.disconnect()

    if not latest_t2:
        print("NO_TIER2")
        return 2

    q = latest_t2.get("quests", {}) or {}
    log = q.get("quest_log", [])
    aq = q.get("active_quest", {}) or {}

    print(f"\n=== After open_quest_log ===")
    print(f"active_quest_id={q.get('active_quest_id')}")
    print(f"active_quest decoded fields:")
    for key in ("name", "location", "npc", "description", "objectives"):
        v = aq.get(key)
        if v:
            print(f"  {key}: {v[:160]}")

    decoded_count = sum(1 for e in log if "name" in e and e["name"])
    print(f"\nQuest log ({len(log)} entries, {decoded_count} decoded):")
    for e in log[:13]:
        name = e.get("name", "<still-enc-only>")
        loc = e.get("location", "")
        npc = e.get("npc", "")
        print(f"  quest_id={e.get('quest_id')} "
              f"active={e.get('is_active')} "
              f"done={e.get('is_completed')} "
              f"name={name!r}")
        if loc:
            print(f"      location: {loc}")
        if npc:
            print(f"      npc: {npc}")

    return 0 if decoded_count > 0 else 3


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
