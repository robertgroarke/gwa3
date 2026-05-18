"""Offline tests for Froggy autonomous bridge recommendations."""

from __future__ import annotations

import time
import unittest

from bridge.agent_loop import AgentLoop


def _loop() -> AgentLoop:
    return AgentLoop(
        ipc=None,
        llm=None,
        objective="Autonomously farm Froggy HM / Bogroot Growths in a loop",
    )


def _snapshot(
    map_id: int,
    *,
    loading_state: int = 1,
    free_slots: int | None = 20,
    gold_character: int = 5000,
    gold_storage: int = 0,
    conset_material_stacks: int = 0,
    loose_consets: int = 0,
    stored_consets: int = 100,
    grail_inventory: int = 1,
    essence_inventory: int = 1,
    armor_inventory: int = 1,
    stored_grail: int = 50,
    stored_essence: int = 50,
    stored_armor: int = 50,
) -> dict:
    inv = {
        "free_slots_total": free_slots,
        "gold_character": gold_character,
        "gold_storage": gold_storage,
        "froggy_maintenance": {
            "conset_material_stacks_inventory": conset_material_stacks,
            "loose_consets_inventory_total": loose_consets,
            "grail_inventory": grail_inventory,
            "essence_inventory": essence_inventory,
            "armor_inventory": armor_inventory,
            "stored_consets_total": stored_consets,
            "stored_grail": stored_grail,
            "stored_essence": stored_essence,
            "stored_armor": stored_armor,
        },
    }
    if free_slots is None:
        inv.pop("free_slots_total")
    return {
        "map": {
            "map_id": map_id,
            "loading_state": loading_state,
        },
        "inventory": inv,
    }


class FroggyRecommendationTests(unittest.TestCase):
    def test_recommends_maintenance_for_low_inventory_in_gadds(self):
        agent = _loop()
        agent.observations.add_snapshot(_snapshot(638, free_slots=4))

        rec = agent._recommend_froggy_action()

        self.assertEqual(rec["recommended_next_action"], "froggy_run_full_maintenance")
        self.assertEqual(rec["reason"], "low_inventory_space_in_gadds")

    def test_recent_maintenance_suppresses_loose_conset_repeat_with_acceptable_slots(self):
        agent = _loop()
        agent._last_froggy_full_maintenance_success_time = time.monotonic()
        agent.observations.add_snapshot(_snapshot(638, free_slots=3, loose_consets=2))

        rec = agent._recommend_froggy_action()

        self.assertEqual(rec["recommended_next_action"], "froggy_run_town_setup")
        self.assertEqual(rec["reason"], "recent_maintenance_slots_acceptable")

    def test_recommends_maintenance_for_conset_material_pressure(self):
        agent = _loop()
        agent.observations.add_snapshot(
            _snapshot(638, free_slots=8, conset_material_stacks=12)
        )

        rec = agent._recommend_froggy_action()

        self.assertEqual(rec["recommended_next_action"], "froggy_run_full_maintenance")
        self.assertEqual(rec["reason"], "conset_material_pressure_in_gadds")

    def test_recommends_maintenance_for_gold_pressure(self):
        agent = _loop()
        agent.observations.add_snapshot(_snapshot(638, gold_character=95000))

        rec = agent._recommend_froggy_action()

        self.assertEqual(rec["recommended_next_action"], "froggy_run_full_maintenance")
        self.assertEqual(rec["reason"], "character_gold_near_cap")

    def test_recommends_maintenance_when_stored_consets_low_and_gold_available(self):
        agent = _loop()
        agent.observations.add_snapshot(
            _snapshot(638, stored_consets=12, gold_storage=900000)
        )

        rec = agent._recommend_froggy_action()

        self.assertEqual(rec["recommended_next_action"], "froggy_run_full_maintenance")
        self.assertEqual(rec["reason"], "stored_consets_low_with_gold_available")

    def test_recommends_maintenance_when_character_conset_set_incomplete(self):
        agent = _loop()
        agent._last_froggy_full_maintenance_success_time = time.monotonic()
        agent.observations.add_snapshot(_snapshot(638, armor_inventory=0, free_slots=5))

        rec = agent._recommend_froggy_action()

        self.assertEqual(rec["recommended_next_action"], "froggy_run_full_maintenance")
        self.assertEqual(rec["reason"], "character_conset_set_incomplete")

    def test_recommends_town_setup_after_recent_maintenance_with_acceptable_slots(self):
        agent = _loop()
        agent._last_froggy_full_maintenance_success_time = time.monotonic()
        agent.observations.add_snapshot(_snapshot(638, free_slots=4))

        rec = agent._recommend_froggy_action()

        self.assertEqual(rec["recommended_next_action"], "froggy_run_town_setup")
        self.assertEqual(rec["reason"], "recent_maintenance_slots_acceptable")

    def test_recommends_segmented_entry_in_sparkfly_and_loop_in_bogroot(self):
        agent = _loop()
        agent.observations.add_snapshot(_snapshot(558))
        rec = agent._recommend_froggy_action()
        self.assertEqual(rec["recommended_next_action"], "froggy_run_sparkfly_route_to_tekks")

        for map_id in (615, 616):
            agent.observations.add_snapshot(_snapshot(map_id))
            rec = agent._recommend_froggy_action()
            self.assertEqual(rec["recommended_next_action"], "froggy_run_dungeon_loop")

    def test_enriched_completed_run_recommends_maintenance_when_slots_low(self):
        agent = _loop()
        agent.observations.add_snapshot(_snapshot(638, free_slots=2))

        result = agent._enrich_tool_result("froggy_run_dungeon_loop", {"success": True})

        self.assertTrue(result["completed_dungeon_run"])
        self.assertEqual(result["recommended_next_action"], "froggy_run_full_maintenance")
        self.assertEqual(result["reason"], "completed_run_low_inventory_space")


if __name__ == "__main__":
    unittest.main()
