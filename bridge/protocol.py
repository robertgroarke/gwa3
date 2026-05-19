"""Shared bridge protocol contract constants and validators."""

from __future__ import annotations

from typing import Any


IPC_PROTOCOL_VERSION = 1
TOOL_SCHEMA_VERSION = "gwa3-tools-v1"
DEFAULT_LANE = "default"


class ProtocolMismatchError(RuntimeError):
    """Raised when a bridge peer speaks an unsupported IPC protocol."""


def with_protocol_version(message: dict[str, Any]) -> dict[str, Any]:
    """Return a copy of *message* stamped with the current IPC protocol."""
    stamped = dict(message)
    stamped["protocol_version"] = IPC_PROTOCOL_VERSION
    stamped["v"] = IPC_PROTOCOL_VERSION
    return stamped


def read_protocol_version(message: dict[str, Any]) -> int | None:
    """Read the wire protocol version from either supported field."""
    version = message.get("protocol_version", message.get("v"))
    return version if isinstance(version, int) else None


def validate_protocol_version(message: dict[str, Any], context: str = "message") -> None:
    """Fail fast when an IPC message uses a different protocol version."""
    version = read_protocol_version(message)
    if version != IPC_PROTOCOL_VERSION:
        raise ProtocolMismatchError(
            f"{context} protocol_version={version!r}; expected {IPC_PROTOCOL_VERSION}"
        )


def lane_name_from_pipe(pipe_name: str) -> str:
    """Infer a non-sensitive lane identifier from a gwa3 named pipe."""
    name = pipe_name.rsplit("\\", 1)[-1].lower()
    prefix = "gwa3_llm"
    if name == prefix:
        return DEFAULT_LANE
    if name.startswith(prefix + "_") or name.startswith(prefix + "-"):
        lane = name[len(prefix) + 1 :]
    else:
        return DEFAULT_LANE
    lane = "".join(ch if ch.isalnum() or ch in "_-" else "_" for ch in lane)
    return lane or DEFAULT_LANE
