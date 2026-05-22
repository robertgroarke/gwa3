"""Shared primitives for bridge agent loops."""

from __future__ import annotations

import asyncio
import inspect
import time
from collections.abc import Callable, Awaitable
from typing import Any

from .run_summary_memory import RunSummaryMemory


MessageHandler = Callable[[dict[str, Any]], Awaitable[None] | None]
HeartbeatHandler = Callable[[], Awaitable[None] | None]


class HeartbeatMonitor:
    """Track bridge heartbeat freshness without tying loops to wall-clock globals."""

    def __init__(
        self,
        timeout_seconds: float,
        *,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self.timeout_seconds = timeout_seconds
        self._clock = clock
        self._last_seen = 0.0
        self._saw_heartbeat = False

    def mark(self) -> None:
        self._last_seen = self._clock()
        self._saw_heartbeat = True

    @property
    def saw_heartbeat(self) -> bool:
        return self._saw_heartbeat

    def age_seconds(self) -> float | None:
        if not self._saw_heartbeat or self._last_seen <= 0.0:
            return None
        return round(max(0.0, self._clock() - self._last_seen), 3)

    def timed_out(self) -> bool:
        if not self._saw_heartbeat:
            return False
        return self._clock() - self._last_seen > self.timeout_seconds


class HeartbeatPayloadState:
    """Hold the latest heartbeat payload and optionally mirror it to telemetry."""

    def __init__(self, telemetry: Any | None = None) -> None:
        self.message: dict[str, Any] = {}
        self.telemetry = telemetry

    def record(self, message: dict[str, Any] | None) -> None:
        self.message = message or {}
        if self.telemetry is not None:
            self.telemetry.record_heartbeat(self.message)

    def get(self, key: str, default: Any = None) -> Any:
        return self.message.get(key, default)

    @property
    def outbound_snapshots_dropped(self) -> Any:
        return self.get("outbound_snapshots_dropped")


class UserMessageInbox:
    """Non-blocking user-message inbox shared by single and two-model loops."""

    def __init__(self) -> None:
        self._queue: asyncio.Queue[str] = asyncio.Queue()

    async def put(self, message: str) -> None:
        await self._queue.put(message)

    def empty(self) -> bool:
        return self._queue.empty()

    def get_nowait(self) -> str:
        return self._queue.get_nowait()

    def drain_nowait(self) -> list[str]:
        messages: list[str] = []
        while not self._queue.empty():
            messages.append(self._queue.get_nowait())
        return messages


class ObservationPump:
    """Drain IPC messages and route them to loop-specific handlers."""

    def __init__(
        self,
        ipc: Any,
        *,
        on_snapshot: MessageHandler | None = None,
        on_event: MessageHandler | None = None,
        on_action_result: MessageHandler | None = None,
        on_heartbeat: HeartbeatHandler | None = None,
        on_heartbeat_message: MessageHandler | None = None,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self.ipc = ipc
        self.on_snapshot = on_snapshot
        self.on_event = on_event
        self.on_action_result = on_action_result
        self.on_heartbeat = on_heartbeat
        self.on_heartbeat_message = on_heartbeat_message
        self._clock = clock

    async def drain(
        self,
        *,
        max_messages: int | None = None,
        max_seconds: float | None = None,
        read_timeout: float = 0.05,
    ) -> int:
        started = self._clock()
        read_count = 0
        while True:
            if max_messages is not None and read_count >= max_messages:
                break
            if max_seconds is not None and self._clock() - started >= max_seconds:
                break

            msg = await asyncio.wait_for(self.ipc.read_message(), timeout=read_timeout)
            if msg is None:
                break

            read_count += 1
            await self._dispatch(msg)
        return read_count

    async def _dispatch(self, msg: dict[str, Any]) -> None:
        msg_type = msg.get("type", "")
        if msg_type == "snapshot":
            await self._call(self.on_snapshot, msg)
        elif msg_type == "event":
            await self._call(self.on_event, msg)
        elif msg_type == "action_result":
            await self._call(self.on_action_result, msg)
        elif msg_type == "heartbeat":
            await self._call(self.on_heartbeat)
            await self._call(self.on_heartbeat_message, msg)

    @staticmethod
    async def _call(handler: Callable[..., Awaitable[None] | None] | None, *args: Any) -> None:
        if handler is None:
            return
        result = handler(*args)
        if inspect.isawaitable(result):
            await result


class RunSummarizer:
    """Shared run-summary facade for prompt memory, planner history, and UI cards."""

    def __init__(self, memory: RunSummaryMemory | None = None) -> None:
        self.memory = memory or RunSummaryMemory()

    @property
    def entries(self) -> list[dict[str, Any]]:
        return self.memory.entries

    def list(self) -> list[dict[str, Any]]:
        return self.memory.list()

    def format_for_prompt(self) -> str:
        return self.memory.format_for_prompt()

    def add(self, summary: str, event: dict[str, Any] | None = None) -> dict[str, Any] | None:
        return self.memory.add(summary, event)

    def append(self, summary: dict[str, Any]) -> dict[str, Any]:
        return self.memory.append(summary)
