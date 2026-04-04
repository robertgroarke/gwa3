"""Test runner — discovers, executes, and reports bridge integration tests."""

from __future__ import annotations

import asyncio
import importlib
import inspect
import sys
import time
import traceback
from fnmatch import fnmatch

from .base import BridgeTestCase, TestSkipped
from .helpers import TestFailure

# Default per-test timeout
DEFAULT_TIMEOUT = 30.0


def discover_tests(module_names: list[str], filter_pattern: str | None = None) -> list[tuple[str, callable]]:
    """Find all async test functions in the given modules.

    Test functions are any `async def test_*` in modules.
    Returns list of (test_name, test_func) tuples.
    """
    tests = []
    for mod_name in module_names:
        mod = importlib.import_module(f".{mod_name}", package="bridge.tests")
        for name, func in inspect.getmembers(mod, inspect.isfunction):
            if name.startswith("test_") and asyncio.iscoroutinefunction(func):
                if filter_pattern and not fnmatch(name, filter_pattern):
                    continue
                tests.append((name, func))
    return tests


async def run_single_test(name: str, func: callable, timeout: float = DEFAULT_TIMEOUT) -> tuple[str, str, float]:
    """Run a single test function. Returns (status, detail, elapsed_seconds).

    status: 'PASS', 'FAIL', or 'SKIP'
    """
    tc = BridgeTestCase()
    start = time.monotonic()
    try:
        await asyncio.wait_for(tc.setUp(), timeout=timeout)
        try:
            await asyncio.wait_for(func(tc), timeout=timeout)
            elapsed = time.monotonic() - start
            return ("PASS", "", elapsed)
        except TestSkipped as e:
            elapsed = time.monotonic() - start
            return ("SKIP", str(e), elapsed)
        except TestFailure as e:
            elapsed = time.monotonic() - start
            return ("FAIL", str(e), elapsed)
        except asyncio.TimeoutError:
            elapsed = time.monotonic() - start
            return ("FAIL", f"Timed out after {timeout}s", elapsed)
        except Exception as e:
            elapsed = time.monotonic() - start
            return ("FAIL", f"{type(e).__name__}: {e}", elapsed)
        finally:
            try:
                await asyncio.wait_for(tc.tearDown(), timeout=5.0)
            except Exception:
                pass
    except Exception as e:
        elapsed = time.monotonic() - start
        return ("FAIL", f"setUp failed: {e}", elapsed)


async def run_all(filter_pattern: str | None = None):
    """Discover and run all bridge tests."""
    module_names = [
        "test_a_ipc",
        "test_b_observations",
        "test_c_actions",
        "test_d_validation",
    ]

    tests = discover_tests(module_names, filter_pattern)
    if not tests:
        print("[!] No tests found" + (f" matching '{filter_pattern}'" if filter_pattern else ""))
        return 1

    print(f"=== GWA3 Bridge Integration Tests ===")
    print(f"Running {len(tests)} test(s)...\n")

    passed = 0
    failed = 0
    skipped = 0
    results = []

    for name, func in tests:
        status, detail, elapsed = await run_single_test(name, func)

        if status == "PASS":
            passed += 1
            print(f"  [PASS] {name} ({elapsed:.1f}s)")
        elif status == "SKIP":
            skipped += 1
            print(f"  [SKIP] {name} — {detail}")
        else:
            failed += 1
            print(f"  [FAIL] {name} — {detail}")

        results.append((name, status, detail, elapsed))

    print(f"\n=== Results ===")
    print(f"Passed: {passed} / Failed: {failed} / Skipped: {skipped} / Total: {len(tests)}")

    return 0 if failed == 0 else 1
