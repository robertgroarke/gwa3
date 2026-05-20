"""In-process event bus for the bridge HTTP/SSE surface."""

from __future__ import annotations

import asyncio
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True)
class BridgeEvent:
    """A typed event emitted to HTTP/SSE clients."""

    sequence: int
    event: str
    data: dict[str, Any]
    timestamp: float = field(default_factory=time.time)


class BridgeEventBus:
    """Small fan-out bus with bounded replay for late-joining clients."""

    def __init__(self, replay_size: int = 200):
        self._sequence = 0
        self._replay: deque[BridgeEvent] = deque(maxlen=replay_size)
        self._subscribers: set[asyncio.Queue[BridgeEvent]] = set()
        self._lock = asyncio.Lock()

    async def emit(self, event: str, data: dict[str, Any] | None = None) -> BridgeEvent:
        async with self._lock:
            self._sequence += 1
            item = BridgeEvent(
                sequence=self._sequence,
                event=event,
                data=data or {},
            )
            self._replay.append(item)
            subscribers = list(self._subscribers)

        for queue in subscribers:
            try:
                queue.put_nowait(item)
            except asyncio.QueueFull:
                try:
                    queue.get_nowait()
                except asyncio.QueueEmpty:
                    pass
                queue.put_nowait(item)
        return item

    async def subscribe(self, replay: bool = True) -> asyncio.Queue[BridgeEvent]:
        queue: asyncio.Queue[BridgeEvent] = asyncio.Queue(maxsize=256)
        async with self._lock:
            self._subscribers.add(queue)
            history = list(self._replay) if replay else []
        for item in history:
            queue.put_nowait(item)
        return queue

    async def unsubscribe(self, queue: asyncio.Queue[BridgeEvent]) -> None:
        async with self._lock:
            self._subscribers.discard(queue)

    @property
    def subscriber_count(self) -> int:
        return len(self._subscribers)

    def recent(self, count: int = 50) -> list[BridgeEvent]:
        return list(self._replay)[-count:]
