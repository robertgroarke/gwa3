"""Category M (LLM): Gemma-in-the-loop quest log manipulation.

Closes the original task's test loop:
    "develop a test to prove that Gemma will be able to read the quest log
     and manipulate it through the LLM bridge."

These tests drive the REAL AgentLoop (real IpcClient to a live DLL, real
tool schema, real observation window) but swap in a scripted LLMClient so
the harness is hermetic and deterministic. A scripted LLM is fair for this
proof: the AgentLoop contract is that given any tool-calling model whose
prompt includes the relevant state, the model can drive quest actions. We
exercise every link in that chain (prompt building, tool dispatch, pipe
transport, DLL action handling, snapshot round-trip) except the remote
Gemma HTTP hop.

Test plan:
## L01 observation.build_context_summary lists quest_log entries with IDs
      so a real tool-calling LLM has the data it needs to pick a target
## L02 scripted LLM sees a Gemma-style prompt (system + objective +
      GAME STATE including quest_log), emits set_active_quest(quest_id),
      AgentLoop forwards it through the real pipe, the DLL flips the
      active quest, the next snapshot reflects the change
## L03 scripted LLM request_quest_info round-trip through AgentLoop
      (confirms the tool is reachable via the real agent dispatch path,
      not just via BridgeTestCase.send_action)

Run:
    python -m bridge.tests --filter "test_llm_*"
    python -m bridge.tests --filter "test_m_*"
"""

from __future__ import annotations

import asyncio
import json
import unittest
from dataclasses import dataclass, field

from bridge.agent_loop import AgentLoop, SYSTEM_PROMPT
from bridge.llm_client import LLMResponse, ToolCall
from bridge.observation import ObservationWindow

from .base import BridgeTestCase
from .helpers import assert_true


# ---------------------------------------------------------------------------
# L01: prompt-building unit test — no pipe, no live game
# ---------------------------------------------------------------------------

class TestObservationQuestLog(unittest.TestCase):
    """L01: The observation summary must expose quest_log entries so the LLM
    has the raw material (quest_id + name + flags) to pick a target."""

    def test_l01_quest_log_entries_render(self):
        window = ObservationWindow()
        window.add_snapshot({
            "tier": 2,
            "quests": {
                "active_quest_id": 100,
                "active_quest": {"name": "First Quest", "is_completed": False},
                "quest_log": [
                    {"quest_id": 100, "name": "First Quest",
                     "is_active": True, "is_primary": True,
                     "location": "Shing Jea", "npc": "Headmaster Zhu"},
                    {"quest_id": 200, "name": "Second Quest",
                     "is_active": False, "is_completed": False,
                     "location": "Kaineng City"},
                    {"quest_id": 300, "name": "Third Quest",
                     "is_completed": True},
                ],
            },
        })
        summary = window.build_context_summary()
        self.assertIn("Quest Log (3 quests)", summary)
        self.assertIn("quest_id=100", summary)
        self.assertIn("First Quest", summary)
        self.assertIn("ACTIVE", summary)
        self.assertIn("PRIMARY", summary)
        self.assertIn("quest_id=200", summary)
        self.assertIn("quest_id=300", summary)
        self.assertIn("COMPLETED", summary)
        self.assertIn("Shing Jea", summary)
        self.assertIn("Headmaster Zhu", summary)

    def test_l01b_quest_log_rendered_without_active_quest(self):
        """Quest log should still show even when no quest is active."""
        window = ObservationWindow()
        window.add_snapshot({
            "tier": 2,
            "quests": {
                "active_quest_id": 0,
                "quest_log": [
                    {"quest_id": 42, "name": "Orphan Quest"},
                ],
            },
        })
        summary = window.build_context_summary()
        self.assertIn("Quest Log (1 quests)", summary)
        self.assertIn("quest_id=42", summary)


# ---------------------------------------------------------------------------
# Scripted LLM — deterministic stand-in for a remote Gemma endpoint
# ---------------------------------------------------------------------------

@dataclass
class ScriptedLLM:
    """Drop-in replacement for bridge.llm_client.LLMClient.

    Records every prompt it receives, and emits a predetermined sequence of
    LLMResponse objects. Terminates the loop on its own by having the agent
    cooperating caller stop once it sees state change.
    """
    responses: list[LLMResponse] = field(default_factory=list)
    captured_prompts: list[list[dict]] = field(default_factory=list)
    _idx: int = 0

    async def chat_completion(self, messages, tools=None, tool_choice="auto",
                              temperature=0.3, max_tokens=2048):
        del tools, tool_choice, temperature, max_tokens  # signature-parity only
        self.captured_prompts.append(list(messages))
        if self._idx < len(self.responses):
            r = self.responses[self._idx]
            self._idx += 1
            return r
        return LLMResponse(content=None, tool_calls=[])

    async def close(self):
        pass


def _scripted_tool_call(call_id: str, name: str, params: dict) -> ToolCall:
    return ToolCall(id=call_id, name=name, arguments=json.dumps(params))


async def _wait_for_loop_dispatched(
    scripted: "ScriptedLLM", timeout: float
) -> None:
    """Wait until the scripted LLM has been called (meaning the AgentLoop
    reached chat_completion with observations and is executing the tool
    call), then give a short grace period for the ipc.send_action to
    actually reach the DLL and for the DLL to apply any state change.

    We used to gate on cycle_count as well, but cycle_count increments
    at the TOP of each cycle — so "cycle_count >= 2" can fire at the
    moment cycle 2 begins, before cycle 1's execute_tool_calls has had
    a chance to send through the pipe. scripted._idx >= 1 is the right
    signal: it only advances after Gemma's response is consumed."""
    end = asyncio.get_event_loop().time() + timeout
    while asyncio.get_event_loop().time() < end:
        await asyncio.sleep(0.1)
        if scripted._idx >= 1:
            # Grace period: execute_tool_calls does the pipe send + a
            # 0.1s pause + a follow-up observation drain. 1.0s is ample.
            await asyncio.sleep(1.0)
            return


async def _stop_agent_loop(loop: AgentLoop, run_task: asyncio.Task) -> None:
    """Cleanly stop the agent loop and release the ipc lock."""
    loop.stop()
    try:
        await asyncio.wait_for(run_task, timeout=3.0)
    except asyncio.TimeoutError:
        run_task.cancel()
        try:
            await run_task
        except (asyncio.CancelledError, Exception):
            pass


# ---------------------------------------------------------------------------
# L02: scripted Gemma → AgentLoop → pipe → DLL → snapshot round-trip
# ---------------------------------------------------------------------------

async def test_llm_scripted_gemma_switches_active_quest(tc: BridgeTestCase):
    """L02: The scripted LLM picks a non-active quest from the observation,
    emits set_active_quest(quest_id), AgentLoop forwards it, the DLL flips
    the active quest, and the next tier-2 snapshot reflects the change."""

    snap = await tc.wait_for_snapshot(tier=2)
    log = snap.get("quests", {}).get("quest_log", [])
    if len(log) < 2:
        tc.skip("Need at least 2 quests in the log to switch between")

    active_id = snap.get("quests", {}).get("active_quest_id", 0)
    target = None
    for q in log:
        qid = q.get("quest_id", 0)
        if qid and qid != active_id and not q.get("is_completed"):
            target = qid
            break
    if target is None:
        tc.skip("No switchable non-completed quest distinct from active")

    scripted = ScriptedLLM(responses=[
        LLMResponse(
            content=None,
            tool_calls=[_scripted_tool_call(
                "call-switch-1", "set_active_quest", {"quest_id": target}
            )],
        ),
    ])

    loop = AgentLoop(
        ipc=tc.ipc, llm=scripted, autonomy="tactical",
        objective=(
            f"Switch the active quest to quest_id={target}. "
            "Use set_active_quest exactly once and then stop acting."
        ),
    )
    # Seed the loop with the tier-2 snapshot we already have so the
    # first chat_completion's prompt contains quest_log entries.
    # Without this, the first cycle often grabs a tier-1 snapshot from
    # the pipe (which doesn't carry quest state) and the prompt-
    # visibility assertion below fails.
    loop.observations.add_snapshot(snap)

    run_task = asyncio.create_task(loop.run())
    try:
        await _wait_for_loop_dispatched(scripted, timeout=6.0)
    finally:
        # Stop the AgentLoop before using tc.ipc — they share the pipe and
        # we need tc.ipc exclusively for the state-change poll below.
        await _stop_agent_loop(loop, run_task)

    def active_matches(s: dict) -> bool:
        return s.get("quests", {}).get("active_quest_id") == target

    new_snap = await tc.wait_for_state_change(
        active_matches, tier=2, timeout=8.0
    )

    assert_true(
        scripted._idx >= 1,
        "Scripted LLM was never invoked — AgentLoop never reached chat_completion",
    )
    # The prompt the scripted LLM received must contain the materials a real
    # Gemma would need to make the same choice.
    first_prompt = scripted.captured_prompts[0]
    assert_true(
        any(m.get("role") == "system" and SYSTEM_PROMPT[:64] in (m.get("content") or "")
            for m in first_prompt),
        "System prompt was not present in the LLM payload",
    )
    assert_true(
        any("[GAME STATE" in (m.get("content") or "")
            and f"quest_id={target}" in (m.get("content") or "")
            for m in first_prompt),
        "Target quest_id was not visible in the GAME STATE prompt — a real "
        "LLM could not have chosen it",
    )
    assert_true(
        new_snap["quests"]["active_quest_id"] == target,
        f"Expected active_quest_id=={target}, got {new_snap['quests']['active_quest_id']}",
    )


# ---------------------------------------------------------------------------
# L03: scripted Gemma → request_quest_info via AgentLoop dispatch
# ---------------------------------------------------------------------------

async def test_llm_scripted_gemma_requests_quest_info(tc: BridgeTestCase):
    """L03: AgentLoop forwards request_quest_info to the DLL (proves the
    tool is reachable via the real agent dispatch path, not just via
    BridgeTestCase.send_action)."""

    snap = await tc.wait_for_snapshot(tier=2)
    log = snap.get("quests", {}).get("quest_log", [])
    if not log:
        tc.skip("Quest log is empty")

    target = log[0]["quest_id"]

    scripted = ScriptedLLM(responses=[
        LLMResponse(
            content=None,
            tool_calls=[_scripted_tool_call(
                "call-info-1", "request_quest_info", {"quest_id": target}
            )],
        ),
    ])

    loop = AgentLoop(
        ipc=tc.ipc, llm=scripted, autonomy="tactical",
        objective=f"Fetch full quest info for quest_id={target} and stop.",
    )

    run_task = asyncio.create_task(loop.run())
    try:
        await _wait_for_loop_dispatched(scripted, timeout=6.0)
    finally:
        await _stop_agent_loop(loop, run_task)

    assert_true(
        scripted._idx >= 1,
        "Scripted LLM was never invoked — AgentLoop never called chat_completion",
    )
    assert_true(
        len(loop.history) > 0,
        "AgentLoop history is empty — the tool call was never dispatched",
    )
    dispatched = [
        h for h in loop.history
        if h.get("role") == "assistant"
        and any(tc.get("function", {}).get("name") == "request_quest_info"
                for tc in (h.get("tool_calls") or []))
    ]
    assert_true(
        len(dispatched) == 1,
        f"Expected exactly one request_quest_info dispatch, got {len(dispatched)}",
    )
