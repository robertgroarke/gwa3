import tempfile
import unittest
from pathlib import Path

from bridge.agent_loop import AgentLoop
from bridge.llm_client import LLMResponse
from bridge.run_summary_memory import RunSummaryMemory


def _snapshot(map_id=638, *, free_slots=12, party_defeated=False):
    return {
        "map": {"map_id": map_id, "loading_state": 1},
        "inventory": {
            "free_slots_total": free_slots,
            "gold_character": 5000,
            "froggy_maintenance": {
                "stored_consets_total": 42,
                "loose_consets_inventory_total": 0,
            },
        },
        "party": {"is_defeated": party_defeated, "dead_count": 8 if party_defeated else 0},
        "agents": [
            {"id": 123, "agent_type": "gadget", "is_chest": True, "distance": 90},
        ],
    }


class FakeSummaryLlm:
    def __init__(self, content="Dungeon summary from model."):
        self.content = content
        self.calls = []

    async def chat_completion(self, messages, **kwargs):
        self.calls.append((messages, kwargs))
        return LLMResponse(content=self.content)


class RunSummaryMemoryTests(unittest.TestCase):
    def test_memory_persists_last_twenty_summaries(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "summaries.jsonl"
            memory = RunSummaryMemory(path=path)
            for idx in range(25):
                memory.add(f"summary {idx}", {"event_type": "test"})

            reloaded = RunSummaryMemory(path=path)

            self.assertEqual(len(reloaded.entries), 20)
            self.assertEqual(reloaded.entries[0]["summary"], "summary 5")
            self.assertIn("summary 24", reloaded.format_for_prompt())

    def test_agent_prompt_includes_persisted_summaries(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "summaries.jsonl"
            RunSummaryMemory(path=path).add("Maintenance completed cleanly.", {
                "event_type": "maintenance_completed",
            })
            agent = AgentLoop(ipc=None, llm=None, objective="farm", run_summary_path=path)

            prompt = "\n".join(m["content"] for m in agent._build_messages())

            self.assertIn("[RUN SUMMARY MEMORY]", prompt)
            self.assertIn("Maintenance completed cleanly.", prompt)


class AgentLoopRunSummaryTests(unittest.IsolatedAsyncioTestCase):
    async def test_completed_dungeon_run_forces_short_summary_turn(self):
        with tempfile.TemporaryDirectory() as tmp:
            llm = FakeSummaryLlm("Completed Bogroot, reward claimed, refresh inventory.")
            agent = AgentLoop(
                ipc=None,
                llm=llm,
                objective="Autonomously farm Froggy HM / Bogroot Growths in a loop",
                run_summary_path=Path(tmp) / "summaries.jsonl",
            )
            agent.observations.add_snapshot(_snapshot(map_id=638, free_slots=3))
            enriched = agent._enrich_tool_result("froggy_run_dungeon_loop", {"success": True})

            await agent._record_run_summary_if_needed("froggy_run_dungeon_loop", {}, enriched)

            self.assertEqual(len(llm.calls), 1)
            self.assertEqual(agent.run_summaries.entries[0]["event_type"], "dungeon_run_completed")
            self.assertIn("Completed Bogroot", agent.run_summaries.format_for_prompt())
            self.assertIn("RUN SUMMARY RECORDED", agent.history[-1]["content"])

    async def test_maintenance_completion_records_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            agent = AgentLoop(
                ipc=None,
                llm=None,
                objective="Autonomously farm Froggy HM / Bogroot Growths in a loop",
                run_summary_path=Path(tmp) / "summaries.jsonl",
            )
            agent.observations.add_snapshot(_snapshot(map_id=638))
            enriched = agent._enrich_tool_result("froggy_run_full_maintenance", {"success": True})

            await agent._record_run_summary_if_needed("froggy_run_full_maintenance", {}, enriched)

            self.assertEqual(agent.run_summaries.entries[0]["event_type"], "maintenance_completed")
            self.assertIn("Town maintenance completed", agent.run_summaries.entries[0]["summary"])

    async def test_chest_and_party_return_signals_are_supported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "summaries.jsonl"
            agent = AgentLoop(ipc=None, llm=None, objective="generic farming", run_summary_path=path)
            agent.observations.add_snapshot(_snapshot(map_id=615))

            await agent._record_run_summary_if_needed(
                "interact_signpost",
                {"agent_id": 123},
                {"success": True},
            )
            agent.observations.add_snapshot(_snapshot(map_id=615, party_defeated=True))
            await agent._record_run_summary_if_needed(
                "return_to_outpost",
                {},
                {"success": True},
            )

            event_types = [entry["event_type"] for entry in agent.run_summaries.entries]
            self.assertEqual(event_types, [
                "chest_interaction_completed",
                "party_defeated_returned_to_outpost",
            ])


if __name__ == "__main__":
    unittest.main()
