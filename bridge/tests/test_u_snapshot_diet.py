import unittest

from bridge.observation import ObservationWindow


def _foe(agent_id: int, distance: float, *, casting: bool = False, activation: float | None = None) -> dict:
    foe = {
        "id": agent_id,
        "agent_type": "living",
        "allegiance": 3,
        "is_alive": True,
        "distance": distance,
        "hp": 1.0,
        "energy": 0.5,
        "level": 24,
        "primary": 3,
        "secondary": 0,
        "has_hex": False,
        "has_enchantment": False,
        "is_casting": casting,
        "casting_skill_id": 0,
        "name": f"Bog foe {agent_id}",
    }
    if casting:
        foe["casting_skill_id"] = 999
        if activation is not None:
            foe["casting_skill_activation"] = activation
    return foe


def _skillbar(recharge_slot: int | None = None) -> list[dict]:
    skills = []
    for slot in range(8):
        skills.append({
            "slot": slot,
            "skill_id": 2000 + slot,
            "recharge": 12 if slot == recharge_slot else 0,
            "adrenaline": 0,
            "event": 0,
            "type": 2,
            "energy_cost": 5,
        })
    return skills


class SnapshotDietTests(unittest.TestCase):
    def test_context_limits_foe_details_but_keeps_priority_caster(self):
        observations = ObservationWindow()
        agents = [_foe(1000 + index, 100 + index * 100) for index in range(12)]
        agents.append(_foe(1099, 2400, casting=True, activation=2.0))

        observations.add_snapshot({
            "me": {"agent_id": 1, "hp": 1.0, "energy": 1.0},
            "map": {"map_id": 615, "loading_state": 1, "instance_time": 120000},
            "agents": agents,
        })

        summary = observations.build_context_summary()

        self.assertIn("Foes shown: 6/13", summary)
        self.assertIn("id=1000", summary)
        self.assertIn("id=1004", summary)
        self.assertIn("id=1099", summary)
        self.assertNotIn("id=1005", summary)
        self.assertLess(len(summary.encode("utf-8")), 1800)

    def test_skillbar_and_ground_items_are_delta_rendered(self):
        observations = ObservationWindow()
        item = {
            "id": 500,
            "agent_type": "item",
            "distance": 250,
            "item_id": 9000,
            "model_id": 146,
            "quantity": 1,
            "owner": 1,
        }

        observations.add_snapshot({
            "me": {"agent_id": 1, "hp": 1.0, "energy": 1.0},
            "skillbar": _skillbar(),
            "agents": [item],
        })
        first_summary = observations.build_context_summary()
        self.assertIn("Skills:", first_summary)
        self.assertIn("Items on ground:", first_summary)

        observations.add_snapshot({
            "me": {"agent_id": 1, "hp": 1.0, "energy": 1.0},
            "skillbar": _skillbar(recharge_slot=2),
            "agents": [item],
        })
        second_summary = observations.build_context_summary()

        self.assertIn("Skill changes:", second_summary)
        self.assertIn("[2:", second_summary)
        self.assertNotIn("[1:", second_summary)
        self.assertIn("Items on ground unchanged: 1 tracked", second_summary)
        self.assertNotIn("agent_id=500 dist=250", second_summary)

    def test_tier_delta_contract_is_present_in_cpp_serializer(self):
        from pathlib import Path

        source = (Path(__file__).resolve().parents[2] / "src" / "gwa3" / "llm" / "GameSnapshot.cpp").read_text(
            encoding="utf-8"
        )

        self.assertIn('j["delta_from_tier"] = 1;', source)
        self.assertIn('j["delta_from_tier"] = 2;', source)
        self.assertIn('j["agents_meta"] = BuildNearbyAgentsMetaJson();', source)
        self.assertIn("Foes are capped to the nearest five", source)


if __name__ == "__main__":
    unittest.main()
