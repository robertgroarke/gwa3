"""Local-only HTTP/SSE listener for bridge UI clients."""

from __future__ import annotations

import asyncio
import json
import os
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Awaitable, Callable
from urllib.parse import urlparse

from .event_bus import BridgeEventBus
from .flight_recorder import FlightRecorder, RECORDED_EVENTS, compact_payload
from .plan import PlanState

DEFAULT_LANE = "default"
SNAPSHOT_RECOVERABLE_DEGRADATION_PREFIXES = (
    "awaiting_first_snapshot",
)
PLAN_RECOVERABLE_DEGRADATION_PREFIXES = (
    "planner_invalid_json:",
    "planner_slow",
)


@dataclass
class BridgeHttpState:
    event_bus: BridgeEventBus = field(default_factory=BridgeEventBus)
    plan_state: PlanState = field(default_factory=PlanState)
    lane: str = DEFAULT_LANE
    status: str = "stopped"
    last_status_error: str | None = None
    agent_loop: Any | None = None
    last_tool_calls: deque[dict[str, Any]] = field(default_factory=lambda: deque(maxlen=10))
    run_summaries: deque[dict[str, Any]] = field(default_factory=lambda: deque(maxlen=20))
    telemetry: dict[str, Any] = field(default_factory=dict)
    telemetry_timeline: deque[dict[str, Any]] = field(default_factory=lambda: deque(maxlen=50))
    profile: dict[str, Any] = field(default_factory=dict)
    last_disconnect: dict[str, Any] | None = None
    last_degradation: dict[str, Any] | None = None
    last_snapshot_summary: dict[str, Any] | None = None
    launcher: Callable[[], Awaitable[dict[str, Any]]] | None = None
    stopper: Callable[[], Awaitable[dict[str, Any]]] | None = None
    flight_recorder: FlightRecorder | None = None

    def __post_init__(self) -> None:
        if self.flight_recorder is not None:
            return
        directory = os.environ.get("GWA3_FLIGHT_RECORDER_DIR", "").strip()
        if directory:
            self.flight_recorder = FlightRecorder(Path(directory), lane=self.lane)

    async def emit(self, event: str, data: dict[str, Any] | None = None) -> None:
        payload = dict(data or {})
        if event == "bridge.status" and self.profile:
            payload.setdefault("profile", self.profile)
        if event == "bridge.status":
            self.status = str(payload.get("status", self.status))
            if payload.get("error"):
                self.last_status_error = str(payload.get("error"))
            elif self.status in {"connecting", "connected"}:
                self.last_status_error = None
            if self.status == "stopped" and not payload.get("error"):
                self.last_status_error = None
                self.last_degradation = None
        elif event == "tool.call":
            self.last_tool_calls.appendleft(payload)
        elif event == "tool.result":
            self.last_tool_calls.appendleft(payload)
        elif event == "run.summary":
            self.run_summaries.appendleft(payload)
        elif event == "snapshot.summary":
            self.last_snapshot_summary = payload
            await self._clear_recovered_degradation(SNAPSHOT_RECOVERABLE_DEGRADATION_PREFIXES)
        elif event == "llm.telemetry":
            self.telemetry = payload
        elif event == "disconnect_detected":
            self.last_disconnect = payload
            if self.status not in {"stopped", "connecting"}:
                self.status = "degraded"
        elif event == "plan.updated":
            await self._clear_recovered_degradation(PLAN_RECOVERABLE_DEGRADATION_PREFIXES)
        elif event == "degradation":
            self.last_degradation = payload
            if self.status not in {"stopped", "connecting"}:
                self.status = "degraded"
        self._record_telemetry_event(event, payload)
        await self.event_bus.emit(event, payload)

    def _record_telemetry_event(self, event: str, payload: dict[str, Any]) -> None:
        if event not in RECORDED_EVENTS:
            return
        record = {"event": event, "payload": compact_payload(payload)}
        self.telemetry_timeline.appendleft(record)
        if self.flight_recorder is None:
            return
        try:
            self.flight_recorder.record(event, payload)
        except Exception:
            return

    async def _clear_recovered_degradation(self, prefixes: tuple[str, ...]) -> None:
        if self.status != "degraded" or not self.last_degradation:
            return
        reason = str(self.last_degradation.get("reason", ""))
        if not reason.startswith(prefixes):
            return
        self.last_degradation = None
        self.status = "connected"
        await self.event_bus.emit("bridge.status", {
            "status": self.status,
            "lane": self.lane,
            "observers": self.event_bus.subscriber_count,
            "profile": self.profile,
        })

    async def snapshot(self) -> dict[str, Any]:
        return {
            "bridge": {
                "status": self.status,
                "lane": self.lane,
                "observers": self.event_bus.subscriber_count,
                "error": self.last_status_error,
            },
            "plan": await self.plan_state.snapshot(),
            "snapshot": self.last_snapshot_summary,
            "last_tool_calls": list(self.last_tool_calls)[:10],
            "run_summaries": list(self.run_summaries)[:20],
            "telemetry": self.telemetry,
            "profile": self.profile,
            "telemetry_timeline": list(self.telemetry_timeline)[:50],
            "flight_recorder": self.flight_recorder.snapshot() if self.flight_recorder else None,
            "last_disconnect": self.last_disconnect,
            "degradation": self.last_degradation,
        }


class BridgeHttpServer:
    """Minimal asyncio HTTP/1.1 server; avoids adding bridge dependencies."""

    def __init__(self, state: BridgeHttpState, host: str = "127.0.0.1", port: int = 8765):
        self.state = state
        self.host = host
        self.port = port
        self._server: asyncio.AbstractServer | None = None

    async def start(self) -> None:
        self._server = await asyncio.start_server(self._handle, self.host, self.port)
        sockets = self._server.sockets or []
        if sockets:
            self.port = int(sockets[0].getsockname()[1])
        await self.state.emit("bridge.status", {
            "status": self.state.status,
            "lane": self.state.lane,
        })

    async def stop(self) -> None:
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None

    async def _handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            request_line = await reader.readline()
            if not request_line:
                return
            method, raw_path, _ = request_line.decode("iso-8859-1").strip().split(" ", 2)
            headers: dict[str, str] = {}
            while True:
                line = await reader.readline()
                if line in (b"\r\n", b"\n", b""):
                    break
                key, value = line.decode("iso-8859-1").split(":", 1)
                headers[key.strip().lower()] = value.strip()
            length = int(headers.get("content-length", "0") or "0")
            body = await reader.readexactly(length) if length else b""
            payload = json.loads(body.decode("utf-8")) if body else {}
            parsed = urlparse(raw_path)
            await self._route(method.upper(), parsed.path, payload, writer)
        except Exception as exc:
            await self._json(writer, 500, {"error": "server_error", "message": str(exc)})
        finally:
            if not writer.is_closing():
                writer.close()
                try:
                    await writer.wait_closed()
                except ConnectionError:
                    pass

    async def _route(self, method: str, path: str, payload: dict, writer: asyncio.StreamWriter) -> None:
        if method == "OPTIONS":
            await self._empty(writer, 204)
            return
        if method == "GET" and path == "/api/llm/state":
            await self._json(writer, 200, await self.state.snapshot())
            return
        if method == "GET" and path == "/api/llm/stream":
            await self._stream(writer)
            return
        if method == "POST" and path == "/api/llm/chat":
            message = str(payload.get("message", "")).strip()
            if not message:
                await self._json(writer, 400, {"error": "missing_message"})
                return
            if self.state.agent_loop is not None:
                await self.state.agent_loop.inject_user_message(message)
            else:
                await self.state.emit("chat.user", {"message": message})
            await self._json(writer, 200, {"ok": True})
            return
        if method == "POST" and path == "/api/llm/launch":
            if not self._lane_allowed(payload):
                await self._json(writer, 400, self._lane_error())
                return
            if self.state.status in {"connecting", "connected"}:
                await self._json(writer, 200, {"ok": True, "status": self.state.status, "noop": True})
                return
            await self.state.emit("bridge.status", {
                "status": "connecting",
                "lane": self.state.lane,
            })
            result = await self.state.launcher() if self.state.launcher else {"ok": True, "mode": "bridge_already_loaded"}
            await self._json(writer, 200, result)
            return
        if method == "POST" and path == "/api/llm/stop":
            await self.state.emit("bridge.status", {
                "status": "stopped",
                "lane": self.state.lane,
            })
            if self.state.agent_loop is not None:
                self.state.agent_loop.stop()
            result = await self.state.stopper() if self.state.stopper else {"ok": True}
            await self._json(writer, 200, result)
            return
        await self._json(writer, 404, {"error": "not_found"})

    def _lane_allowed(self, payload: dict) -> bool:
        lane = str(payload.get("lane", self.state.lane)).lower()
        return lane == self.state.lane.lower()

    def _lane_error(self) -> dict[str, str]:
        return {
            "error": "lane_not_permitted",
            "message": f"{self.state.lane.upper()} lane only",
        }

    async def _stream(self, writer: asyncio.StreamWriter) -> None:
        writer.write(
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Type: text/event-stream\r\n"
            b"Cache-Control: no-cache\r\n"
            b"Connection: keep-alive\r\n"
            b"Access-Control-Allow-Origin: *\r\n\r\n"
        )
        await writer.drain()
        queue = await self.state.event_bus.subscribe(replay=True)
        await self.state.emit("bridge.status", {
            "status": self.state.status,
            "lane": self.state.lane,
            "observers": self.state.event_bus.subscriber_count,
        })
        try:
            while True:
                item = await queue.get()
                data = json.dumps(item.data, separators=(",", ":"))
                writer.write(f"id: {item.sequence}\nevent: {item.event}\ndata: {data}\n\n".encode("utf-8"))
                await writer.drain()
        finally:
            await self.state.event_bus.unsubscribe(queue)
            await self.state.emit("bridge.status", {
                "status": self.state.status,
                "lane": self.state.lane,
                "observers": self.state.event_bus.subscriber_count,
            })

    async def _json(self, writer: asyncio.StreamWriter, status: int, data: dict) -> None:
        body = json.dumps(data).encode("utf-8")
        writer.write(
            f"HTTP/1.1 {status} {self._reason(status)}\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Headers: content-type\r\n"
            f"Content-Length: {len(body)}\r\n\r\n".encode("utf-8") + body
        )
        await writer.drain()

    async def _empty(self, writer: asyncio.StreamWriter, status: int) -> None:
        writer.write(
            f"HTTP/1.1 {status} {self._reason(status)}\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Headers: content-type\r\n"
            "Content-Length: 0\r\n\r\n".encode("utf-8")
        )
        await writer.drain()

    @staticmethod
    def _reason(status: int) -> str:
        return {
            200: "OK",
            204: "No Content",
            400: "Bad Request",
            404: "Not Found",
            500: "Internal Server Error",
        }.get(status, "OK")
