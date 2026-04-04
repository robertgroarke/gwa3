"""Shared test utilities and assertion helpers."""

from __future__ import annotations


class TestFailure(Exception):
    """Raised when a test assertion fails."""


def snapshot_get(snap: dict, dotpath: str):
    """Navigate a nested dict with a dotted path. E.g., 'me.hp' or 'party.members.0.is_alive'."""
    parts = dotpath.split(".")
    current = snap
    for part in parts:
        if isinstance(current, dict):
            if part not in current:
                return None
            current = current[part]
        elif isinstance(current, list):
            try:
                current = current[int(part)]
            except (IndexError, ValueError):
                return None
        else:
            return None
    return current


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


def assert_equal(actual, expected, label: str = ""):
    prefix = f"{label}: " if label else ""
    if actual != expected:
        raise TestFailure(f"{prefix}expected {expected!r}, got {actual!r}")


def assert_gt(value, threshold, label: str = ""):
    prefix = f"{label}: " if label else ""
    if not value > threshold:
        raise TestFailure(f"{prefix}expected > {threshold}, got {value}")


def assert_gte(value, threshold, label: str = ""):
    prefix = f"{label}: " if label else ""
    if not value >= threshold:
        raise TestFailure(f"{prefix}expected >= {threshold}, got {value}")
