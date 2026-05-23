"""Unit tests for the work registry and helper script."""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from datetime import datetime, timezone
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

| Work Area | Status | Owner | Lane | Heartbeat | Scope | Primary Files | Notes |
|---|---|---|---|---|---|---|---|
| `agent-coordination` | `active` | `BISCUIT` | `none` | `2026-05-23T23:45:00Z` | registry tests fixture | `AGENT_WORK_REGISTRY.md` | active for fixture |
| `kamadan-bridge` | `available` | `-` | `none` | `2026-05-23T23:45:00Z` | bridge fixture | `gwa3/bridge` | bridge work stable |
| `player-trade-validation` | `active` | `CODEX` | `any` | `2026-05-23T23:45:00Z` | player trade fixture | `gwa3/bridge/tests/test_f_player_trade.py` | active for fixture |
| `stale-active` | `active` | `CODEX` | `disco` | `2026-05-23T23:20:00Z` | stale active fixture | `file` | stale active |
| `fresh-active` | `active` | `CODEX` | `marvin` | `2026-05-23T23:44:00Z` | fresh active fixture | `file` | fresh active |
| `stale-blocked` | `blocked` | `CODEX` | `beastrit` | `2026-05-23T23:19:00Z` | stale blocked fixture | `file` | stale blocked |
| `malformed-heartbeat` | `active` | `CODEX` | `biscuit` | `not-a-time` | malformed fixture | `file` | malformed heartbeat |
"""

CHECK_REGISTRY_TEMPLATE = """# Agent Work Registry

| Work Area | Status | Owner | Lane | Heartbeat | Scope | Primary Files | Notes |
|---|---|---|---|---|---|---|---|
{rows}
"""


class TestAgentWorkRegistry(unittest.TestCase):
    def _temp_registry(self) -> Path:
        with tempfile.NamedTemporaryFile(suffix="_agent_work_registry.md", delete=False) as tmp:
            path = Path(tmp.name)
        path.write_text(WORK_REGISTRY_FIXTURE, encoding="utf-8")
        return path

    def _temp_registry_with_rows(self, rows: str) -> Path:
        with tempfile.NamedTemporaryFile(suffix="_agent_work_registry.md", delete=False) as tmp:
            path = Path(tmp.name)
        path.write_text(CHECK_REGISTRY_TEMPLATE.format(rows=rows), encoding="utf-8")
        return path

    def _write_session(self, directory: Path, name: str, lane: str) -> None:
        directory.mkdir(parents=True, exist_ok=True)
        payload = {
            "lane": lane,
            "status": "running",
            "gw_pid": int(name),
            "character": f"{lane}-character",
        }
        (directory / f"{name}.json").write_text(json.dumps(payload), encoding="utf-8")

    def test_list_rows_filters_active(self):
        """PASS: list_rows can show only active work areas."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        output = agent_work_registry.list_rows(rows, status="active")
        self.assertIn("agent-coordination", output)
        self.assertIn("player-trade-validation", output)

    def test_claim_existing_available_then_release(self):
        """PASS: claiming and releasing an available row updates owner and status."""
        registry_path = self._temp_registry()
        prefix, rows, suffix = agent_work_registry.load_registry(registry_path)
        agent_work_registry.claim_row(
            rows,
            "kamadan-bridge",
            "BISCUIT",
            "none",
            "test scope",
            "file_a,file_b",
            "notes",
        )
        agent_work_registry.save_registry(rows, prefix, suffix, registry_path)

        _, updated_rows, _ = agent_work_registry.load_registry(registry_path)
        row = agent_work_registry.find_row(updated_rows, "kamadan-bridge")
        self.assertEqual(row.status, "active")
        self.assertEqual(row.owner, "BISCUIT")
        self.assertEqual(row.lane, "none")

        agent_work_registry.release_row(row, owner="BISCUIT", notes="released")
        self.assertEqual(row.status, "available")
        self.assertEqual(row.owner, "-")
        self.assertEqual(row.notes, "released")

    def test_double_claim_refused(self):
        """PASS: active work cannot be claimed twice."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        with self.assertRaisesRegex(ValueError, "not available"):
            agent_work_registry.claim_row(
                rows,
                "player-trade-validation",
                "BISCUIT",
                "any",
                "overlap",
                "x",
                "x",
            )

    def test_lane_collision_refused_with_colliding_row(self):
        """PASS: an available row cannot claim a lane held by another row."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        with self.assertRaisesRegex(ValueError, "stale-active"):
            agent_work_registry.claim_row(
                rows,
                "kamadan-bridge",
                "BISCUIT",
                "disco",
                "overlap",
                "x",
                "x",
            )

    def test_cli_lane_collision_refused_with_diagnostic(self):
        """PASS: the CLI diagnostic names the colliding row."""
        registry_path = self._temp_registry()
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT_PATH),
                "claim",
                "kamadan-bridge",
                "--lane",
                "disco",
                "--owner",
                "BISCUIT",
                "--registry",
                str(registry_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("lane collision", result.stderr)
        self.assertIn("stale-active", result.stderr)

    def test_release_by_wrong_owner_refused(self):
        """PASS: a row can only be released by its current owner."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        row = agent_work_registry.find_row(rows, "player-trade-validation")
        with self.assertRaisesRegex(ValueError, "owner mismatch"):
            agent_work_registry.release_row(row, owner="BISCUIT", notes="released")

    def test_cli_waits_for_registry_file_lock(self):
        """PASS: concurrent CLI attempts wait for the registry lock."""
        registry_path = self._temp_registry()
        holder_code = f"""
import importlib.util
import sys
import time
from pathlib import Path
spec = importlib.util.spec_from_file_location('agent_work_registry_script', {str(SCRIPT_PATH)!r})
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)
with module.lock_registry(Path({str(registry_path)!r}), timeout_seconds=5):
    print('locked', flush=True)
    time.sleep(1.0)
"""
        holder = subprocess.Popen(
            [sys.executable, "-c", holder_code],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            self.assertEqual(holder.stdout.readline().strip(), "locked")
            started = time.monotonic()
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT_PATH),
                    "claim",
                    "kamadan-bridge",
                    "--lane",
                    "none",
                    "--owner",
                    "BISCUIT",
                    "--registry",
                    str(registry_path),
                    "--lock-timeout-seconds",
                    "5",
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )
            elapsed = time.monotonic() - started
        finally:
            holder.wait(timeout=10)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertGreaterEqual(elapsed, 0.8)

    def test_prune_stale_active_sets_available(self):
        """PASS: stale active rows are released by the prune rule."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)
        agent_work_registry.prune_stale_rows(rows, max_age_minutes=15, now=now, replacement_heartbeat="now")
        row = agent_work_registry.find_row(rows, "stale-active")
        self.assertEqual(row.status, "available")
        self.assertEqual(row.owner, "-")

    def test_prune_fresh_active_keeps_claim(self):
        """PASS: fresh active rows remain claimed."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)
        agent_work_registry.prune_stale_rows(rows, max_age_minutes=15, now=now, replacement_heartbeat="now")
        row = agent_work_registry.find_row(rows, "fresh-active")
        self.assertEqual(row.status, "active")
        self.assertEqual(row.owner, "CODEX")

    def test_prune_stale_blocked_sets_available(self):
        """PASS: stale blocked rows are released by the prune rule."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)
        agent_work_registry.prune_stale_rows(rows, max_age_minutes=15, now=now, replacement_heartbeat="now")
        row = agent_work_registry.find_row(rows, "stale-blocked")
        self.assertEqual(row.status, "available")
        self.assertEqual(row.owner, "-")

    def test_prune_malformed_heartbeat_treats_as_stale(self):
        """PASS: malformed heartbeat values are stale."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)
        agent_work_registry.prune_stale_rows(rows, max_age_minutes=15, now=now, replacement_heartbeat="now")
        row = agent_work_registry.find_row(rows, "malformed-heartbeat")
        self.assertEqual(row.status, "available")
        self.assertEqual(row.owner, "-")

    def test_check_flags_active_lane_without_live_session(self):
        """PASS: check reports an active registry lane with no live DLL session."""
        registry_path = self._temp_registry_with_rows(
            "| `active-beastrit` | `active` | `CODEX` | `beastrit` | `2026-05-23T23:45:00Z` | scope | `file` | - |"
        )
        _, rows, _ = agent_work_registry.load_registry(registry_path)
        with tempfile.TemporaryDirectory() as tmp:
            sessions = agent_work_registry.load_session_registry(Path(tmp))
        issues = agent_work_registry.find_session_mismatches(rows, sessions)
        self.assertTrue(any("active-without-live-session lane=beastrit" in issue for issue in issues))

    def test_check_flags_live_session_without_active_claim(self):
        """PASS: check reports a live DLL session whose lane has no active work row."""
        registry_path = self._temp_registry_with_rows(
            "| `available-disco` | `available` | `-` | `disco` | `2026-05-23T23:45:00Z` | scope | `file` | - |"
        )
        _, rows, _ = agent_work_registry.load_registry(registry_path)
        with tempfile.TemporaryDirectory() as tmp:
            session_dir = Path(tmp)
            self._write_session(session_dir, "1234", "disco")
            sessions = agent_work_registry.load_session_registry(session_dir)
        issues = agent_work_registry.find_session_mismatches(rows, sessions)
        self.assertTrue(any("live-session-without-active-claim lane=disco" in issue for issue in issues))

    def test_check_flags_multiple_active_rows_same_lane(self):
        """PASS: check reports duplicate active work rows on one lane."""
        registry_path = self._temp_registry_with_rows(
            "\n".join(
                [
                    "| `first-disco` | `active` | `CODEX` | `disco` | `2026-05-23T23:45:00Z` | scope | `file` | - |",
                    "| `second-disco` | `active` | `BISCUIT` | `disco` | `2026-05-23T23:45:00Z` | scope | `file` | - |",
                ]
            )
        )
        _, rows, _ = agent_work_registry.load_registry(registry_path)
        with tempfile.TemporaryDirectory() as tmp:
            session_dir = Path(tmp)
            self._write_session(session_dir, "1234", "disco")
            sessions = agent_work_registry.load_session_registry(session_dir)
        issues = agent_work_registry.find_session_mismatches(rows, sessions)
        self.assertTrue(any("multiple-active-rows lane=disco" in issue for issue in issues))

    def test_check_warn_only_exits_zero_on_mismatch(self):
        """PASS: --warn-only prints mismatches but exits successfully."""
        registry_path = self._temp_registry_with_rows(
            "| `available-disco` | `available` | `-` | `disco` | `2026-05-23T23:45:00Z` | scope | `file` | - |"
        )
        with tempfile.TemporaryDirectory() as tmp:
            session_dir = Path(tmp)
            self._write_session(session_dir, "1234", "disco")
            env = dict(os.environ)
            env["GWA3_SESSION_REGISTRY_DIR"] = str(session_dir)
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT_PATH),
                    "check",
                    "--registry",
                    str(registry_path),
                    "--warn-only",
                ],
                capture_output=True,
                text=True,
                timeout=10,
                env=env,
            )
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("MISMATCH live-session-without-active-claim lane=disco", result.stdout)


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
