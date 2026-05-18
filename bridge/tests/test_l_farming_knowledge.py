"""Category L: farming_knowledge + dungeon_runs — unit tests (no game pipe).

Validates the static knowledge tables that Gemma queries via the
get_recipe / get_outpost_info / get_material_info / get_dungeon_info /
get_blessing_info / get_hero_build / get_quest_info tools. Runs without
any game injection or IPC — purely Python module tests.

Run with:  python -m unittest bridge.tests.test_l_farming_knowledge -v

## Test plan — strict pass/fail criteria

## Lookup behaviour
## L01: get_recipe known ID      -- 24861 returns Grail of Might with all fields
## L02: get_recipe unknown ID    -- returns error + known_consumable_model_ids
## L03: get_outpost known        -- 857 returns Embark Beach full payload
## L04: get_outpost partial      -- map with model_id but no coords returns partial
## L05: get_outpost unknown      -- returns error + known_outpost_map_ids
## L06: get_material known       -- 921 returns "Bone"
## L07: get_material unknown     -- returns error
## L08: get_blessing known       -- "dwarven" returns dialog_codes + effect_ids
## L09: get_blessing case insen  -- "DWARVEN" matches "dwarven"
## L10: get_blessing unknown     -- returns error + known_blessings
## L11: get_hero known           -- "Gwen" returns hero_id + skillbar
## L12: get_hero case insen      -- "gwen" matches "Gwen"
## L13: get_hero unknown         -- returns error + known_heroes
## L14: get_quest by name        -- "Tekks's War" returns Tekks data
## L15: get_quest by id int      -- 825 resolves same payload
## L16: get_quest by id str      -- "825" as string also resolves
## L17: get_quest unknown        -- returns error
## L18: get_dungeon known        -- "Bogroot Growths" returns entry + levels
## L19: get_dungeon case insen   -- "bogroot growths" matches
## L20: get_dungeon apostrophe   -- "Arachnis Haunt" matches "Arachni's Haunt"
## L21: get_dungeon unknown      -- returns error + known_dungeons

## Internal consistency — catches schema drift
## L30: recipes have required fields
## L31: recipe crafter_outpost_map_id resolves via get_outpost_info
## L32: recipe materials resolve via get_material_info
## L33: recipe gold_cost > 0
## L34: outposts have `name` and at least one of material_trader/merchant/crafters
## L35: dungeon entry_outpost.map_id is valid
## L36: dungeon quest giver_map_id resolves via MAP_NAMES
## L37: dungeon prep.hero_team entries all resolve via get_hero_build
## L38: dungeon prep.blessings_before_entry entries resolve via get_blessing_info
## L39: dungeon level map_ids are distinct ints
## L40: dungeon level key_points have valid `kind`
## L41: dungeon end_chest exists in at least one level

## Tool schema integration
## L50: all 7 new tool schemas present in ALL_TOOLS
## L51: tool names match dispatch handler names in agent_loop

## Agent-loop dispatch (mocked IPC)
## L60: get_recipe routed locally, ipc.send_action NOT called
## L61: get_outpost_info routed locally
## L62: get_material_info routed locally
## L63: get_dungeon_info routed locally
## L64: get_blessing_info routed locally
## L65: get_hero_build routed locally
## L66: get_quest_info routed locally
## L67: unknown tool name still forwarded to pipe (regression guard)
"""

import asyncio
import datetime as dt
import httpx
import json
import time
import unittest
from unittest.mock import AsyncMock, MagicMock

from bridge import farming_knowledge as fk
from bridge import dungeon_runs
from bridge import tool_schema as ts


# ---------------------------------------------------------------------------
# Lookup behaviour
# ---------------------------------------------------------------------------

class TestLookupBehaviour(unittest.TestCase):

    # --- get_recipe ---
    def test_L01_recipe_known(self):
        res = fk.get_recipe(24861)
        self.assertTrue(res.get("success"))
        r = res["recipe"]
        for key in ("name", "crafter_name", "crafter_outpost_map_id",
                    "crafter_x", "crafter_y", "gold_cost", "materials"):
            self.assertIn(key, r, f"recipe missing field {key}")
        self.assertEqual(r["name"], "Grail of Might")

    def test_L02_recipe_unknown(self):
        res = fk.get_recipe(9999999)
        self.assertEqual(res.get("error"), "unknown_consumable")
        self.assertIn("known_consumable_model_ids", res)
        self.assertGreater(len(res["known_consumable_model_ids"]), 0)

    # --- get_outpost_info ---
    def test_L03_outpost_known(self):
        res = fk.get_outpost_info(857)
        self.assertTrue(res.get("success"))
        info = res["info"]
        self.assertEqual(info["name"], "Embark Beach")
        self.assertIn("material_trader", info)
        self.assertIn("xunlai_chest", info)
        self.assertGreaterEqual(len(info["crafters"]), 3)

    def test_L04_outpost_partial(self):
        # Map 4 is a Prophet guild hall — has a trader model_id but no full
        # OUTPOST_NPCS entry. Should return partial=True.
        res = fk.get_outpost_info(4)
        self.assertTrue(res.get("success"))
        self.assertTrue(res.get("partial"))
        self.assertIn("material_trader_npc_model_id", res["info"])

    def test_L05_outpost_unknown(self):
        res = fk.get_outpost_info(99999)
        self.assertEqual(res.get("error"), "unknown_outpost")
        self.assertIn("known_outpost_map_ids", res)

    # --- get_material_info ---
    def test_L06_material_known(self):
        res = fk.get_material_info(921)
        self.assertTrue(res.get("success"))
        self.assertEqual(res["name"], "Bone")

    def test_L07_material_unknown(self):
        res = fk.get_material_info(0)
        self.assertEqual(res.get("error"), "unknown_material")

    # --- get_blessing_info ---
    def test_L08_blessing_known(self):
        res = fk.get_blessing_info("dwarven")
        self.assertTrue(res.get("success"))
        info = res["info"]
        self.assertIn("dialog_codes", info)
        self.assertGreater(len(info["effect_ids"]), 0)

    def test_L09_blessing_case_insensitive(self):
        upper = fk.get_blessing_info("DWARVEN")
        lower = fk.get_blessing_info("dwarven")
        self.assertEqual(upper, lower)

    def test_L10_blessing_unknown(self):
        res = fk.get_blessing_info("moonlight")
        self.assertEqual(res.get("error"), "unknown_blessing")
        self.assertIn("known_blessings", res)

    # --- get_hero_build ---
    def test_L11_hero_known(self):
        res = fk.get_hero_build("Gwen")
        self.assertTrue(res.get("success"))
        b = res["build"]
        self.assertEqual(b["hero_id"], 24)
        self.assertIn("skillbar_template", b)

    def test_L12_hero_case_insensitive(self):
        upper = fk.get_hero_build("GWEN")
        lower = fk.get_hero_build("gwen")
        self.assertEqual(upper["build"], lower["build"])

    def test_L13_hero_unknown(self):
        res = fk.get_hero_build("Ragnar Lothbrok")
        self.assertEqual(res.get("error"), "unknown_hero")
        self.assertIn("known_heroes", res)

    # --- get_quest_info ---
    def test_L14_quest_by_name(self):
        res = fk.get_quest_info("Tekks's War")
        self.assertTrue(res.get("success"))
        self.assertEqual(res["info"]["quest_id"], 825)

    def test_L15_quest_by_id_int(self):
        res = fk.get_quest_info(825)
        self.assertTrue(res.get("success"))
        self.assertEqual(res["info"]["giver_npc_name"], "Tekks")

    def test_L16_quest_by_id_string(self):
        res = fk.get_quest_info("825")
        self.assertTrue(res.get("success"))
        self.assertEqual(res["info"]["giver_npc_name"], "Tekks")

    def test_L17_quest_unknown(self):
        res = fk.get_quest_info("Nope")
        self.assertEqual(res.get("error"), "unknown_quest")

    # --- get_dungeon_info ---
    def test_L18_dungeon_known(self):
        res = fk.get_dungeon_info("Bogroot Growths")
        self.assertTrue(res.get("success"))
        self.assertIn("entry_outpost", res)
        self.assertIn("levels", res)
        self.assertGreaterEqual(len(res["levels"]), 1)

    def test_L19_dungeon_case_insensitive(self):
        res = fk.get_dungeon_info("bogroot growths")
        self.assertTrue(res.get("success"))
        self.assertEqual(res["name"], "Bogroot Growths")

    def test_L20_dungeon_apostrophe_insensitive(self):
        a = fk.get_dungeon_info("Arachni's Haunt")
        b = fk.get_dungeon_info("Arachnis Haunt")
        self.assertTrue(a.get("success") and b.get("success"))
        self.assertEqual(a["name"], b["name"])

    def test_L21_dungeon_unknown(self):
        res = fk.get_dungeon_info("Cave of Nope")
        self.assertEqual(res.get("error"), "unknown_dungeon")
        self.assertIn("known_dungeons", res)


# ---------------------------------------------------------------------------
# Internal consistency
# ---------------------------------------------------------------------------

class TestDataConsistency(unittest.TestCase):
    """Catches drift between tables so Gemma never gets a dangling reference."""

    def test_L30_recipes_required_fields(self):
        required = {"name", "crafter_name", "crafter_outpost_map_id",
                    "crafter_x", "crafter_y", "gold_cost", "materials",
                    "effect_id"}
        for model_id, r in fk.CONSUMABLE_RECIPES.items():
            missing = required - set(r.keys())
            self.assertFalse(missing,
                             f"recipe {model_id} missing fields: {missing}")

    def test_L31_recipe_outpost_map_resolves(self):
        for model_id, r in fk.CONSUMABLE_RECIPES.items():
            map_id = r["crafter_outpost_map_id"]
            self.assertIn(map_id, fk.OUTPOST_NPCS,
                          f"recipe {model_id} points to unknown outpost {map_id}")

    def test_L32_recipe_materials_resolve(self):
        for model_id, r in fk.CONSUMABLE_RECIPES.items():
            for mat in r["materials"]:
                self.assertIn(mat["model_id"], fk.MATERIAL_NAMES,
                              f"recipe {model_id} references unknown material "
                              f"{mat['model_id']}")

    def test_L33_recipe_gold_cost_positive(self):
        for model_id, r in fk.CONSUMABLE_RECIPES.items():
            self.assertGreater(r["gold_cost"], 0)

    def test_L34_outposts_have_core_fields(self):
        for map_id, info in fk.OUTPOST_NPCS.items():
            self.assertIn("name", info, f"outpost {map_id} missing name")
            has_usable = any(k in info for k in
                             ("material_trader", "merchant", "crafters",
                              "xunlai_chest"))
            self.assertTrue(has_usable,
                            f"outpost {map_id} has no tradable NPCs")

    def test_L35_dungeon_entry_outpost_valid(self):
        for name, d in dungeon_runs.DUNGEON_RUNS.items():
            eo = d.get("entry_outpost")
            self.assertIsNotNone(eo, f"dungeon {name} missing entry_outpost")
            self.assertIsInstance(eo.get("map_id"), int,
                                  f"dungeon {name} entry_outpost.map_id "
                                  f"not int")

    def test_L36_dungeon_quest_map_resolves(self):
        for name, d in dungeon_runs.DUNGEON_RUNS.items():
            quest = d.get("quest") or {}
            map_id = quest.get("giver_map_id")
            if map_id is None:
                continue
            self.assertIn(map_id, fk.MAP_NAMES,
                          f"dungeon {name} quest map {map_id} not in MAP_NAMES")

    def test_L37_dungeon_heroes_resolve(self):
        for name, d in dungeon_runs.DUNGEON_RUNS.items():
            prep = d.get("prep") or {}
            for hero in prep.get("hero_team", []):
                hn = hero.get("name")
                if hn is None:
                    continue
                res = fk.get_hero_build(hn)
                self.assertTrue(res.get("success"),
                                f"dungeon {name} references unknown hero {hn}")

    def test_L38_dungeon_blessings_resolve(self):
        for name, d in dungeon_runs.DUNGEON_RUNS.items():
            prep = d.get("prep") or {}
            for blessing in prep.get("blessings_before_entry", []):
                res = fk.get_blessing_info(blessing)
                self.assertTrue(res.get("success"),
                                f"dungeon {name} references unknown blessing "
                                f"{blessing!r}")

    def test_L39_dungeon_level_map_ids_distinct_ints(self):
        for name, d in dungeon_runs.DUNGEON_RUNS.items():
            seen = set()
            for lvl in d.get("levels", []):
                mid = lvl.get("map_id")
                self.assertIsInstance(mid, int,
                                      f"dungeon {name} level has non-int map_id")
                self.assertNotIn(mid, seen,
                                 f"dungeon {name} has duplicate map_id {mid}")
                seen.add(mid)

    def test_L40_dungeon_key_point_kinds_valid(self):
        valid_kinds = {"blessing", "quest_accept", "quest_complete",
                       "checkpoint", "key", "door", "portal",
                       "boss_engagement", "end_chest", "interact_gadget",
                       "interact_npc"}
        for name, d in dungeon_runs.DUNGEON_RUNS.items():
            for lvl in d.get("levels", []):
                for kp in lvl.get("key_points", []):
                    self.assertIn(kp.get("kind"), valid_kinds,
                                  f"dungeon {name} level {lvl.get('name')} "
                                  f"has invalid kind {kp.get('kind')!r}")

    def test_L41_dungeon_has_end_chest(self):
        for name, d in dungeon_runs.DUNGEON_RUNS.items():
            has_chest = any(
                kp.get("kind") == "end_chest"
                for lvl in d.get("levels", [])
                for kp in lvl.get("key_points", [])
            )
            self.assertTrue(has_chest,
                            f"dungeon {name} has no end_chest key_point")


# ---------------------------------------------------------------------------
# Tool schema
# ---------------------------------------------------------------------------

class TestToolSchema(unittest.TestCase):
    KNOWLEDGE_TOOLS = {
        "get_recipe", "get_outpost_info", "get_material_info",
        "get_dungeon_info", "get_blessing_info", "get_hero_build",
        "get_quest_info",
    }

    def test_L50_all_tools_present(self):
        names = {t["function"]["name"] for t in ts.ALL_TOOLS}
        missing = self.KNOWLEDGE_TOOLS - names
        self.assertFalse(missing,
                         f"knowledge tools missing from ALL_TOOLS: {missing}")

    def test_L51_tool_names_unique(self):
        names = [t["function"]["name"] for t in ts.ALL_TOOLS]
        duplicates = {n for n in names if names.count(n) > 1}
        self.assertFalse(duplicates,
                         f"duplicate tool names in ALL_TOOLS: {duplicates}")

    def test_L52_froggy_autonomous_tool_surface_excludes_low_level_cycle(self):
        self.assertIn("froggy_run_full_maintenance", ts.FROGGY_AUTONOMOUS_TOOL_NAMES)
        self.assertIn("froggy_travel_to_gadds", ts.FROGGY_AUTONOMOUS_TOOL_NAMES)
        self.assertIn("froggy_run_sparkfly_route_to_tekks", ts.FROGGY_AUTONOMOUS_TOOL_NAMES)
        self.assertIn("froggy_prepare_tekks_dungeon_entry", ts.FROGGY_AUTONOMOUS_TOOL_NAMES)
        self.assertNotIn("froggy_run_maintenance_cycle", ts.FROGGY_AUTONOMOUS_TOOL_NAMES)


# ---------------------------------------------------------------------------
# Agent-loop dispatch (routes locally, no pipe round trip)
# ---------------------------------------------------------------------------

class TestAgentLoopDispatch(unittest.IsolatedAsyncioTestCase):
    """Each knowledge tool call should be answered from farming_knowledge
    without touching ipc.send_action."""

    def _make_tool_call(self, name: str, args: dict):
        tc = MagicMock()
        tc.id = "test-call-1"
        tc.name = name
        tc.parsed_arguments = args
        return tc

    def _make_response(self, tool_calls):
        resp = MagicMock()
        resp.tool_calls = tool_calls
        return resp

    async def _make_loop(self):
        from bridge.agent_loop import AgentLoop
        ipc = MagicMock()
        ipc.send_action = AsyncMock()
        # Also mock read_message so _collect_observations_safe doesn't hang
        ipc.read_message = AsyncMock(side_effect=asyncio.TimeoutError())
        llm = MagicMock()
        kam = MagicMock()
        loop = AgentLoop(ipc=ipc, llm=llm, kamadan_client=kam)
        return loop, ipc

    async def _assert_routed_locally(self, name, args, expected_key):
        loop, ipc = await self._make_loop()
        resp = self._make_response([self._make_tool_call(name, args)])
        await loop._execute_tool_calls(resp)
        ipc.send_action.assert_not_called()
        self.assertEqual(len(loop.history), 1)
        content = json.loads(loop.history[0]["content"])
        self.assertIn(expected_key, content,
                      f"{name} response missing {expected_key!r}: {content}")

    async def test_L60_get_recipe_routed(self):
        await self._assert_routed_locally(
            "get_recipe", {"consumable_model_id": 24861}, "success")

    async def test_L61_get_outpost_routed(self):
        await self._assert_routed_locally(
            "get_outpost_info", {"map_id": 857}, "success")

    async def test_L62_get_material_routed(self):
        await self._assert_routed_locally(
            "get_material_info", {"model_id": 921}, "success")

    async def test_L63_get_dungeon_routed(self):
        await self._assert_routed_locally(
            "get_dungeon_info", {"name": "Bogroot Growths"}, "success")

    async def test_L64_get_blessing_routed(self):
        await self._assert_routed_locally(
            "get_blessing_info", {"blessing_type": "dwarven"}, "success")

    async def test_L65_get_hero_routed(self):
        await self._assert_routed_locally(
            "get_hero_build", {"hero_name": "Gwen"}, "success")

    async def test_L66_get_quest_routed(self):
        await self._assert_routed_locally(
            "get_quest_info", {"key": 825}, "success")

    async def test_L67_unknown_tool_forwarded(self):
        """Regression guard: a non-knowledge tool must still hit the pipe."""
        loop, ipc = await self._make_loop()
        loop._tool_timeout_seconds = lambda _name: 0.01
        resp = self._make_response([self._make_tool_call(
            "move_to", {"x": 0.0, "y": 0.0})])
        await loop._execute_tool_calls(resp)
        ipc.send_action.assert_awaited_once()

    async def test_L68_froggy_objective_blocks_generic_tools(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        resp = self._make_response([self._make_tool_call(
            "move_to", {"x": 0.0, "y": 0.0})])
        await loop._execute_tool_calls(resp)
        ipc.send_action.assert_not_called()
        self.assertEqual(len(loop.history), 2)
        content = json.loads(loop.history[0]["content"])
        self.assertFalse(content.get("success"))
        self.assertEqual(content.get("error"), "tool_blocked_in_froggy_autonomous_mode")
        self.assertEqual(content.get("blocked_tool"), "move_to")
        self.assertIn("[FROGGY HARNESS FALLBACK DEFERRED]", loop.history[1]["content"])
        self.assertIn('"fallback_action": "wait"', loop.history[1]["content"])

    async def test_L68b_froggy_blocks_dungeon_loop_from_gadds(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {"free_slots_total": 20, "gold_character": 5000},
        })
        loop._wait_for_action_result = AsyncMock(return_value={"success": True})

        resp = self._make_response([self._make_tool_call(
            "froggy_run_dungeon_loop", {})])
        await loop._execute_tool_calls(resp)

        ipc.send_action.assert_not_called()
        content = json.loads(loop.history[0]["content"])
        self.assertFalse(content.get("success"))
        self.assertEqual(content.get("error"), "froggy_tool_contradicts_current_state")
        self.assertEqual(content.get("blocked_tool"), "froggy_run_dungeon_loop")
        self.assertEqual(content.get("recommended_next_action"), "froggy_run_town_setup")
        self.assertIn("[FROGGY HARNESS FALLBACK DEFERRED]", loop.history[1]["content"])
        self.assertIn('"fallback_action": "froggy_run_town_setup"', loop.history[1]["content"])

    async def test_L68c_froggy_blocks_maintenance_when_setup_recommended(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._last_froggy_full_maintenance_success_time = time.monotonic()
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {"free_slots_total": 4, "gold_character": 5000},
        })
        loop._wait_for_action_result = AsyncMock(return_value={"success": True})

        resp = self._make_response([self._make_tool_call(
            "froggy_run_full_maintenance", {})])
        await loop._execute_tool_calls(resp)

        ipc.send_action.assert_not_called()
        content = json.loads(loop.history[0]["content"])
        self.assertFalse(content.get("success"))
        self.assertEqual(content.get("error"), "froggy_tool_contradicts_current_state")
        self.assertEqual(content.get("blocked_tool"), "froggy_run_full_maintenance")
        self.assertEqual(content.get("recommended_next_action"), "froggy_run_town_setup")
        self.assertIn("[FROGGY HARNESS FALLBACK DEFERRED]", loop.history[1]["content"])
        self.assertIn('"fallback_action": "froggy_run_town_setup"', loop.history[1]["content"])

    async def test_L68d_froggy_blocks_phase_tools_on_unexpected_map(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 148, "loading_state": 1},
            "inventory": {"free_slots_total": 20, "gold_character": 5000},
        })
        loop._wait_for_action_result = AsyncMock(return_value={"success": True})

        resp = self._make_response([self._make_tool_call(
            "froggy_travel_to_gadds", {})])
        await loop._execute_tool_calls(resp)

        ipc.send_action.assert_not_called()
        content = json.loads(loop.history[0]["content"])
        self.assertFalse(content.get("success"))
        self.assertEqual(content.get("error"), "froggy_tool_contradicts_current_state")
        self.assertEqual(content.get("blocked_tool"), "froggy_travel_to_gadds")
        self.assertEqual(content.get("recommended_next_action"), "query_state")
        self.assertEqual(content.get("reason"), "unexpected_map_148")
        self.assertIn("[FROGGY HARNESS FALLBACK DEFERRED]", loop.history[1]["content"])
        self.assertIn('"fallback_action": "query_state"', loop.history[1]["content"])

    async def test_L69_froggy_completed_run_recommends_maintenance_when_low_slots(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {"free_slots_total": 1, "gold_character": 5000},
            "party": {"is_defeated": True, "dead_count": 8},
        })

        enriched = loop._enrich_tool_result("froggy_run_dungeon_loop", {"success": True})

        self.assertTrue(enriched.get("completed_dungeon_run"))
        self.assertTrue(enriched.get("quest_reward_claimed"))
        self.assertEqual(enriched.get("recommended_next_action"), "froggy_run_full_maintenance")
        self.assertEqual(enriched.get("reason"), "completed_run_low_inventory_space")
        self.assertIn("not a wipe", enriched.get("interpretation", ""))

    async def test_L70_froggy_completed_run_refreshes_before_next_run(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {"free_slots_total": 20, "gold_character": 5000},
            "party": {"is_defeated": False, "dead_count": 0},
        })

        enriched = loop._enrich_tool_result("froggy_run_dungeon_loop", {"success": True})

        self.assertTrue(enriched.get("completed_dungeon_run"))
        self.assertEqual(enriched.get("recommended_next_action"), "query_state")
        self.assertEqual(enriched.get("reason"), "completed_run_refresh_state_before_next_run")

    async def test_L71_froggy_recommends_maintenance_for_ten_slots_in_gadds(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 10,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_full_maintenance")
        self.assertEqual(recommendation.get("reason"), "low_inventory_space_in_gadds")

    async def test_L72_froggy_recommends_maintenance_for_storage_gold_pressure(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 30,
                "gold_character": 5000,
                "gold_storage": 800000,
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_full_maintenance")
        self.assertEqual(recommendation.get("reason"), "storage_gold_above_conset_conversion_threshold")

    async def test_L73_froggy_recommends_query_state_when_inventory_unknown_in_gadds(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "query_state")
        self.assertEqual(recommendation.get("reason"), "inventory_unknown_in_gadds")

    async def test_L74_froggy_recent_maintenance_allows_acceptable_slots(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._last_froggy_full_maintenance_success_time = time.monotonic()
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 6,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_town_setup")
        self.assertEqual(recommendation.get("reason"), "recent_maintenance_slots_acceptable")

    async def test_L75_froggy_recent_maintenance_allows_critical_floor_slots(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._last_froggy_full_maintenance_success_time = time.monotonic()
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 2,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_town_setup")
        self.assertEqual(recommendation.get("reason"), "recent_maintenance_slots_acceptable")

    async def test_L75b_froggy_repeated_exhausted_maintenance_allows_critical_floor_slots(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._consecutive_froggy_maintenance_failures = 2
        loop._last_froggy_maintenance_failure_time = time.monotonic()
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 2,
                "gold_character": 5000,
                "gold_storage": 100000,
                "froggy_maintenance": {
                    "conset_material_stacks_inventory": 0,
                    "loose_consets_inventory_total": 3,
                    "stored_consets_total": 300,
                },
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_town_setup")
        self.assertEqual(recommendation.get("reason"), "recent_maintenance_exhausted_slots_acceptable")

    async def test_L75c_froggy_missing_character_conset_overrides_recent_maintenance(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._last_froggy_full_maintenance_success_time = time.monotonic()
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 5,
                "gold_character": 5000,
                "gold_storage": 100000,
                "froggy_maintenance": {
                    "conset_material_stacks_inventory": 0,
                    "loose_consets_inventory_total": 60,
                    "grail_inventory": 40,
                    "essence_inventory": 20,
                    "armor_inventory": 0,
                    "stored_consets_total": 300,
                    "stored_grail": 100,
                    "stored_essence": 100,
                    "stored_armor": 100,
                },
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_full_maintenance")
        self.assertEqual(recommendation.get("reason"), "character_conset_set_incomplete")

    async def test_L76_froggy_state_prompt_includes_harness_recommendation(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._cycle_count = 76
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 1},
            "inventory": {"free_slots_total": 12, "gold_character": 5000},
        })

        messages = loop._build_messages()
        prompt = messages[-1]["content"]

        self.assertIn("[FROGGY HIGH-LEVEL RUNBOOK]", prompt)
        self.assertIn("[FROGGY HARNESS RECOMMENDATION]", prompt)
        self.assertIn("Froggy hint: in Sparkfly Swamp, use froggy_run_sparkfly_route_to_tekks", prompt)
        self.assertNotIn("Froggy hint: use froggy_run_dungeon_loop", prompt)
        self.assertIn("Do not call froggy_run_dungeon_loop", prompt)
        self.assertIn("immediately after Sparkfly travel", prompt)
        self.assertIn("recommended_next_action=froggy_run_sparkfly_route_to_tekks", prompt)
        self.assertIn("reason=sparkfly_entry_route_to_tekks", prompt)

    async def test_L76b_froggy_state_prompt_includes_compact_monitoring_stats(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "bot": {
                "is_running": True,
                "state": "llm_controlled",
                "combat_mode": "builtin",
                "froggy_monitoring": {
                    "run_count": 4,
                    "fail_count": 1,
                    "monitoring_wipes": 2,
                    "route_wipe_count": 2,
                    "chests_opened": 5,
                    "rare_skins": 1,
                    "gold_items": 11,
                    "tomes": 2,
                },
                "froggy_dungeon_loop": {
                    "last_waypoint_index": 21,
                    "last_waypoint_label": "19",
                    "entered_lvl2": False,
                    "boss_started": False,
                    "boss_completed": False,
                    "returned_to_sparkfly": False,
                    "chest_successes": 0,
                    "chest_attempts": 0,
                    "reward_attempted": False,
                },
            },
            "map": {"map_id": 615, "loading_state": 1},
            "inventory": {"free_slots_total": 12, "gold_character": 5000},
        })

        prompt = loop._build_messages()[-1]["content"]

        self.assertIn("Froggy stats: runs=4 fails=1 wipes=2 route_wipes=2", prompt)
        self.assertIn("chests=5 rare=1 gold_items=11 tomes=2", prompt)
        self.assertIn("Froggy loop: wp=21 label=\"19\"", prompt)
        self.assertIn("entered_lvl2=False", prompt)

    async def test_L77_invalid_tool_json_returns_structured_error(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 1},
            "inventory": {"free_slots_total": 12, "gold_character": 5000},
        })

        class BadToolCall:
            id = "test-call-1"
            name = "froggy_run_dungeon_loop"
            arguments = "{not-json"

            @property
            def parsed_arguments(self):
                return json.loads(self.arguments)

        call = BadToolCall()
        loop._wait_for_action_result = AsyncMock(return_value={"success": True})

        await loop._execute_tool_calls(self._make_response([call]))

        ipc.send_action.assert_not_called()
        self.assertEqual(len(loop.history), 2)
        content = json.loads(loop.history[0]["content"])
        self.assertFalse(content.get("success"))
        self.assertEqual(content.get("error"), "invalid_tool_arguments_json")
        self.assertEqual(content.get("recommended_next_action"), "froggy_run_sparkfly_route_to_tekks")
        self.assertIn("[FROGGY HARNESS FALLBACK DEFERRED]", loop.history[1]["content"])
        self.assertIn('"fallback_trigger": "invalid_tool_arguments_json"', loop.history[1]["content"])

    async def test_L78_froggy_town_setup_result_recommends_entry_travel(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {"free_slots_total": 20, "gold_character": 5000},
        })

        enriched = loop._enrich_tool_result("froggy_run_town_setup", {"success": True})

        self.assertTrue(enriched.get("completed_town_setup"))
        self.assertEqual(enriched.get("recommended_next_action"), "froggy_travel_to_sparkfly")
        self.assertEqual(enriched.get("reason"), "town_setup_complete_travel_to_entry_map")

    async def test_L79_froggy_entry_travel_result_recommends_wait(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {"free_slots_total": 20, "gold_character": 5000},
        })

        enriched = loop._enrich_tool_result("froggy_travel_to_sparkfly", {"success": True})

        self.assertTrue(enriched.get("completed_entry_travel"))
        self.assertEqual(enriched.get("recommended_next_action"), "wait")
        self.assertEqual(enriched.get("reason"), "entry_travel_complete_wait_for_loaded_snapshot")

    async def test_L80_froggy_prompt_says_high_level_tools_own_dialogs(self):
        from bridge.agent_loop import SYSTEM_PROMPT

        self.assertIn("Froggy tools", SYSTEM_PROMPT)
        self.assertIn("do not handle open dialogs with generic dialog actions", SYSTEM_PROMPT)

    async def test_L81_froggy_recommends_maintenance_for_conset_material_pressure(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 10,
                "gold_character": 5000,
                "gold_storage": 100000,
                "froggy_maintenance": {
                    "conset_material_stacks_inventory": 11,
                    "conset_material_quantity_inventory": 1100,
                    "loose_consets_inventory_total": 0,
                    "stored_consets_total": 300,
                },
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_full_maintenance")
        self.assertEqual(recommendation.get("reason"), "conset_material_pressure_in_gadds")

    async def test_L82_froggy_recommends_maintenance_for_loose_consets(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 10,
                "gold_character": 5000,
                "gold_storage": 100000,
                "froggy_maintenance": {
                    "conset_material_stacks_inventory": 0,
                    "loose_consets_inventory_total": 3,
                    "stored_consets_total": 300,
                },
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_full_maintenance")
        self.assertEqual(recommendation.get("reason"), "loose_consets_need_storage")

    async def test_L83_froggy_recommends_maintenance_for_low_stored_consets_with_gold(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 30,
                "gold_character": 5000,
                "gold_storage": 800000,
                "froggy_maintenance": {
                    "conset_material_stacks_inventory": 0,
                    "loose_consets_inventory_total": 0,
                    "stored_consets_total": 20,
                },
            },
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_full_maintenance")
        self.assertEqual(recommendation.get("reason"), "stored_consets_low_with_gold_available")

    async def test_L84_froggy_idle_fallback_executes_recommended_pipe_action(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 1},
            "inventory": {"free_slots_total": 12, "gold_character": 5000},
        })
        loop._wait_for_action_result = AsyncMock(return_value={"success": True})

        ok = await loop._execute_froggy_recommended_action("unit_test")

        self.assertTrue(ok)
        ipc.send_action.assert_awaited_once()
        self.assertEqual(ipc.send_action.await_args.args[0], "froggy_run_sparkfly_route_to_tekks")
        self.assertEqual(len(loop.history), 1)
        self.assertEqual(loop.history[0]["role"], "user")
        content = loop.history[0]["content"]
        self.assertIn("[FROGGY HARNESS FALLBACK EXECUTED]", content)
        self.assertIn('"harness_fallback_executed": true', content)
        self.assertIn('"fallback_action": "froggy_run_sparkfly_route_to_tekks"', content)

    async def test_L85_froggy_idle_fallback_waits_locally_while_loading(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 0},
            "inventory": {"free_slots_total": 12, "gold_character": 5000},
        })

        ok = await loop._execute_froggy_recommended_action("unit_test")

        self.assertTrue(ok)
        ipc.send_action.assert_not_called()
        self.assertEqual(len(loop.history), 1)
        self.assertIn('"fallback_action": "wait"', loop.history[0]["content"])

    async def test_L86_froggy_recommends_gadds_recovery_from_embark(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 857, "loading_state": 1},
            "inventory": {"free_slots_total": 3, "gold_character": 5000},
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_travel_to_gadds")
        self.assertEqual(recommendation.get("reason"), "recover_from_embark_to_gadds")

    async def test_L87_froggy_gadds_recovery_result_refreshes_state(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 857, "loading_state": 1},
            "inventory": {"free_slots_total": 3, "gold_character": 5000},
        })

        enriched = loop._enrich_tool_result("froggy_travel_to_gadds", {"success": True})

        self.assertTrue(enriched.get("completed_recovery_travel"))
        self.assertEqual(enriched.get("recommended_next_action"), "query_state")
        self.assertEqual(enriched.get("reason"), "returned_to_gadds_refresh_state")

    async def test_L88_froggy_rate_limit_defers_harness_fallback_by_default(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._execute_froggy_recommended_action = AsyncMock(return_value=True)
        request = httpx.Request("POST", "http://localhost:11434/v1/chat/completions")
        response = httpx.Response(429, request=request, headers={"retry-after": "1"})
        error = httpx.HTTPStatusError("rate limited", request=request, response=response)

        handled = await loop._handle_llm_rate_limit(error)

        self.assertTrue(handled)
        loop._execute_froggy_recommended_action.assert_not_called()
        self.assertGreater(loop._llm_rate_limit_until, time.monotonic())
        self.assertIn("[FROGGY HARNESS FALLBACK DEFERRED]", loop.history[0]["content"])

    async def test_L88a_codex_exec_usage_limit_defers_until_reset_time(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._execute_froggy_recommended_action = AsyncMock(return_value=True)
        reset = dt.datetime.now() + dt.timedelta(minutes=45)
        reset_label = reset.strftime("%I:%M %p").lstrip("0")
        error = RuntimeError(
            "codex exec failed with exit 1: ERROR: You've hit your usage limit "
            f"for GPT-5.3-Codex-Spark. Switch to another model now, or try again at {reset_label}."
        )

        handled = await loop._handle_llm_rate_limit(error)

        self.assertTrue(handled)
        loop._execute_froggy_recommended_action.assert_not_called()
        self.assertGreater(loop._llm_rate_limit_until - time.monotonic(), 1200)
        self.assertIn("[FROGGY HARNESS FALLBACK DEFERRED]", loop.history[0]["content"])

    async def test_L88b_froggy_blocks_repeated_failed_high_level_action(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {"free_slots_total": 20, "gold_character": 5000},
        })
        loop._enrich_tool_result("froggy_run_town_setup", {
            "success": False,
            "error": "froggy_outpost_setup_failed",
        })
        loop._enrich_tool_result("froggy_run_town_setup", {
            "success": False,
            "error": "froggy_outpost_setup_failed",
        })

        resp = self._make_response([self._make_tool_call(
            "froggy_run_town_setup", {})])
        await loop._execute_tool_calls(resp)

        ipc.send_action.assert_not_called()
        content = json.loads(loop.history[-1]["content"])
        self.assertFalse(content.get("success"))
        self.assertEqual(content.get("error"), "froggy_action_retry_budget_exhausted")
        self.assertEqual(content.get("blocked_tool"), "froggy_run_town_setup")
        self.assertEqual(content.get("recommended_next_action"), "query_state")

    async def test_L89_froggy_recommendation_keeps_tier3_inventory_after_tier1(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "type": "snapshot",
            "tier": 3,
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 20,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })
        loop.observations.add_snapshot({
            "type": "snapshot",
            "tier": 1,
            "map": {"map_id": 638, "loading_state": 1},
        })

        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_run_town_setup")
        self.assertEqual(recommendation.get("reason"), "ready_for_outpost_setup")

    async def test_L90_froggy_town_setup_success_advances_to_entry_travel(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 3,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })

        loop._enrich_tool_result("froggy_run_town_setup", {"success": True})
        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "froggy_travel_to_sparkfly")
        self.assertEqual(recommendation.get("reason"), "town_setup_complete_travel_to_entry_map")

    async def test_L91_froggy_entry_travel_success_waits_while_still_in_gadds(self):
        loop, _ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 638, "loading_state": 1},
            "inventory": {
                "free_slots_total": 3,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })

        loop._enrich_tool_result("froggy_travel_to_sparkfly", {"success": True})
        recommendation = loop._recommend_froggy_action()

        self.assertEqual(recommendation.get("recommended_next_action"), "wait")
        self.assertEqual(recommendation.get("reason"), "waiting_for_sparkfly_load_after_entry_travel")

    async def test_L92_froggy_autopilot_is_disabled_by_default(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 1},
            "inventory": {
                "free_slots_total": 12,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })
        loop._wait_for_action_result = AsyncMock(return_value={"success": True})

        ok = await loop._execute_froggy_autopilot_action()

        self.assertFalse(ok)
        ipc.send_action.assert_not_called()

    async def test_L92b_froggy_autopilot_executes_when_enabled(self):
        from bridge import agent_loop as al

        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 1},
            "inventory": {
                "free_slots_total": 12,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })
        loop._wait_for_action_result = AsyncMock(return_value={"success": True})

        previous = al.FROGGY_AUTOPILOT_ENABLED
        al.FROGGY_AUTOPILOT_ENABLED = True
        try:
            ok = await loop._execute_froggy_autopilot_action()
        finally:
            al.FROGGY_AUTOPILOT_ENABLED = previous

        self.assertTrue(ok)
        ipc.send_action.assert_awaited_once()
        self.assertEqual(ipc.send_action.await_args.args[0], "froggy_run_sparkfly_route_to_tekks")
        self.assertIn('"fallback_trigger": "froggy_autopilot"', loop.history[0]["content"])

    async def test_L93_froggy_autopilot_defers_after_user_message_until_llm_runs(self):
        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop._froggy_force_llm_once = True
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 1},
            "inventory": {
                "free_slots_total": 12,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })

        ok = await loop._execute_froggy_autopilot_action()

        self.assertFalse(ok)
        ipc.send_action.assert_not_called()

    async def test_L94_froggy_autopilot_handles_wait_when_enabled(self):
        from bridge import agent_loop as al

        loop, ipc = await self._make_loop()
        loop.objective = "Autonomously farm Froggy HM / Bogroot Growths"
        loop.observations.add_snapshot({
            "map": {"map_id": 558, "loading_state": 0},
            "inventory": {
                "free_slots_total": 12,
                "gold_character": 5000,
                "gold_storage": 100000,
            },
        })

        previous = al.FROGGY_AUTOPILOT_ENABLED
        al.FROGGY_AUTOPILOT_ENABLED = True
        try:
            ok = await loop._execute_froggy_autopilot_action()
        finally:
            al.FROGGY_AUTOPILOT_ENABLED = previous

        self.assertTrue(ok)
        ipc.send_action.assert_not_called()
        self.assertIn('"fallback_action": "wait"', loop.history[0]["content"])


if __name__ == "__main__":
    unittest.main()
