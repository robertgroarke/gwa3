"""Shared test utilities and assertion helpers."""

from __future__ import annotations


class TestFailure(Exception):
    """Raised when a test assertion fails."""


def assert_keys_present(d: dict, keys: list[str], label: str = ""):
    """Verify all keys exist in a dict."""
    prefix = f"{label}: " if label else ""
    for key in keys:
        if key not in d:
            raise TestFailure(f"{prefix}missing key '{key}' (got keys: {list(d.keys())})")


def assert_type(value, expected_type, label: str = ""):
    """Type check with descriptive failure."""
    prefix = f"{label}: " if label else ""
    if not isinstance(value, expected_type):
        raise TestFailure(
            f"{prefix}expected {expected_type.__name__}, got {type(value).__name__} ({value!r})"
        )


def assert_in_range(value, low, high, label: str = ""):
    """Numeric range check."""
    prefix = f"{label}: " if label else ""
    if value < low or value > high:
        raise TestFailure(f"{prefix}value {value} not in range [{low}, {high}]")


def assert_true(condition: bool, message: str = "assertion failed"):
    if not condition:
        raise TestFailure(message)


def assert_gt(value, threshold, label: str = ""):
    prefix = f"{label}: " if label else ""
    if not value > threshold:
        raise TestFailure(f"{prefix}expected > {threshold}, got {value}")


def assert_gte(value, threshold, label: str = ""):
    prefix = f"{label}: " if label else ""
    if not value >= threshold:
        raise TestFailure(f"{prefix}expected >= {threshold}, got {value}")
