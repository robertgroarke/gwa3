"""Named pipe IPC client for communicating with gwa3.dll."""

import asyncio
import json
import struct
import sys

# Windows named pipe support
if sys.platform == "win32":
    import win32file
    import win32pipe
    import pywintypes


class IpcClient:
    """Async-friendly named pipe client for the gwa3 LLM bridge."""

    def __init__(self, pipe_name: str = r"\\.\pipe\gwa3_llm"):
        self.pipe_name = pipe_name
        self._handle = None
        self._connected = False
        self._read_lock = asyncio.Lock()
        self._write_lock = asyncio.Lock()

    async def connect(self, timeout: float = 30.0) -> bool:
        """Connect to the gwa3 named pipe. Retries until timeout."""
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
                self._connected = True
                return True
            except pywintypes.error:
                await asyncio.sleep(0.5)
        return False

    def disconnect(self):
        """Close the pipe connection."""
        if self._handle is not None:
            try:
                win32file.CloseHandle(self._handle)
            except Exception:
                pass
            self._handle = None
        self._connected = False

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

    async def read_message(self) -> dict | None:
        """Read one length-prefixed JSON message from the pipe.

        Returns the parsed JSON dict, or None on error/disconnect.
        """
        async with self._read_lock:
            try:
                # Run blocking read in a thread to avoid blocking the event loop
                loop = asyncio.get_event_loop()
                header = await loop.run_in_executor(None, self._read_bytes, 4)
                (length,) = struct.unpack("<I", header)
                if length == 0 or length > 1024 * 1024:
                    return None
                payload = await loop.run_in_executor(None, self._read_bytes, length)
                return json.loads(payload.decode("utf-8"))
            except Exception:
                self._connected = False
                return None

    async def send_message(self, msg: dict):
        """Send a length-prefixed JSON message to the pipe."""
        async with self._write_lock:
            payload = json.dumps(msg).encode("utf-8")
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
