"""Unit tests for the work registry and helper script."""

from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

from .helpers import TestFailure


REPO_ROOT = Path(__file__).resolve().parents[3]
REGISTRY_PATH = REPO_ROOT / "AGENT_WORK_REGISTRY.md"
SCRIPT_PATH = REPO_ROOT / "scripts" / "agent_work_registry.py"
_SPEC = importlib.util.spec_from_file_location("agent_work_registry_script", SCRIPT_PATH)
agent_work_registry = importlib.util.module_from_spec(_SPEC)
assert _SPEC is not None and _SPEC.loader is not None
sys.modules[_SPEC.name] = agent_work_registry
_SPEC.loader.exec_module(agent_work_registry)

WORK_REGISTRY_FIXTURE = """# Agent Work Registry

| Work Area | Status | Owner | Lane | Scope | Primary Files | Notes |
|---|---|---|---|---|---|---|
| `agent-coordination` | `active` | `BISCUIT` | `none` | registry tests fixture | `AGENT_WORK_REGISTRY.md` | active for fixture |
| `kamadan-bridge` | `handoff` | `BISCUIT` | `none` | bridge fixture | `gwa3/bridge` | bridge work stable |
| `player-trade-validation` | `active` | `CODEX` | `any` | player trade fixture | `gwa3/bridge/tests/test_f_player_trade.py` | active for fixture |
"""


class TestAgentWorkRegistry(unittest.TestCase):
    def _temp_registry(self) -> Path:
        with tempfile.NamedTemporaryFile(suffix="_agent_work_registry.md", delete=False) as tmp:
            path = Path(tmp.name)
        path.write_text(WORK_REGISTRY_FIXTURE, encoding="utf-8")
        return path

    def test_list_rows_filters_active(self):
        """PASS: list_rows can show only active work areas."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        output = agent_work_registry.list_rows(rows, status="active")
        self.assertIn("agent-coordination", output)
        self.assertIn("player-trade-validation", output)

    def test_claim_new_area_adds_row(self):
        """PASS: claiming a new work area appends a sorted active row."""
        registry_path = self._temp_registry()
        prefix, rows, suffix = agent_work_registry.load_registry(registry_path)
        agent_work_registry.claim_row(
            rows,
            "new-area",
            "BISCUIT",
            "none",
            "test scope",
            "file_a,file_b",
            "notes",
        )
        agent_work_registry.save_registry(rows, prefix, suffix, registry_path)

        _, updated_rows, _ = agent_work_registry.load_registry(registry_path)
        row = agent_work_registry.find_row(updated_rows, "new-area")
        self.assertEqual(row.status, "active")
        self.assertEqual(row.owner, "BISCUIT")
        self.assertEqual(row.lane, "none")

    def test_claim_existing_active_area_requires_force_for_other_owner(self):
        """PASS: active work owned by someone else cannot be stolen accidentally."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        with self.assertRaisesRegex(ValueError, "already active"):
            agent_work_registry.claim_row(
                rows,
                "player-trade-validation",
                "BISCUIT",
                "any",
                "overlap",
                "x",
                "x",
            )

    def test_release_row_sets_available(self):
        """PASS: release resets row status and owner."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        row = agent_work_registry.find_row(rows, "kamadan-bridge")
        agent_work_registry.release_row(row, notes="released")
        self.assertEqual(row.status, "available")
        self.assertEqual(row.owner, "-")
        self.assertEqual(row.notes, "released")


def _run_case(case_type: type[unittest.TestCase]) -> None:
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(case_type)
    result = unittest.TestResult()
    suite.run(result)
    if not result.wasSuccessful():
        details = []
        for test, message in result.failures + result.errors:
            details.append(f"{test.id()}: {message.splitlines()[-1] if message else 'unknown failure'}")
        raise TestFailure("; ".join(details))


async def test_agent_work_registry_suite(_tc):
    """Bridge-runner wrapper: validate the work registry helper and registry shape."""
    _run_case(TestAgentWorkRegistry)


test_agent_work_registry_suite.requires_bridge = False
test_agent_work_registry_suite.__test__ = False


if __name__ == "__main__":
    unittest.main()
