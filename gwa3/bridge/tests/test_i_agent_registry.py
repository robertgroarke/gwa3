"""Consistency checks for the shared agent account registry."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from .helpers import TestFailure


REPO_ROOT = Path(__file__).resolve().parents[3]
REGISTRY_PATH = REPO_ROOT / "AGENT_ACCOUNT_REGISTRY.md"
PRESETS_PATH = REPO_ROOT / "gwa3" / "CMakePresets.json"


def _parse_registry_rows() -> list[dict[str, str]]:
    text = REGISTRY_PATH.read_text(encoding="utf-8")
    rows = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped.startswith("|"):
            continue
        if stripped.startswith("|---"):
            continue
        cells = [cell.strip() for cell in stripped.strip("|").split("|")]
        if len(cells) != 8 or cells[0] == "Account Index":
            continue
        rows.append(
            {
                "account_index": cells[0],
                "character": cells[1].strip("`"),
                "status": cells[2].strip("`"),
                "lane_tag": cells[3].strip("`"),
                "build_dir": cells[4].strip("`"),
                "dll": cells[5].strip("`"),
                "pipe": cells[6].strip("`"),
                "launcher_script": cells[7].strip("`"),
            }
        )
    return rows


def _run_case(case_type: type[unittest.TestCase]) -> None:
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(case_type)
    result = unittest.TestResult()
    suite.run(result)
    if not result.wasSuccessful():
        details = []
        for test, message in result.failures + result.errors:
            details.append(f"{test.id()}: {message.splitlines()[-1] if message else 'unknown failure'}")
        raise TestFailure("; ".join(details))


class TestAgentRegistry(unittest.TestCase):
    def test_registry_rows_have_unique_account_indexes_and_lane_tags(self):
        """PASS: each registry row has a unique account index and lane tag."""
        rows = _parse_registry_rows()
        self.assertGreaterEqual(len(rows), 5)
        self.assertEqual(len({row["account_index"] for row in rows}), len(rows))
        self.assertEqual(len({row["lane_tag"] for row in rows}), len(rows))

    def test_registry_statuses_are_known(self):
        """PASS: registry only uses documented status values."""
        rows = _parse_registry_rows()
        allowed = {"available", "reserved", "active", "helper-only"}
        for row in rows:
            self.assertIn(row["status"], allowed, msg=f"unexpected status in {row}")

    def test_registry_launchers_exist(self):
        """PASS: every registry row points at a real launcher script."""
        rows = _parse_registry_rows()
        for row in rows:
            launcher_path = REPO_ROOT / Path(row["launcher_script"])
            self.assertTrue(launcher_path.exists(), msg=f"missing launcher for {row['lane_tag']}: {launcher_path}")

    def test_registry_non_helper_lanes_match_cmake_presets(self):
        """PASS: each general-purpose lane matches a real CMake preset and its isolated outputs."""
        rows = _parse_registry_rows()
        presets = json.loads(PRESETS_PATH.read_text(encoding="utf-8"))
        configure = {preset["name"]: preset for preset in presets["configurePresets"]}
        build = {preset["name"]: preset for preset in presets["buildPresets"]}

        for row in rows:
            lane = row["lane_tag"]
            if lane == "trade-helper":
                lane = "trade"
            if row["status"] == "helper-only":
                expected_preset = "trade"
            else:
                expected_preset = row["lane_tag"]

            self.assertIn(expected_preset, configure, msg=f"missing configure preset for {row}")
            self.assertIn(expected_preset, build, msg=f"missing build preset for {row}")

            configure_preset = configure[expected_preset]
            build_preset = build[expected_preset]
            self.assertEqual(build_preset["configurePreset"], expected_preset)
            self.assertEqual(configure_preset["binaryDir"], "${sourceDir}/" + Path(row["build_dir"]).name)
            self.assertEqual(configure_preset["cacheVariables"]["GWA3_DLL_NAME"], Path(row["dll"]).stem)
            self.assertEqual(configure_preset["cacheVariables"]["GWA3_PIPE_NAME"], row["pipe"])

    def test_registry_mentions_known_accounts_json_indexes(self):
        """PASS: registry preserves the known 0-4 account index mapping."""
        rows = _parse_registry_rows()
        indexes = sorted(int(row["account_index"]) for row in rows)
        self.assertEqual(indexes, [0, 1, 2, 3, 4])


async def test_agent_account_registry_suite(_tc):
    """Bridge-runner wrapper: enforce registry consistency in the default test path."""
    _run_case(TestAgentRegistry)

test_agent_account_registry_suite.requires_bridge = False


if __name__ == "__main__":
    unittest.main()
