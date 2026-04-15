"""BridgeTestCase base class — connects to gwa3 pipe and provides test helpers."""

from __future__ import annotations

import asyncio
import json
import os
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
        pipe_name = os.environ.get("GWA3_PIPE_NAME", self.PIPE_NAME)
        self.ipc = IpcClient(pipe_name)
        self._message_buffer: list[dict] = []
        self._action_results: dict[str, dict] = {}
        self._latest_snapshots: dict[int, dict] = {}

    async def setUp(self):
        """Connect to pipe and preserve the first live snapshot."""
        if not await self.ipc.connect(timeout=30.0):
            raise TestFailure("Could not connect to gwa3 pipe ? is gwa3.dll injected with --llm?")
        self._message_buffer.clear()
        self._action_results.clear()
        self._latest_snapshots.clear()
        try:
            first = await self.wait_for_snapshot(timeout=10.0)
            self._message_buffer.insert(0, first)
        except Exception as exc:
            raise TestFailure(f"Could not receive initial snapshot after pipe connect: {exc}") from exc

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
            except asyncio.TimeoutError:
                break

    def _route_message(self, msg: dict):
        """Sort a message into the appropriate buffer."""
        msg_type = msg.get("type", "")
        if msg_type == "action_result":
            req_id = msg.get("request_id", "")
            if req_id:
                self._action_results[req_id] = msg
        elif msg_type in ("snapshot", "event", "heartbeat"):
            if msg_type == "snapshot":
                tier = int(msg.get("tier", 0) or 0)
                if tier > 0:
                    self._latest_snapshots[tier] = msg
            self._message_buffer.append(msg)

    def latest_snapshot(self, tier: int | None = None) -> dict | None:
        if tier is None:
            if not self._latest_snapshots:
                return None
            latest_tier = sorted(self._latest_snapshots.keys())[-1]
            return self._latest_snapshots.get(latest_tier)
        return self._latest_snapshots.get(tier)

    def _pop_buffered_snapshot(self, tier: int | None = None) -> dict | None:
        for idx, msg in enumerate(self._message_buffer):
            if msg.get("type") != "snapshot":
                continue
            if tier is not None and msg.get("tier") != tier:
                continue
            return self._message_buffer.pop(idx)
        return None

    async def wait_for_snapshot(self, tier: int | None = None, timeout: float = 5.0) -> dict:
        """Read pipe messages until a snapshot of the requested tier arrives.

        If tier is None, returns the first snapshot of any tier.
        Heartbeats and action_results are buffered, not discarded.
        """
        buffered = self._pop_buffered_snapshot(tier)
        if buffered is not None:
            return buffered
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
                buffered = self._pop_buffered_snapshot(tier)
                if buffered is not None:
                    return buffered
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
        self,
        name: str,
        params: dict | None = None,
        timeout: float = 3.0,
        await_result: bool = True,
    ) -> dict:
        """Send an action and wait for the corresponding action_result.

        Returns the action_result dict: {"success": bool, "error": str|null, ...}
        """
        request_id = str(uuid.uuid4())[:8] if await_result else ""
        await self.ipc.send_action(name, params or {}, request_id)
        if not await_result:
            return {
                "type": "action_result",
                "request_id": request_id,
                "success": True,
                "error": None,
                "fire_and_forget": True,
            }

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

    async def send_action_no_wait(self, name: str, params: dict | None = None) -> dict:
        """Send an action without waiting for an action_result."""
        return await self.send_action(name, params=params, await_result=False)

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
