import unittest

from bridge.tool_schema import (
    FROGGY_AUTONOMOUS_TOOLS,
    filter_tools_for_observation,
    tools_for_observation,
)


def _names(tools: list[dict]) -> set[str]:
    return {tool["function"]["name"] for tool in tools}


class ConditionalToolExposureTests(unittest.TestCase):
    def test_trade_tools_only_surface_when_trade_window_is_open(self):
        closed = _names(tools_for_observation({"trade": {"is_open": False}}))
        opened = _names(tools_for_observation({"trade": {"is_open": True}}))

        self.assertNotIn("accept_trade", closed)
        self.assertNotIn("offer_trade_item", closed)
        self.assertIn("accept_trade", opened)
        self.assertIn("offer_trade_item", opened)
        self.assertIn("query_state", closed)

    def test_salvage_and_identify_tools_require_candidate_items(self):
        no_candidates = _names(tools_for_observation({
            "inventory": {"bags": [{"items": [{"item_id": 1, "is_identified": True}]}]},
        }))
        unid_candidate = _names(tools_for_observation({
            "inventory": {"bags": [{"items": [{"item_id": 2, "is_identified": False}]}]},
        }))
        salvage_candidate = _names(tools_for_observation({
            "inventory": {"bags": [{"items": [{"item_id": 3, "is_material_salvageable": True}]}]},
        }))

        self.assertNotIn("identify_item", no_candidates)
        self.assertNotIn("salvage_start", no_candidates)
        self.assertIn("identify_item", unid_candidate)
        self.assertIn("salvage_start", salvage_candidate)

    def test_dungeon_tools_only_surface_inside_dungeon_maps(self):
        outpost = _names(filter_tools_for_observation(
            FROGGY_AUTONOMOUS_TOOLS,
            {"map": {"map_id": 638}},
        ))
        dungeon = _names(filter_tools_for_observation(
            FROGGY_AUTONOMOUS_TOOLS,
            {"map": {"map_id": 615}},
        ))

        self.assertNotIn("froggy_run_dungeon_loop", outpost)
        self.assertIn("froggy_run_dungeon_loop", dungeon)
        self.assertIn("froggy_run_town_setup", outpost)


if __name__ == "__main__":
    unittest.main()
