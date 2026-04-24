"""Unit tests for MARVIN soak aggregation logic."""

import unittest

from .marvin_soak import _status_for_run


class TestMarvinSoak(unittest.TestCase):
    def test_status_pass_when_ok_and_no_events(self):
        self.assertEqual(
            _status_for_run({"ok": True, "action_success": True, "target_action_success": True}, []),
            "PASS",
        )

    def test_status_fail_when_not_ok(self):
        self.assertEqual(_status_for_run({"ok": False}, []), "FAIL")

    def test_status_fail_when_first_action_missing(self):
        self.assertEqual(
            _status_for_run({"ok": True, "action_success": False, "target_action_success": True}, []),
            "FAIL",
        )

    def test_status_fail_when_target_action_missing(self):
        self.assertEqual(
            _status_for_run({"ok": True, "action_success": True, "target_action_success": False}, []),
            "FAIL",
        )

    def test_status_warn_when_ok_but_events_present(self):
        self.assertEqual(
            _status_for_run({"ok": True, "action_success": True, "target_action_success": True}, ["minor"]),
            "WARN",
        )
