"""Unit tests for configurable player-trade harness lane selection."""

from __future__ import annotations

import os
import threading
import unittest
from pathlib import Path
from unittest.mock import AsyncMock, patch

from .helpers import TestFailure


def _run_case(case_type: type[unittest.TestCase]) -> None:
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(case_type)
    result = unittest.TestResult()
    suite.run(result)
    if not result.wasSuccessful():
        details = []
        for test, message in result.failures + result.errors:
            details.append(f"{test.id()}: {message.splitlines()[-1] if message else 'unknown failure'}")
        raise TestFailure("; ".join(details))


class TestTradeHarnessConfig(unittest.TestCase):
    def test_trade_harness_defaults_match_trade_lane(self):
        """PASS: harness keeps its historical trade-lane defaults when no env overrides exist."""
        from . import trade_harness

        with patch.dict(os.environ, {}, clear=False):
            self.assertEqual(trade_harness._main_name(), trade_harness.DISCO_NAME)
            self.assertEqual(trade_harness._helper_name(), trade_harness.HELPER_NAME)
            self.assertEqual(trade_harness._main_launcher(), trade_harness.DEFAULT_MAIN_LAUNCHER)
            self.assertEqual(trade_harness._helper_launcher(), trade_harness.DEFAULT_HELPER_LAUNCHER)
            self.assertEqual(trade_harness._main_log(), trade_harness.DEFAULT_MAIN_LOG)
            self.assertEqual(trade_harness._helper_log(), trade_harness.DEFAULT_HELPER_LOG)

    def test_trade_harness_honors_env_overrides(self):
        """PASS: harness can target an isolated lane via env vars instead of hard-coded Disco/BLUMPKINS values."""
        from . import trade_harness

        fake_main_launcher = Path(r"C:\tmp\launch_marvin_via_gwlauncher.au3")
        fake_helper_launcher = Path(r"C:\tmp\launch_helper_via_gwlauncher.au3")
        fake_main_log = Path(r"C:\tmp\launch_marvin.log")
        fake_helper_log = Path(r"C:\tmp\launch_helper.log")

        with patch.dict(
            os.environ,
            {
                "GWA3_TRADE_MAIN_NAME": "Starvin M A R V I N",
                "GWA3_TRADE_HELPER_NAME": "Trade Helper",
                "GWA3_TRADE_MAIN_LAUNCHER": str(fake_main_launcher),
                "GWA3_TRADE_HELPER_LAUNCHER": str(fake_helper_launcher),
                "GWA3_TRADE_MAIN_LOG": str(fake_main_log),
                "GWA3_TRADE_HELPER_LOG": str(fake_helper_log),
            },
            clear=False,
        ):
            self.assertEqual(trade_harness._main_name(), "Starvin M A R V I N")
            self.assertEqual(trade_harness._helper_name(), "Trade Helper")
            self.assertEqual(trade_harness._main_launcher(), fake_main_launcher)
            self.assertEqual(trade_harness._helper_launcher(), fake_helper_launcher)
            self.assertEqual(trade_harness._main_log(), fake_main_log)
            self.assertEqual(trade_harness._helper_log(), fake_helper_log)

    def test_default_trade_lane_config_is_valid(self):
        """PASS: historical trade-lane defaults remain an allowed two-client configuration."""
        from . import trade_harness

        with patch.dict(os.environ, {}, clear=False):
            trade_harness._validate_trade_lane_config()

    def test_custom_main_lane_requires_explicit_helper_lane(self):
        """PASS: custom main/build/pipe settings fail fast unless helper lane overrides are also explicit."""
        from . import trade_harness

        with patch.dict(
            os.environ,
            {
                "GWA3_BUILD_DIR": r"C:\custom\build_marvin",
                "GWA3_DLL_NAME": "gwa3_marvin.dll",
                "GWA3_PIPE_NAME": r"\\.\pipe\gwa3_llm_marvin",
                "GWA3_TRADE_MAIN_NAME": "Starvin M A R V I N",
                "GWA3_TRADE_MAIN_LAUNCHER": r"C:\tmp\launch_marvin_via_gwlauncher.au3",
                "GWA3_TRADE_MAIN_LOG": r"C:\tmp\launch_marvin.log",
            },
            clear=False,
        ):
            with self.assertRaisesRegex(Exception, "helper overrides"):
                trade_harness._validate_trade_lane_config()

    def test_custom_main_lane_with_helper_lane_is_valid(self):
        """PASS: fully specified isolated two-client lanes are accepted by the harness."""
        from . import trade_harness

        with patch.dict(
            os.environ,
            {
                "GWA3_BUILD_DIR": r"C:\custom\build_marvin",
                "GWA3_DLL_NAME": "gwa3_marvin.dll",
                "GWA3_PIPE_NAME": r"\\.\pipe\gwa3_llm_marvin",
                "GWA3_TRADE_MAIN_NAME": "Starvin M A R V I N",
                "GWA3_TRADE_MAIN_LAUNCHER": r"C:\tmp\launch_marvin_via_gwlauncher.au3",
                "GWA3_TRADE_MAIN_LOG": r"C:\tmp\launch_marvin.log",
                "GWA3_TRADE_HELPER_NAME": "Trade Helper",
                "GWA3_TRADE_HELPER_LAUNCHER": r"C:\tmp\launch_helper_via_gwlauncher.au3",
                "GWA3_TRADE_HELPER_LOG": r"C:\tmp\launch_helper.log",
            },
            clear=False,
        ):
            trade_harness._validate_trade_lane_config()


class TestTradeRunnerConfig(unittest.TestCase):
    def test_player_trade_runner_preserves_preconfigured_lane_env(self):
        """PASS: runner no longer stomps a caller-provided build dir, DLL, or pipe for player-trade tests."""
        from . import runner

        async def fake_test(_tc):
            return None

        with patch.object(runner.BridgeTestCase, "setUp", new=AsyncMock(return_value=None)), patch.object(
            runner.BridgeTestCase, "tearDown", new=AsyncMock(return_value=None)
        ), patch("bridge.tests.trade_harness.ensure_trade_main_running", new=AsyncMock(return_value=1234)), patch(
            "bridge.tests.trade_harness.cleanup_trade_clients"
        ), patch.dict(
            os.environ,
            {
                "GWA3_BUILD_DIR": r"C:\custom\build_marvin",
                "GWA3_DLL_NAME": "gwa3_marvin.dll",
                "GWA3_PIPE_NAME": r"\\.\pipe\gwa3_llm_marvin",
            },
            clear=False,
        ):
            holder: dict[str, object] = {}

            def run_in_thread():
                try:
                    holder["result"] = runner.asyncio.run(
                        runner.run_single_test("test_player_trade_dummy", fake_test, timeout=0.1)
                    )
                except Exception as exc:
                    holder["error"] = exc

            thread = threading.Thread(target=run_in_thread)
            thread.start()
            thread.join(timeout=5.0)
            self.assertFalse(thread.is_alive(), "runner thread should complete promptly")
            if "error" in holder:
                raise holder["error"]
            result = holder["result"]
            self.assertEqual(os.environ["GWA3_BUILD_DIR"], r"C:\custom\build_marvin")
            self.assertEqual(os.environ["GWA3_DLL_NAME"], "gwa3_marvin.dll")
            self.assertEqual(os.environ["GWA3_PIPE_NAME"], r"\\.\pipe\gwa3_llm_marvin")

        self.assertEqual(result[0], "PASS")


async def test_trade_harness_config_suite(_tc):
    """Bridge-runner wrapper: enforce trade harness config invariants in the default test path."""
    _run_case(TestTradeHarnessConfig)
    _run_case(TestTradeRunnerConfig)

test_trade_harness_config_suite.requires_bridge = False


if __name__ == "__main__":
    unittest.main()
