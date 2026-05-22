"""Plan schema and atomic state container for two-model bridge mode."""

from __future__ import annotations

import asyncio
import time
from dataclasses import asdict, dataclass, field
from typing import Any


class PlanValidationError(ValueError):
    """Raised when planner output cannot be accepted as a Plan."""


PHASE_EXPIRY_SECONDS = {
    "long_walk": 60.0,
    "route": 60.0,
    "travel": 60.0,
    "boss": 10.0,
    "dialog": 10.0,
    "merchant": 10.0,
    "combat": 10.0,
}

PLAN_ARGUMENT_SCHEMA = {
    "properties": {
        "version": {
            "type": "integer",
            "enum": [1],
            "description": "Plan schema version. Must be 1.",
        },
        "issued_at": {
            "type": "number",
            "description": "Optional Unix timestamp when the plan was issued.",
        },
        "expires_at": {
            "type": "number",
            "description": "Optional Unix timestamp when the plan expires.",
        },
        "phase": {
            "type": "string",
            "description": "Short machine-readable phase label.",
        },
        "phase_kind": {
            "type": "string",
            "enum": sorted(PHASE_EXPIRY_SECONDS.keys()),
            "description": "Planner phase category.",
        },
        "intent": {
            "type": "string",
            "description": "One concise sentence describing the strategic intent.",
        },
        "next_step": {
            "type": "string",
            "description": "One concrete executor instruction.",
        },
        "deviation": {
            "type": "string",
            "description": "Optional route or state deviation explanation.",
        },
        "constraints": {
            "type": "array",
            "items": {"type": "string"},
            "description": "Constraints the executor must obey.",
        },
        "expected_route": {
            "type": "array",
            "items": {"type": "object"},
            "description": "Expected route/checkpoint objects when route context matters.",
        },
        "abort_conditions": {
            "type": "array",
            "items": {"type": "object"},
            "description": "Conditions that should force replanning or fallback.",
        },
        "fallback": {
            "type": "string",
            "description": "Safe fallback action or policy.",
        },
        "trace_id": {
            "type": "string",
            "description": "Unique-ish ID for correlating planner decisions.",
        },
    },
    "required": [
        "version",
        "phase",
        "phase_kind",
        "intent",
        "next_step",
        "constraints",
        "expected_route",
        "abort_conditions",
        "fallback",
        "trace_id",
    ],
    "additionalProperties": False,
}


@dataclass(frozen=True)
class Plan:
    version: int = 1
    issued_at: float = field(default_factory=time.time)
    expires_at: float = 0.0
    phase: str = "idle"
    phase_kind: str = "route"
    intent: str = "Observe and wait for a planner decision."
    next_step: str = "wait"
    deviation: str | None = None
    constraints: list[str] = field(default_factory=list)
    expected_route: list[dict[str, Any]] = field(default_factory=list)
    abort_conditions: list[dict[str, Any]] = field(default_factory=list)
    fallback: str = "handoff_to_froggy"
    trace_id: str = "plan-initial"

    def __post_init__(self) -> None:
        _require_number("issued_at", self.issued_at)
        if self.expires_at <= 0.0:
            ttl = PHASE_EXPIRY_SECONDS.get(self.phase_kind, 30.0)
            object.__setattr__(self, "expires_at", self.issued_at + ttl)
        self._validate()

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Plan":
        if not isinstance(data, dict):
            raise PlanValidationError("plan must be a JSON object")
        allowed = set(cls.__dataclass_fields__.keys())
        extra = sorted(set(data.keys()) - allowed)
        if extra:
            raise PlanValidationError(f"unexpected plan fields: {', '.join(extra)}")
        required = set(PLAN_ARGUMENT_SCHEMA["required"])
        missing = sorted(required - set(data.keys()))
        if missing:
            raise PlanValidationError(f"missing required plan fields: {', '.join(missing)}")
        values = {key: data[key] for key in allowed if key in data}
        return cls(**values)

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @property
    def expired(self) -> bool:
        return time.time() >= self.expires_at

    def _validate(self) -> None:
        if isinstance(self.version, bool) or not isinstance(self.version, int):
            raise PlanValidationError("version must be integer 1")
        if self.version != 1:
            raise PlanValidationError(f"unsupported plan version {self.version}")

        _require_number("issued_at", self.issued_at)
        _require_number("expires_at", self.expires_at)
        _require_string("phase", self.phase)
        _require_string("phase_kind", self.phase_kind)
        _require_string("intent", self.intent)
        _require_string("next_step", self.next_step)
        _require_optional_string("deviation", self.deviation)
        _require_string("fallback", self.fallback)
        _require_string("trace_id", self.trace_id)

        if self.phase_kind not in PHASE_EXPIRY_SECONDS:
            raise PlanValidationError(f"unsupported phase_kind {self.phase_kind}")
        _require_list_of("constraints", self.constraints, str)
        _require_list_of("expected_route", self.expected_route, dict)
        _require_list_of("abort_conditions", self.abort_conditions, dict)


def _require_number(name: str, value: Any) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise PlanValidationError(f"{name} must be a number")


def _require_string(name: str, value: Any) -> None:
    if not isinstance(value, str):
        raise PlanValidationError(f"{name} must be a string")
    if not value.strip():
        raise PlanValidationError(f"{name} is required")


def _require_optional_string(name: str, value: Any) -> None:
    if value is not None and not isinstance(value, str):
        raise PlanValidationError(f"{name} must be a string")


def _require_list_of(name: str, value: Any, item_type: type) -> None:
    if not isinstance(value, list):
        raise PlanValidationError(f"{name} must be a list")
    for index, item in enumerate(value):
        if not isinstance(item, item_type):
            expected = item_type.__name__
            raise PlanValidationError(f"{name}[{index}] must be a {expected}")


class PlanState:
    """Async-safe atomic Plan holder."""

    def __init__(self, initial: Plan | None = None):
        self._plan = initial or Plan()
        self._lock = asyncio.Lock()

    async def get(self) -> Plan:
        async with self._lock:
            return self._plan

    async def replace(self, plan: Plan) -> Plan:
        async with self._lock:
            self._plan = plan
            return self._plan

    async def snapshot(self) -> dict[str, Any]:
        return (await self.get()).to_dict()
