"""Named pipe IPC client for communicating with gwa3.dll.

Architecture: one long-running reader task drains the pipe and pushes
parsed messages onto an asyncio.Queue. Consumers call ``read_message``
and can safely wrap it in ``asyncio.wait_for(...)`` for timeouts —
cancelling the wait cancels only the ``queue.get()``, NOT the reader.

Why this matters: win32file's blocking ``ReadFile`` call runs in a
thread-pool executor. If you ``wait_for`` a ``run_in_executor`` future
and it times out, asyncio cancels the future but the executor thread
stays blocked inside the Windows API. When data eventually arrives, it
is consumed from the pipe and then dropped, because the cancelled
future can no longer deliver it. The next ``ReadFile`` reads the REST
of the message (or the middle of the NEXT message), silently corrupting
the length-prefix framing. We saw this surface as an L02-after-L03
test flake where every subsequent read returned a bogus length.
"""

import asyncio
import json
import logging
import struct
import sys

from .protocol import (
    ProtocolMismatchError,
    lane_name_from_pipe,
    validate_protocol_version,
    with_protocol_version,
)

# Windows named pipe support
if sys.platform == "win32":
    import win32file
    import win32pipe
    import pywintypes


log = logging.getLogger(__name__)


class IpcClient:
    """Async-friendly named pipe client for the gwa3 LLM bridge."""

    def __init__(self, pipe_name: str = r"\\.\pipe\gwa3_llm", lane_name: str | None = None):
        self.pipe_name = pipe_name
        self.lane_name = lane_name or lane_name_from_pipe(pipe_name)
        self._handle = None
        self._connected = False
        self._write_lock = asyncio.Lock()
        self._queue: asyncio.Queue | None = None
        self._reader_task: asyncio.Task | None = None
        self._pending_action_results: dict[str, asyncio.Future] = {}
        self._action_result_backlog: dict[str, dict] = {}

    def _client_hello_message(self) -> dict:
        return {
            "type": "hello",
            "role": "bridge",
            "lane": self.lane_name,
        }

    async def connect(self, timeout: float = 30.0) -> bool:
        """Connect to the gwa3 named pipe. Retries until timeout.

        On successful connect, spawns a dedicated reader task that drains
        the pipe into ``self._queue``. Consumers read via ``read_message``.
        """
        deadline = asyncio.get_event_loop().time() + timeout
        while asyncio.get_event_loop().time() < deadline:
            try:
                self._handle = win32file.CreateFile(
                    self.pipe_name,
                    win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                    0,  # no sharing
                    None,  # default security
                    win32file.OPEN_EXISTING,
                    0,  # default attributes
                    None,  # no template
                )
                win32pipe.SetNamedPipeHandleState(
                    self._handle, win32pipe.PIPE_READMODE_BYTE, None, None
                )
                loop = asyncio.get_event_loop()
                server_hello = await loop.run_in_executor(
                    None, self._read_message_blocking
                )
                validate_protocol_version(server_hello, "server hello")
                if server_hello.get("type") != "hello":
                    raise ProtocolMismatchError(
                        f"server hello type={server_hello.get('type')!r}; expected 'hello'"
                    )
                await loop.run_in_executor(
                    None,
                    self._write_message_blocking,
                    self._client_hello_message(),
                )
                self._connected = True
                self._queue = asyncio.Queue()
                self._reader_task = asyncio.create_task(self._reader_loop())
                return True
            except ProtocolMismatchError as e:
                log.error("IpcClient: protocol handshake failed: %s", e)
                self.disconnect()
                return False
            except (IOError, OSError, UnicodeDecodeError, json.JSONDecodeError) as e:
                log.error("IpcClient: protocol handshake failed: %s", e)
                self.disconnect()
                return False
            except pywintypes.error:
                await asyncio.sleep(0.5)
        return False

    def disconnect(self):
        """Close the pipe connection and stop the reader task."""
        # Closing the handle first causes a blocked ReadFile in the reader's
        # executor thread to fail cleanly with a pywintypes.error, which the
        # reader catches and exits.
        if self._handle is not None:
            try:
                win32file.CloseHandle(self._handle)
            except Exception:
                pass
            self._handle = None
        self._connected = False
        if self._reader_task is not None and not self._reader_task.done():
            self._reader_task.cancel()
        self._reader_task = None
        for future in self._pending_action_results.values():
            if not future.done():
                future.cancel()
        self._pending_action_results.clear()

    @property
    def connected(self) -> bool:
        return self._connected

    def _read_bytes(self, count: int) -> bytes:
        """Read exactly `count` bytes from the pipe (blocking)."""
        chunks = []
        remaining = count
        while remaining > 0:
            hr, data = win32file.ReadFile(self._handle, remaining)
            if hr != 0:
                raise IOError(f"ReadFile failed with hr={hr}")
            if not data:
                raise IOError("Pipe closed")
            chunks.append(data)
            remaining -= len(data)
        return b"".join(chunks)

    def _write_bytes(self, data: bytes):
        """Write bytes to the pipe (blocking)."""
        win32file.WriteFile(self._handle, data)

    def _read_message_blocking(self) -> dict:
        header = self._read_bytes(4)
        (length,) = struct.unpack("<I", header)
        if length == 0 or length > 1024 * 1024:
            raise IOError(f"Bogus length prefix 0x{length:x}")
        payload = self._read_bytes(length)
        return json.loads(payload.decode("utf-8"))

    def _write_message_blocking(self, msg: dict):
        stamped = with_protocol_version(msg)
        payload = json.dumps(stamped).encode("utf-8")
        header = struct.pack("<I", len(payload))
        self._write_bytes(header + payload)

    async def _reader_loop(self):
        """Drain the pipe and push messages onto ``self._queue``.

        Exits on pipe close / disconnect. Pushes a ``None`` sentinel onto
        the queue on exit so any pending consumer sees the disconnect.
        Malformed frames (bad length, invalid UTF-8, invalid JSON) are
        logged and skipped — we do NOT tear down the reader on parse
        errors, since a single bad frame shouldn't kill the whole stream.
        """
        loop = asyncio.get_event_loop()
        try:
            while True:
                try:
                    header = await loop.run_in_executor(
                        None, self._read_bytes, 4
                    )
                except (IOError, OSError, pywintypes.error):
                    break
                (length,) = struct.unpack("<I", header)
                if length == 0 or length > 1024 * 1024:
                    log.warning(
                        "IpcClient: bogus length prefix 0x%x; "
                        "ending session to avoid framing drift", length
                    )
                    break
                try:
                    payload = await loop.run_in_executor(
                        None, self._read_bytes, length
                    )
                except (IOError, OSError, pywintypes.error):
                    break
                try:
                    msg = json.loads(payload.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError) as e:
                    log.warning(
                        "IpcClient: dropped malformed message (%s); "
                        "continuing", e.__class__.__name__,
                    )
                    continue
                try:
                    validate_protocol_version(msg, "ipc message")
                except ProtocolMismatchError as e:
                    log.error("IpcClient: %s", e)
                    break
                self._deliver_action_result(msg)
                await self._queue.put(msg)
        finally:
            self._connected = False
            # Sentinel so any waiter gets unblocked with None.
            if self._queue is not None:
                try:
                    self._queue.put_nowait(None)
                except asyncio.QueueFull:
                    pass

    async def read_message(self) -> dict | None:
        """Return the next message from the pipe, or None on disconnect.

        Safe to wrap in ``asyncio.wait_for`` — a cancellation only cancels
        this coroutine's ``queue.get()``, it does NOT disturb the reader
        task or leave stray bytes in the pipe. Subsequent calls will
        continue draining the queue in order.
        """
        if self._queue is None:
            return None
        try:
            msg = await self._queue.get()
        except asyncio.CancelledError:
            raise
        return msg

    async def send_message(self, msg: dict):
        """Send a length-prefixed JSON message to the pipe."""
        async with self._write_lock:
            payload = json.dumps(with_protocol_version(msg)).encode("utf-8")
            header = struct.pack("<I", len(payload))
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, self._write_bytes, header + payload)

    async def send_action(
        self, action_name: str, params: dict | None = None, request_id: str = ""
    ) -> None:
        """Send an action command to gwa3."""
        msg = {
            "type": "action",
            "name": action_name,
            "params": params or {},
            "request_id": request_id,
        }
        await self.send_message(msg)

    def expect_action_result(self, request_id: str) -> asyncio.Future:
        """Register interest in a specific action_result before sending."""
        loop = asyncio.get_event_loop()
        future = loop.create_future()
        if not request_id:
            future.set_result({
                "type": "action_result",
                "request_id": request_id,
                "success": False,
                "error": "missing_request_id",
            })
            return future
        backlog = self._action_result_backlog.pop(request_id, None)
        if backlog is not None:
            future.set_result(backlog)
            return future
        self._pending_action_results[request_id] = future
        return future

    def forget_action_result(self, request_id: str, future=None) -> None:
        current = self._pending_action_results.get(request_id)
        if future is None or current is future:
            self._pending_action_results.pop(request_id, None)
        if current is not None and not current.done():
            current.cancel()

    async def wait_for_action_result(
        self,
        request_id: str,
        timeout: float,
        future=None,
    ) -> dict:
        """Wait for the matching action_result while the reader drains the pipe."""
        waiter = future or self.expect_action_result(request_id)
        try:
            result = await asyncio.wait_for(asyncio.shield(waiter), timeout=timeout)
            return result
        finally:
            current = self._pending_action_results.get(request_id)
            if current is waiter:
                self._pending_action_results.pop(request_id, None)
                if not waiter.done():
                    waiter.cancel()

    def _deliver_action_result(self, msg: dict) -> None:
        if msg.get("type") != "action_result":
            return
        request_id = msg.get("request_id", "")
        if not request_id:
            return
        future = self._pending_action_results.get(request_id)
        if future is not None and not future.done():
            future.set_result(msg)
            return
        self._action_result_backlog[request_id] = msg
        if len(self._action_result_backlog) > 100:
            for stale in list(self._action_result_backlog)[:50]:
                self._action_result_backlog.pop(stale, None)
