"""Unit tests for the agent registry utility script."""

from __future__ import annotations

import tempfile
import unittest
import importlib.util
import sys
from pathlib import Path

from .helpers import TestFailure


REPO_ROOT = Path(__file__).resolve().parents[3]
SCRIPT_PATH = REPO_ROOT / "scripts" / "agent_registry.py"
_SPEC = importlib.util.spec_from_file_location("agent_registry_script", SCRIPT_PATH)
agent_registry = importlib.util.module_from_spec(_SPEC)
assert _SPEC is not None and _SPEC.loader is not None
sys.modules[_SPEC.name] = agent_registry
_SPEC.loader.exec_module(agent_registry)


class TestAgentRegistryCli(unittest.TestCase):
    def _temp_registry(self) -> Path:
        with tempfile.NamedTemporaryFile(suffix="_agent_registry.md", delete=False) as tmp:
            path = Path(tmp.name)
        path.write_text(agent_registry.DEFAULT_REGISTRY.read_text(encoding="utf-8"), encoding="utf-8")
        return path

    def test_list_rows_filters_by_status(self):
        """PASS: list_rows can show only available accounts."""
        _, rows, _ = agent_registry.load_registry()
        output = agent_registry.list_rows(rows, status="available")
        self.assertIn("D I S C O P A N I C", output)
        self.assertNotIn("L I L B I S C U I T [active]", output)

    def test_claim_and_release_round_trip(self):
        """PASS: claim then release updates the markdown table status field."""
        registry_path = self._temp_registry()
        prefix, rows, suffix = agent_registry.load_registry(registry_path)

        row = agent_registry.find_row(rows, "1", None)
        self.assertEqual(row.status, "available")
        agent_registry.claim_row(row)
        agent_registry.save_registry(rows, prefix, suffix, registry_path)

        _, rows_after_claim, _ = agent_registry.load_registry(registry_path)
        claimed = agent_registry.find_row(rows_after_claim, "1", None)
        self.assertEqual(claimed.status, "active")

        prefix, rows_after_claim, suffix = agent_registry.load_registry(registry_path)
        claimed = agent_registry.find_row(rows_after_claim, "1", None)
        agent_registry.release_row(claimed)
        agent_registry.save_registry(rows_after_claim, prefix, suffix, registry_path)

        _, rows_after_release, _ = agent_registry.load_registry(registry_path)
        released = agent_registry.find_row(rows_after_release, "1", None)
        self.assertEqual(released.status, "available")

    def test_claim_helper_only_requires_force(self):
        """PASS: helper-only rows cannot be claimed accidentally."""
        _, rows, _ = agent_registry.load_registry()
        helper = agent_registry.find_row(rows, "2", None)
        with self.assertRaisesRegex(ValueError, "helper-only"):
            agent_registry.claim_row(helper)

    def test_find_row_by_character(self):
        """PASS: rows can be resolved by exact character name."""
        _, rows, _ = agent_registry.load_registry()
        row = agent_registry.find_row(rows, None, "L I L B I S C U I T")
        self.assertEqual(row.account_index, "4")
        self.assertEqual(row.lane_tag, "biscuit")

    def test_find_first_available_skips_active_and_helper_only(self):
        """PASS: first-available allocation picks the first general-purpose available account."""
        _, rows, _ = agent_registry.load_registry()
        row = agent_registry.find_first_available(rows)
        self.assertEqual(row.account_index, "1")
        self.assertEqual(row.character, "D I S C O P A N I C")


def _run_case(case_type: type[unittest.TestCase]) -> None:
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(case_type)
    result = unittest.TestResult()
    suite.run(result)
    if not result.wasSuccessful():
        details = []
        for test, message in result.failures + result.errors:
            details.append(f"{test.id()}: {message.splitlines()[-1] if message else 'unknown failure'}")
        raise TestFailure("; ".join(details))


async def test_agent_registry_cli_suite(_tc):
    """Bridge-runner wrapper: validate the registry helper script in the default test path."""
    _run_case(TestAgentRegistryCli)


test_agent_registry_cli_suite.requires_bridge = False


if __name__ == "__main__":
    unittest.main()
