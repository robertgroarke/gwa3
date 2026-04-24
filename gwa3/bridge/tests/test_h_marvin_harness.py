"""Unit tests for the MARVIN-only smoke harness safety logic."""

import asyncio
import json
import tempfile
import unittest
from pathlib import Path

from .marvin_harness import (
    BEASTRIT_LANE,
    MARVIN_LANE,
    _acquire_run_lock,
    _build_dir,
    _choose_best_pid,
    _lock_path,
    _parse_pid_from_log,
    _parse_pid_from_text,
    _pick_target_candidate,
    _read_lock_payload,
    _read_until_action_result,
    _read_until_target_id,
    _read_until_tier2_with_candidate,
    _read_until_snapshot_tick_gt,
    _release_run_lock,
    _snapshot_is_world_ready,
    _snapshot_world_state,
    get_smoke_lane,
)
from .helpers import TestFailure


class TestMarvinHarnessHelpers(unittest.TestCase):
    def test_get_smoke_lane_returns_known_lane(self):
        self.assertIs(get_smoke_lane("marvin"), MARVIN_LANE)
        self.assertIs(get_smoke_lane("beastrit"), BEASTRIT_LANE)

    def test_get_smoke_lane_rejects_unknown_lane(self):
        with self.assertRaises(TestFailure):
            get_smoke_lane("unknown")

    def test_parse_pid_from_text(self):
        self.assertEqual(_parse_pid_from_text("hello\nGWLAUNCHER_PID=12345\nbye"), 12345)

    def test_parse_pid_from_text_returns_none_when_missing(self):
        self.assertIsNone(_parse_pid_from_text("no pid here"))

    def test_parse_pid_from_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            log_path = Path(tmp) / "launch_marvin.log"
            log_path.write_text("=== Launch ===\nGWLAUNCHER_PID=9876\n", encoding="utf-8")
            self.assertEqual(_parse_pid_from_log(log_path), 9876)

    def test_parse_pid_from_log_raises_on_missing_pid(self):
        with tempfile.TemporaryDirectory() as tmp:
            log_path = Path(tmp) / "launch_marvin.log"
            log_path.write_text("=== Launch ===\n", encoding="utf-8")
            with self.assertRaises(TestFailure):
                _parse_pid_from_log(log_path)

    def test_choose_best_pid_prefers_highest_memory_healthy_process(self):
        rows = [{"pid": 11}, {"pid": 22}, {"pid": 33}]
        mem = {11: 90000, 22: 120000, 33: 180000}
        running = {11: True, 22: False, 33: True}
        best = _choose_best_pid(
            rows,
            min_memory_kb=100000,
            memory_fn=lambda pid: mem[pid],
            running_fn=lambda pid: running[pid],
        )
        self.assertEqual(best, 33)

    def test_choose_best_pid_returns_none_when_no_healthy_candidate(self):
        rows = [{"pid": 11}, {"pid": 22}]
        best = _choose_best_pid(
            rows,
            min_memory_kb=100000,
            memory_fn=lambda _pid: 50000,
            running_fn=lambda _pid: True,
        )
        self.assertIsNone(best)

    def test_run_lock_rejects_second_acquire(self):
        lock = _acquire_run_lock()
        try:
            with self.assertRaises(TestFailure):
                _acquire_run_lock()
        finally:
            _release_run_lock(lock)

    def test_read_lock_payload_returns_none_for_bad_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "lock.json"
            path.write_text("{bad", encoding="utf-8")
            self.assertIsNone(_read_lock_payload(path))

    def test_run_lock_replaces_stale_owner(self):
        lock_path = _lock_path(MARVIN_LANE)
        lock_path.parent.mkdir(parents=True, exist_ok=True)
        lock_path.write_text(json.dumps({"pid": 999999, "lane": "marvin"}), encoding="utf-8")
        try:
            lock = _acquire_run_lock()
            self.assertEqual(lock, lock_path)
        finally:
            _release_run_lock(lock_path)

    def test_lane_specific_lock_paths_are_isolated(self):
        marvin_lock = _lock_path(MARVIN_LANE)
        beastrit_lock = _lock_path(BEASTRIT_LANE)
        self.assertNotEqual(marvin_lock, beastrit_lock)
        self.assertEqual(marvin_lock.name, "marvin_bridge_smoke.lock")
        self.assertEqual(beastrit_lock.name, "beastrit_bridge_smoke.lock")

    def test_lane_specific_build_dir_defaults(self):
        self.assertEqual(_build_dir(MARVIN_LANE), MARVIN_LANE.default_build_dir)
        self.assertEqual(_build_dir(BEASTRIT_LANE), BEASTRIT_LANE.default_build_dir)

    def test_snapshot_world_state_uses_nested_map_and_me_fields(self):
        state = _snapshot_world_state({
            "tier": 1,
            "tick": 7,
            "map": {"map_id": 449, "loading_state": 1, "is_loaded": True},
            "me": {"agent_id": 1234},
        })
        self.assertEqual(state["map_id"], 449)
        self.assertEqual(state["loading_state"], 1)
        self.assertTrue(state["is_loaded"])
        self.assertEqual(state["agent_id"], 1234)
        self.assertEqual(state["tick"], 7)

    def test_snapshot_is_world_ready_requires_loaded_map_and_agent(self):
        ready = {
            "map": {"map_id": 449, "loading_state": 1, "is_loaded": True},
            "me": {"agent_id": 1234},
        }
        not_ready = {
            "map": {"map_id": 0, "loading_state": 0, "is_loaded": False},
            "me": {"agent_id": 0},
        }
        self.assertTrue(_snapshot_is_world_ready(ready))
        self.assertFalse(_snapshot_is_world_ready(not_ready))

    def test_pick_target_candidate_prefers_nearest_noncurrent_agent(self):
        snap = {
            "me": {"target_id": 22},
            "agents": [
                {"id": 22, "distance": 10.0},
                {"id": 33, "distance": 90.0},
                {"id": 44, "distance": 15.0},
            ],
        }
        self.assertEqual(_pick_target_candidate(snap), 44)


class _FakeIpc:
    def __init__(self, messages):
        self._messages = list(messages)

    async def read_message(self, timeout=None):
        await asyncio.sleep(0)
        if self._messages:
            return self._messages.pop(0)
        return None


class TestMarvinHarnessAsyncReaders(unittest.TestCase):
    def test_read_until_action_result_matches_request_id(self):
        async def _case():
            ipc = _FakeIpc([
                {"type": "snapshot", "tick": 1},
                {"type": "action_result", "request_id": "wrong", "success": True},
                {"type": "action_result", "request_id": "abc123", "success": True},
            ])
            msg = await _read_until_action_result(ipc, "abc123", timeout=0.5)
            self.assertEqual(msg["request_id"], "abc123")

        asyncio.run(_case())

    def test_read_until_action_result_times_out(self):
        async def _case():
            ipc = _FakeIpc([{"type": "heartbeat"}])
            with self.assertRaises(TestFailure):
                await _read_until_action_result(ipc, "missing", timeout=0.05)

        asyncio.run(_case())

    def test_read_until_snapshot_tick_gt_returns_newer_snapshot(self):
        async def _case():
            ipc = _FakeIpc([
                {"type": "snapshot", "tick": 10},
                {"type": "heartbeat"},
                {"type": "snapshot", "tick": 11},
            ])
            msg = await _read_until_snapshot_tick_gt(ipc, 10, timeout=0.5)
            self.assertEqual(msg["tick"], 11)

        asyncio.run(_case())

    def test_read_until_tier2_with_candidate_returns_candidate(self):
        async def _case():
            ipc = _FakeIpc([
                {"type": "snapshot", "tier": 1, "tick": 1, "me": {"target_id": 0}, "agents": []},
                {
                    "type": "snapshot",
                    "tier": 2,
                    "tick": 2,
                    "me": {"target_id": 0},
                    "agents": [{"id": 55, "distance": 12.0}],
                },
            ])
            snap, candidate = await _read_until_tier2_with_candidate(ipc, timeout=0.5)
            self.assertEqual(candidate, 55)
            self.assertEqual(snap["tick"], 2)

        asyncio.run(_case())

    def test_read_until_target_id_observes_confirmation(self):
        async def _case():
            ipc = _FakeIpc([
                {"type": "snapshot", "tick": 2, "me": {"target_id": 10}},
                {"type": "snapshot", "tick": 3, "me": {"target_id": 55}},
            ])
            msg = await _read_until_target_id(ipc, 55, timeout=0.5)
            self.assertEqual(msg["tick"], 3)

        asyncio.run(_case())
