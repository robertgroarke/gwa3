"""Unit tests for the work registry and helper script."""

from __future__ import annotations

import importlib.util
import io
import json
import os
import shutil
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
WORKSPACE_SCRIPT_PATH = REPO_ROOT / "scripts" / "agent_workspace.py"
_SPEC = importlib.util.spec_from_file_location("agent_work_registry_script", SCRIPT_PATH)
agent_work_registry = importlib.util.module_from_spec(_SPEC)
assert _SPEC is not None and _SPEC.loader is not None
sys.modules[_SPEC.name] = agent_work_registry
_SPEC.loader.exec_module(agent_work_registry)
_WORKSPACE_SPEC = importlib.util.spec_from_file_location("agent_workspace_script", WORKSPACE_SCRIPT_PATH)
agent_workspace = importlib.util.module_from_spec(_WORKSPACE_SPEC)
assert _WORKSPACE_SPEC is not None and _WORKSPACE_SPEC.loader is not None
sys.modules[_WORKSPACE_SPEC.name] = agent_workspace
_WORKSPACE_SPEC.loader.exec_module(agent_workspace)

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

    def test_claim_file_overlap_warns_but_succeeds(self):
        """PASS: overlapping primary files warn to stderr but do not block claim."""
        registry_path = self._temp_registry()
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
                "--files",
                " agent_work_registry.md , extra.py",
                "--registry",
                str(registry_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("WARNING", result.stderr)
        self.assertIn("agent-coordination", result.stderr)
        row = agent_work_registry.find_row(agent_work_registry.load_registry(registry_path)[1], "kamadan-bridge")
        self.assertEqual(row.status, "active")

    def test_claim_strict_files_refuses_overlap(self):
        """PASS: --strict-files promotes file overlap warnings to claim refusal."""
        registry_path = self._temp_registry()
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
                "--files",
                "AGENT_WORK_REGISTRY.md",
                "--strict-files",
                "--registry",
                str(registry_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("file overlap", result.stderr)
        row = agent_work_registry.find_row(agent_work_registry.load_registry(registry_path)[1], "kamadan-bridge")
        self.assertEqual(row.status, "available")

    def test_claim_without_file_overlap_has_no_warning(self):
        """PASS: non-overlapping primary files do not emit warnings."""
        registry_path = self._temp_registry()
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
                "--files",
                "new_file.py",
                "--registry",
                str(registry_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn("WARNING", result.stderr)

    def test_claim_lane_collision_precedes_file_overlap(self):
        """PASS: lane collision remains the first refusal even when files overlap."""
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
                "--files",
                "file",
                "--strict-files",
                "--registry",
                str(registry_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("lane collision", result.stderr)
        self.assertNotIn("file overlap", result.stderr)

    def test_release_by_wrong_owner_refused(self):
        """PASS: a row can only be released by its current owner."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        row = agent_work_registry.find_row(rows, "player-trade-validation")
        with self.assertRaisesRegex(ValueError, "owner mismatch"):
            agent_work_registry.release_row(row, owner="BISCUIT", notes="released")

    def test_touch_updates_heartbeat_on_held_active_row(self):
        """PASS: touch refreshes only the heartbeat for a held active row."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        row = agent_work_registry.find_row(rows, "agent-coordination")
        before = row.__dict__.copy()

        agent_work_registry.touch_row(row, owner="BISCUIT", replacement_heartbeat="2026-05-24T00:01:00Z")

        self.assertEqual(row.heartbeat, "2026-05-24T00:01:00Z")
        for field in ["work_area", "status", "owner", "lane", "scope", "primary_files", "notes"]:
            self.assertEqual(getattr(row, field), before[field])

    def test_cli_touch_refuses_owner_mismatch(self):
        """PASS: touch exits non-zero when the requested owner does not hold the row."""
        registry_path = self._temp_registry()
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT_PATH),
                "touch",
                "agent-coordination",
                "--owner",
                "CODEX",
                "--registry",
                str(registry_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("owner mismatch", result.stderr)

    def test_touch_refuses_available_row(self):
        """PASS: touch refuses rows that are not active or blocked."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        row = agent_work_registry.find_row(rows, "kamadan-bridge")
        with self.assertRaisesRegex(ValueError, "not held"):
            agent_work_registry.touch_row(row, owner="-")

    def test_cli_touch_refuses_unknown_work_area(self):
        """PASS: touch exits non-zero when the work area does not exist."""
        registry_path = self._temp_registry()
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT_PATH),
                "touch",
                "missing-area",
                "--owner",
                "CODEX",
                "--registry",
                str(registry_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("work area not found", result.stderr)

    def test_status_snapshot_output_contains_every_row(self):
        """PASS: status snapshot output contains every registry row."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)
        output = agent_work_registry.format_status_table(rows, now=now)

        for work_area in [
            "agent-coordination",
            "kamadan-bridge",
            "player-trade-validation",
            "stale-active",
            "fresh-active",
            "stale-blocked",
            "malformed-heartbeat",
        ]:
            self.assertIn(work_area, output)

    def test_status_lane_filter_restricts_output(self):
        """PASS: status can restrict output to one lane."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)
        output = agent_work_registry.format_status_table(rows, lane_filter="disco", now=now)

        self.assertIn("stale-active", output)
        self.assertNotIn("agent-coordination", output)
        self.assertNotIn("fresh-active", output)

    def test_status_heartbeat_age_uses_fixed_now(self):
        """PASS: heartbeat age is computed in whole minutes from a fixed now."""
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)

        age = agent_work_registry.heartbeat_age_minutes("2026-05-23T23:20:00Z", now=now)

        self.assertEqual(age, "25")

    def test_status_malformed_heartbeat_shows_stale(self):
        """PASS: status renders malformed heartbeat values as stale."""
        _, rows, _ = agent_work_registry.load_registry(self._temp_registry())
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)
        output = agent_work_registry.format_status_table(rows, lane_filter="biscuit", now=now)

        self.assertIn("malformed-heartbeat", output)
        self.assertIn("stale", output)

    def test_status_watch_reprints_until_keyboard_interrupt(self):
        """PASS: watch mode refreshes repeatedly and stops cleanly on Ctrl+C."""
        registry_path = self._temp_registry()
        output = io.StringIO()
        sleeps: list[int] = []
        now = datetime(2026, 5, 23, 23, 45, tzinfo=timezone.utc)

        def sleep_once(seconds: int) -> None:
            sleeps.append(seconds)
            if len(sleeps) == 2:
                raise KeyboardInterrupt

        agent_work_registry.watch_status(
            registry_path,
            sleep_fn=sleep_once,
            now_fn=lambda: now,
            output=output,
        )

        self.assertEqual(sleeps, [5, 5])
        self.assertEqual(output.getvalue().count("\033[H\033[J"), 2)
        self.assertIn("agent-coordination", output.getvalue())

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


class TestAgentWorkspace(unittest.TestCase):
    def _run_git(self, repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", "-C", str(repo), *args],
            capture_output=True,
            check=True,
            text=True,
            timeout=10,
        )

    def _init_repo(self, repo: Path) -> None:
        repo.mkdir(parents=True)
        subprocess.run(["git", "init", "-b", "master", str(repo)], capture_output=True, check=True, text=True, timeout=10)
        self._run_git(repo, "config", "user.email", "agent@example.invalid")
        self._run_git(repo, "config", "user.name", "Agent Test")
        (repo / "README.md").write_text("workspace fixture\n", encoding="utf-8")
        self._run_git(repo, "add", "README.md")
        self._run_git(repo, "commit", "-m", "initial")

    def test_workspace_provision_creates_temp_parent_worktree(self):
        """PASS: provision creates a real worktree at a caller-provided path."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            repo = root / "repo"
            self._init_repo(repo)
            self._run_git(repo, "branch", "feature/provision")
            worktree = root / "parent" / "gwa3-none"

            result = subprocess.run(
                [
                    sys.executable,
                    str(WORKSPACE_SCRIPT_PATH),
                    "--repo",
                    str(repo),
                    "provision",
                    "--lane",
                    "none",
                    "--branch",
                    "feature/provision",
                    "--path",
                    str(worktree),
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertTrue(worktree.exists())
            self.assertIn(str(worktree), result.stdout)
            branch = self._run_git(worktree, "branch", "--show-current").stdout.strip()
            self.assertEqual(branch, "feature/provision")

    def test_workspace_list_parses_porcelain_output(self):
        """PASS: porcelain worktree output is parsed into branch and lane data."""
        output = "\n".join(
            [
                "worktree C:/Users/Robert/Documents/gwa3-disco",
                "HEAD abc123",
                "branch refs/heads/feature/disco",
                "",
                "worktree C:/Users/Robert/Documents/gwa3-private",
                "HEAD def456",
                "branch refs/heads/master",
                "",
            ]
        )
        entries = agent_workspace.parse_worktree_porcelain(output)

        self.assertEqual(len(entries), 2)
        self.assertEqual(entries[0].branch_name, "feature/disco")
        self.assertEqual(agent_workspace.lane_for_path(entries[0].path), "disco")

    def test_workspace_prune_dry_run_identifies_missing_worktree(self):
        """PASS: prune --dry-run reports a missing synthetic worktree without pruning metadata."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            repo = root / "repo"
            self._init_repo(repo)
            self._run_git(repo, "branch", "feature/missing")
            worktree = root / "gwa3-none"
            self._run_git(repo, "worktree", "add", str(worktree), "feature/missing")
            shutil.rmtree(worktree)

            result = subprocess.run(
                [
                    sys.executable,
                    str(WORKSPACE_SCRIPT_PATH),
                    "--repo",
                    str(repo),
                    "prune",
                    "--dry-run",
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertIn("would remove", result.stdout)
            self.assertIn("missing-path", result.stdout)
            self.assertIn(str(worktree).replace("\\", "/"), self._run_git(repo, "worktree", "list", "--porcelain").stdout)

    def test_workspace_prune_apply_removes_stale_worktree(self):
        """PASS: prune --apply removes a worktree whose branch is merged into master."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            repo = root / "repo"
            self._init_repo(repo)
            self._run_git(repo, "branch", "feature/stale")
            worktree = root / "gwa3-none"
            self._run_git(repo, "worktree", "add", str(worktree), "feature/stale")

            result = subprocess.run(
                [
                    sys.executable,
                    str(WORKSPACE_SCRIPT_PATH),
                    "--repo",
                    str(repo),
                    "prune",
                    "--apply",
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertIn("removed", result.stdout)
            self.assertFalse(worktree.exists())
            self.assertNotIn(str(worktree).replace("\\", "/"), self._run_git(repo, "worktree", "list", "--porcelain").stdout)


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
