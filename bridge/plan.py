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
        if self.expires_at <= 0.0:
            ttl = PHASE_EXPIRY_SECONDS.get(self.phase_kind, 30.0)
            object.__setattr__(self, "expires_at", self.issued_at + ttl)
        self._validate()

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Plan":
        if not isinstance(data, dict):
            raise PlanValidationError("plan must be a JSON object")
        allowed = cls.__dataclass_fields__.keys()
        values = {key: data[key] for key in allowed if key in data}
        return cls(**values)

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @property
    def expired(self) -> bool:
        return time.time() >= self.expires_at

    def _validate(self) -> None:
        if int(self.version) != 1:
            raise PlanValidationError(f"unsupported plan version {self.version}")
        if not str(self.phase).strip():
            raise PlanValidationError("phase is required")
        if not str(self.intent).strip():
            raise PlanValidationError("intent is required")
        if not str(self.next_step).strip():
            raise PlanValidationError("next_step is required")
        if not isinstance(self.constraints, list):
            raise PlanValidationError("constraints must be a list")
        if not isinstance(self.expected_route, list):
            raise PlanValidationError("expected_route must be a list")
        if not isinstance(self.abort_conditions, list):
            raise PlanValidationError("abort_conditions must be a list")


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
