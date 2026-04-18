"""Instrumented flake repro. Monkey-patches AgentLoop + IpcClient to
trace every major event so we can see exactly where the pipe/observer
gets stuck in the 14.9s failing L02 runs.
"""

import asyncio
import os
import sys
import time

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge import agent_loop, ipc_client
from bridge.tests.runner import run_single_test
from bridge.tests import test_m_quest_log_llm as m


T0 = time.monotonic()


def stamp() -> str:
    return f"[{time.monotonic() - T0:6.2f}s]"


# --- Patch IpcClient ---
_orig_connect = ipc_client.IpcClient.connect
_orig_disconnect = ipc_client.IpcClient.disconnect
_orig_read = ipc_client.IpcClient.read_message
_orig_send = ipc_client.IpcClient.send_action


async def traced_connect(self, timeout=30.0):
    print(f"{stamp()} IPC.connect start")
    r = await _orig_connect(self, timeout)
    print(f"{stamp()} IPC.connect -> {r}")
    return r


def traced_disconnect(self):
    print(f"{stamp()} IPC.disconnect")
    return _orig_disconnect(self)


async def traced_read(self):
    r = await _orig_read(self)
    mtype = r.get("type") if r else None
    tier = r.get("tier") if r else None
    if mtype == "snapshot":
        print(f"{stamp()} IPC.read -> snapshot tier={tier}")
    else:
        print(f"{stamp()} IPC.read -> {mtype}")
    return r


async def traced_send(self, action_name, params=None, request_id=""):
    print(f"{stamp()} IPC.send_action {action_name} id={request_id}")
    r = await _orig_send(self, action_name, params, request_id)
    print(f"{stamp()} IPC.send_action {action_name} returned")
    return r


ipc_client.IpcClient.connect = traced_connect
ipc_client.IpcClient.disconnect = traced_disconnect
ipc_client.IpcClient.read_message = traced_read
ipc_client.IpcClient.send_action = traced_send


# --- Patch AgentLoop ---
_orig_agent_run = agent_loop.AgentLoop.run


async def traced_run(self):
    print(f"{stamp()} AgentLoop.run start cycle=0")
    await _orig_agent_run(self)
    print(f"{stamp()} AgentLoop.run exit")


agent_loop.AgentLoop.run = traced_run


# --- Also peek observations + cycle count periodically ---
_orig_collect = agent_loop.AgentLoop._collect_observations_safe


async def traced_collect(self):
    before_latest = self.observations.latest is not None
    await _orig_collect(self)
    after_latest = self.observations.latest is not None
    if after_latest and not before_latest:
        print(f"{stamp()} AgentLoop obs.latest populated cycle={self._cycle_count}")


agent_loop.AgentLoop._collect_observations_safe = traced_collect


# --- Run the known flake sequence ---
L02 = ("test_llm_scripted_gemma_switches_active_quest",
       m.test_llm_scripted_gemma_switches_active_quest)


async def main():
    # Run L02 twice back-to-back — first should pass, second often fails.
    for i in range(2):
        print(f"\n{stamp()} ===== RUN {i+1}: L02 =====")
        status, detail, elapsed = await run_single_test(
            L02[0], L02[1], timeout=30.0
        )
        print(f"{stamp()} RUN {i+1} result: {status} ({elapsed:.1f}s) {detail}")


if __name__ == "__main__":
    asyncio.run(main())
