import json
import re
import unittest
from pathlib import Path

from bridge.agent_loop import SYSTEM_PROMPT
from bridge.observation import ObservationWindow


ROOT = Path(__file__).resolve().parents[2]
ROUTE_FILE = ROOT / "routes" / "bogroot_hm_v1.json"
ROUTE_SCHEMA = ROOT / "routes" / "route_schema.json"
GAME_SNAPSHOT = ROOT / "src" / "gwa3" / "llm" / "GameSnapshot.cpp"
ROUTE_WALKER = ROOT / "src" / "gwa3" / "llm" / "RouteWalker.cpp"


class RouteContractTests(unittest.TestCase):
    def test_bogroot_route_has_contiguous_public_safe_steps(self):
        route = json.loads(ROUTE_FILE.read_text(encoding="utf-8"))
        schema = json.loads(ROUTE_SCHEMA.read_text(encoding="utf-8"))
        allowed_kinds = set(schema["properties"]["steps"]["items"]["properties"]["kind"]["enum"])

        self.assertEqual(route["schema_version"], 1)
        self.assertEqual(route["script_id"], "bogroot_hm_v1")
        self.assertEqual(route["bot"], "FroggyHM")
        self.assertEqual(len(route["steps"]), 29)
        self.assertEqual([step["order"] for step in route["steps"]], list(range(29)))

        for step in route["steps"]:
            for key in ("id", "map_id", "kind", "label", "x", "y", "tolerance", "phase", "order", "source"):
                self.assertIn(key, step)
            self.assertIn(step["kind"], allowed_kinds)
            self.assertIn(step["map_id"], {558, 615, 616, 638})
            self.assertGreaterEqual(step["tolerance"], 0)

        serialized = json.dumps(route).lower()
        for private_key in ("account", "character", "email", "password", "launcher"):
            self.assertNotIn(private_key, serialized)

    def test_tier1_snapshot_contains_route_builder(self):
        source = GAME_SNAPSHOT.read_text(encoding="utf-8")

        self.assertIn('#include <gwa3/llm/RouteWalker.h>', source)
        self.assertIn('j["route"] = TryBuildRouteJson();', source)

    def test_route_walker_uses_direct_map_constants(self):
        source = ROUTE_WALKER.read_text(encoding="utf-8")

        self.assertIn("MapIds::SPARKFLY_SWAMP", source)
        self.assertNotRegex(source, re.compile(r"constexpr\s+[^=]+\s+\w+\s*=\s*GWA3::"))

    def test_observation_summary_surfaces_route_status_and_deviation(self):
        observations = ObservationWindow()
        observations.add_snapshot({
            "route": {
                "script_id": "bogroot_hm_v1",
                "step_index": 12,
                "step_count": 29,
                "phase": "dungeon_entry",
                "next_step": {"kind": "dungeon_entry"},
                "deviation": {
                    "reason": "off_route",
                    "recovery_actions": ["query_state", "wait"],
                },
            }
        })

        summary = observations.build_context_summary(tier1_only=True)

        self.assertIn("Route: On route bogroot_hm_v1 step 12/29 (dungeon_entry)", summary)
        self.assertIn("Route deviation: off_route", summary)
        self.assertIn("query_state, wait", summary)

    def test_prompt_encourages_wait_when_native_route_has_no_deviation(self):
        self.assertIn("snapshot.route.deviation is null", SYSTEM_PROMPT)
        self.assertIn("prefer wait/query_state/no-op", SYSTEM_PROMPT)


if __name__ == "__main__":
    unittest.main()
