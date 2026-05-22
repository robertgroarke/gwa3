"""Disconnect detection helpers shared by bridge loops and replay tests."""

from __future__ import annotations

import re
from typing import Any


DISCONNECTED_LOADING_STATE = 2
CODE_RE = re.compile(r"(?:code[_=\s-]*)(\d{3})", re.IGNORECASE)


def _first_str(*values: Any) -> str | None:
    for value in values:
        if value is None:
            continue
        text = str(value).strip()
        if text:
            return text
    return None


def _disconnect_code(connection: dict[str, Any], reason: str | None, state: str | None) -> str | None:
    explicit = _first_str(
        connection.get("likely_disconnect_code"),
        connection.get("disconnect_code"),
        connection.get("code"),
    )
    if explicit:
        digits = re.sub(r"\D+", "", explicit)
        return digits or explicit

    text = " ".join(part for part in (reason, state) if part)
    match = CODE_RE.search(text)
    if match:
        return match.group(1)
    if "007" in text:
        return "007"
    return None


def disconnect_details(
    snapshot: dict[str, Any] | None,
    *,
    heartbeat_age_s: float | None = None,
) -> dict[str, Any] | None:
    """Return compact disconnect telemetry for a snapshot, or ``None``.

    Native snapshots report modern disconnects through ``connection``. Older
    DLLs only exposed MapMgr's disconnected loading state, so keep that
    fallback centralized and visible to replay tests.
    """

    if not snapshot:
        return None

    connection = snapshot.get("connection", {}) or {}
    map_state = snapshot.get("map", {}) or {}
    bot = snapshot.get("bot", {}) or {}

    state = _first_str(connection.get("state"))
    reason = _first_str(connection.get("reason"), connection.get("error"))
    loading_state = map_state.get("loading_state")
    disconnected = bool(connection.get("disconnected", False))
    if loading_state == DISCONNECTED_LOADING_STATE:
        disconnected = True
        reason = reason or "loading_state_disconnected"
    if state and "disconnect" in state.lower():
        disconnected = True

    if not disconnected:
        return None

    code = _disconnect_code(connection, reason, state)
    screenshot_path = _first_str(
        connection.get("screenshot_path"),
        connection.get("watchdog_screenshot"),
        connection.get("last_screenshot_path"),
        bot.get("watchdog_screenshot"),
        bot.get("last_screenshot_path"),
    )

    details = {
        "event": "disconnect_detected",
        "reason": reason or state or "unknown",
        "code": code,
        "connection_state": state,
        "connection_reason": reason,
        "map_id": map_state.get("map_id"),
        "loading_state": loading_state,
        "heartbeat_age_s": heartbeat_age_s,
        "bot_state": bot.get("state"),
        "bot_phase": bot.get("phase") or bot.get("state"),
        "screenshot_path": screenshot_path,
    }
    return {key: value for key, value in details.items() if value is not None}


def disconnect_reason(snapshot: dict[str, Any] | None) -> str | None:
    details = disconnect_details(snapshot)
    if details is None:
        return None
    label = details.get("reason") or details.get("connection_state") or "unknown"
    label_text = str(label)
    if label_text.lower() in {"code_007", "code 007", "007"} and details.get("code"):
        label = details["code"]
    if label and not str(label).startswith("code_") and str(label).isdigit():
        label = f"code_{label}"
    return f"gw_disconnected:{label}"
