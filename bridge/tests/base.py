"""BridgeTestCase base class — connects to gwa3 pipe and provides test helpers."""

from __future__ import annotations

import asyncio
import json
import uuid
import time
from typing import Callable

from ..ipc_client import IpcClient
from .helpers import TestFailure, assert_true


class TestSkipped(Exception):
    """Raised to skip a test with a reason."""


class BridgeTestCase:
    """Base for all bridge integration tests.

    Provides pipe connection, snapshot reading, action sending, and assertions.
    """

    PIPE_NAME = r"\\.\pipe\gwa3_llm"

    def __init__(self):
        self.ipc = IpcClient(self.PIPE_NAME)
        self._message_buffer: list[dict] = []
        self._action_results: dict[str, dict] = {}

    async def setUp(self):
        """Connect to pipe, drain stale messages, wait for first snapshot."""
        if not await self.ipc.connect(timeout=30.0):
            raise TestFailure("Could not connect to gwa3 pipe — is gwa3.dll injected with --llm?")
        # Drain any stale messages
        await self._drain(timeout=1.0)
        self._message_buffer.clear()
        self._action_results.clear()

    async def tearDown(self):
        """Disconnect from pipe."""
        self.ipc.disconnect()

    async def _drain(self, timeout: float = 0.5):
        """Read all pending pipe messages, buffering them by type."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=0.1)
                if msg is None:
                    break
                self._route_message(msg)
            except (asyncio.TimeoutError, Exception):
                break

    def _route_message(self, msg: dict):
        """Sort a message into the appropriate buffer."""
        msg_type = msg.get("type", "")
        if msg_type == "action_result":
            req_id = msg.get("request_id", "")
            if req_id:
                self._action_results[req_id] = msg
        elif msg_type in ("snapshot", "event", "heartbeat"):
            self._message_buffer.append(msg)

    async def wait_for_snapshot(self, tier: int | None = None, timeout: float = 5.0) -> dict:
        """Read pipe messages until a snapshot of the requested tier arrives.

        If tier is None, returns the first snapshot of any tier.
        Heartbeats and action_results are buffered, not discarded.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                remaining = max(0.05, deadline - time.monotonic())
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=remaining)
                if msg is None:
                    continue
                self._route_message(msg)
                if msg.get("type") == "snapshot":
                    if tier is None or msg.get("tier") == tier:
                        return msg
            except asyncio.TimeoutError:
                continue
        raise TestFailure(f"Timed out waiting for snapshot (tier={tier}, timeout={timeout}s)")

    async def wait_for_message_type(self, msg_type: str, timeout: float = 10.0) -> dict:
        """Read pipe messages until a message of the specified type arrives."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                remaining = max(0.05, deadline - time.monotonic())
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=remaining)
                if msg is None:
                    continue
                self._route_message(msg)
                if msg.get("type") == msg_type:
                    return msg
            except asyncio.TimeoutError:
                continue
        raise TestFailure(f"Timed out waiting for message type '{msg_type}' (timeout={timeout}s)")

    async def send_action(
        self, name: str, params: dict | None = None, timeout: float = 3.0
    ) -> dict:
        """Send an action and wait for the corresponding action_result.

        Returns the action_result dict: {"success": bool, "error": str|null, ...}
        """
        request_id = str(uuid.uuid4())[:8]
        await self.ipc.send_action(name, params or {}, request_id)

        # Read messages until we get the matching action_result
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            # Check if result already arrived in buffer
            if request_id in self._action_results:
                return self._action_results.pop(request_id)
            try:
                remaining = max(0.05, deadline - time.monotonic())
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=remaining)
                if msg is None:
                    continue
                if msg.get("type") == "action_result" and msg.get("request_id") == request_id:
                    return msg
                self._route_message(msg)
            except asyncio.TimeoutError:
                continue

        raise TestFailure(f"Timed out waiting for action_result (action={name}, id={request_id})")

    async def wait_for_state_change(
        self,
        predicate: Callable[[dict], bool],
        tier: int | None = None,
        timeout: float = 10.0,
    ) -> dict:
        """Read snapshots until predicate(snapshot) returns True."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                snap = await self.wait_for_snapshot(
                    tier=tier, timeout=max(0.5, deadline - time.monotonic())
                )
                if predicate(snap):
                    return snap
            except TestFailure:
                continue
        raise TestFailure(f"State change not observed within {timeout}s")

    # --- Assertions ---

    def assert_action_success(self, result: dict):
        assert_true(
            result.get("success") is True,
            f"Expected action success, got error: {result.get('error')}",
        )

    def assert_action_error(self, result: dict, expected_error: str):
        assert_true(
            result.get("success") is False,
            f"Expected action failure with '{expected_error}', but got success",
        )
        actual = result.get("error", "")
        assert_true(
            actual == expected_error,
            f"Expected error '{expected_error}', got '{actual}'",
        )

    def skip(self, reason: str):
        raise TestSkipped(reason)
