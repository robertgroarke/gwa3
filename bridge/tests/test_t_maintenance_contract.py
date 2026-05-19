import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAINTENANCE_MGR = ROOT / "src" / "gwa3" / "managers" / "MaintenanceMgr.cpp"


class MaintenanceContractTests(unittest.TestCase):
    def test_character_conset_restock_uses_storage_before_crafting(self):
        source = MAINTENANCE_MGR.read_text(encoding="utf-8")
        match = re.search(
            r"static bool CraftCharacterConsetRestock\(const Config& cfg\) \{(?P<body>.*?)\n\}",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(match)
        body = match.group("body")

        withdrawal_pos = body.find("WithdrawMissingConsetsFromStorage(target)")
        craft_map_pos = body.find("EnsureMap(MapIds::EMBARK_BEACH)")

        self.assertGreaterEqual(withdrawal_pos, 0)
        self.assertGreaterEqual(craft_map_pos, 0)
        self.assertLess(withdrawal_pos, craft_map_pos)
        self.assertIn("Character conset restock satisfied from Xunlai", body)


if __name__ == "__main__":
    unittest.main()
