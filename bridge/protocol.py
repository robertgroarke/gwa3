"""Shared bridge protocol contract constants and validators."""

from __future__ import annotations

from typing import Any


IPC_PROTOCOL_VERSION = 1
TOOL_SCHEMA_VERSION = "gwa3-tools-v1"


class ProtocolMismatchError(RuntimeError):
    """Raised when a bridge peer speaks an unsupported IPC protocol."""


def with_protocol_version(message: dict[str, Any]) -> dict[str, Any]:
    """Return a copy of *message* stamped with the current IPC protocol."""
    stamped = dict(message)
    stamped["protocol_version"] = IPC_PROTOCOL_VERSION
    return stamped


def validate_protocol_version(message: dict[str, Any], context: str = "message") -> None:
    """Fail fast when an IPC message uses a different protocol version."""
    version = message.get("protocol_version")
    if version != IPC_PROTOCOL_VERSION:
        raise ProtocolMismatchError(
            f"{context} protocol_version={version!r}; expected {IPC_PROTOCOL_VERSION}"
        )
