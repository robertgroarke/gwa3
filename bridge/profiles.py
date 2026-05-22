"""Composable launch profiles for GWA3 LLM mode."""

from __future__ import annotations

from dataclasses import asdict, dataclass, replace
from typing import Any


DEFAULT_PROFILE_NAME = "qwen-safe"

SUPERVISOR_MODES = {"deterministic", "hybrid", "llm"}
EXECUTOR_MODES = {"health-check", "llm", "disabled"}
PLANNER_MODES = {"sync", "async"}
PROMPT_MODES = {"full", "cached", "delta"}
REPLAN_POLICIES = {"strict", "balanced", "aggressive"}
PROVIDERS = {"openai-compatible", "codex-exec"}


@dataclass(frozen=True)
class BridgeProfile:
    name: str
    label: str
    description: str
    two_model: bool
    llm_provider: str
    model: str
    planner_provider: str
    planner_model: str
    executor_provider: str
    executor_model: str
    planner_timeout: float
    executor_timeout: float
    supervisor_mode: str
    executor_mode: str
    planner_mode: str
    prompt_mode: str
    replan_policy: str
    preflight_executor: bool = True

    def with_overrides(self, overrides: dict[str, Any]) -> "BridgeProfile":
        return replace(self, **{key: value for key, value in overrides.items() if value is not None})

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    def public_state(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "label": self.label,
            "description": self.description,
            "two_model": self.two_model,
            "llm_provider": self.llm_provider,
            "model": self.model,
            "planner_provider": self.planner_provider,
            "planner_model": self.planner_model,
            "executor_provider": self.executor_provider,
            "executor_model": self.executor_model,
            "planner_timeout": self.planner_timeout,
            "executor_timeout": self.executor_timeout,
            "supervisor_mode": self.supervisor_mode,
            "executor_mode": self.executor_mode,
            "planner_mode": self.planner_mode,
            "prompt_mode": self.prompt_mode,
            "replan_policy": self.replan_policy,
        }


PROFILES: dict[str, BridgeProfile] = {
    "qwen-safe": BridgeProfile(
        name="qwen-safe",
        label="Qwen Safe",
        description="Default operator profile. Froggy owns normal route phases; Qwen handles ambiguity, recovery, unexpected state, and chat.",
        two_model=True,
        llm_provider="openai-compatible",
        model="qwen3.5:cloud",
        planner_provider="openai-compatible",
        planner_model="qwen3.5:cloud",
        executor_provider="openai-compatible",
        executor_model="qwen3.5:cloud",
        planner_timeout=30.0,
        executor_timeout=10.0,
        supervisor_mode="deterministic",
        executor_mode="health-check",
        planner_mode="async",
        prompt_mode="delta",
        replan_policy="strict",
        preflight_executor=False,
    ),
    "qwen-deterministic": BridgeProfile(
        name="qwen-deterministic",
        label="Qwen Deterministic",
        description="Minimal LLM usage. Froggy owns route decisions; Qwen is reserved for chat and severe unknown state.",
        two_model=True,
        llm_provider="openai-compatible",
        model="qwen3.5:cloud",
        planner_provider="openai-compatible",
        planner_model="qwen3.5:cloud",
        executor_provider="openai-compatible",
        executor_model="qwen3.5:cloud",
        planner_timeout=30.0,
        executor_timeout=10.0,
        supervisor_mode="deterministic",
        executor_mode="disabled",
        planner_mode="sync",
        prompt_mode="delta",
        replan_policy="strict",
        preflight_executor=False,
    ),
    "qwen-hybrid": BridgeProfile(
        name="qwen-hybrid",
        label="Qwen Hybrid",
        description="Froggy keeps moving while async Qwen planning can refresh the plan; stale planner output is discarded.",
        two_model=True,
        llm_provider="openai-compatible",
        model="qwen3.5:cloud",
        planner_provider="openai-compatible",
        planner_model="qwen3.5:cloud",
        executor_provider="openai-compatible",
        executor_model="qwen3.5:cloud",
        planner_timeout=30.0,
        executor_timeout=10.0,
        supervisor_mode="hybrid",
        executor_mode="health-check",
        planner_mode="async",
        prompt_mode="cached",
        replan_policy="balanced",
        preflight_executor=False,
    ),
    "qwen-experimental-two-model": BridgeProfile(
        name="qwen-experimental-two-model",
        label="Qwen Experimental Two-Model",
        description="Research profile preserving full planner/executor LLM behavior for regression comparison.",
        two_model=True,
        llm_provider="openai-compatible",
        model="qwen3.5:cloud",
        planner_provider="openai-compatible",
        planner_model="qwen3.5:cloud",
        executor_provider="openai-compatible",
        executor_model="qwen3.5:cloud",
        planner_timeout=30.0,
        executor_timeout=10.0,
        supervisor_mode="llm",
        executor_mode="llm",
        planner_mode="sync",
        prompt_mode="full",
        replan_policy="aggressive",
    ),
    "spark-nano": BridgeProfile(
        name="spark-nano",
        label="Spark + Nano",
        description="Future intended GPT-5.3 split. Fails loudly if the endpoint cannot serve Spark and Nano.",
        two_model=True,
        llm_provider="openai-compatible",
        model="gpt-5.3-spark",
        planner_provider="openai-compatible",
        planner_model="gpt-5.3-spark",
        executor_provider="openai-compatible",
        executor_model="gpt-5.3-nano",
        planner_timeout=10.0,
        executor_timeout=2.0,
        supervisor_mode="hybrid",
        executor_mode="llm",
        planner_mode="async",
        prompt_mode="cached",
        replan_policy="balanced",
    ),
    "legacy-single-model": BridgeProfile(
        name="legacy-single-model",
        label="Legacy Single-Model",
        description="Existing single-model advisory baseline kept for rollback and comparison.",
        two_model=False,
        llm_provider="openai-compatible",
        model="qwen3.5:cloud",
        planner_provider="openai-compatible",
        planner_model="qwen3.5:cloud",
        executor_provider="openai-compatible",
        executor_model="qwen3.5:cloud",
        planner_timeout=30.0,
        executor_timeout=10.0,
        supervisor_mode="llm",
        executor_mode="llm",
        planner_mode="sync",
        prompt_mode="full",
        replan_policy="aggressive",
    ),
}


def profile_names() -> list[str]:
    return sorted(PROFILES)


def resolve_profile(name: str | None, overrides: dict[str, Any] | None = None) -> BridgeProfile:
    key = (name or DEFAULT_PROFILE_NAME).strip().lower()
    try:
        profile = PROFILES[key]
    except KeyError as exc:
        raise ValueError(f"unknown LLM profile: {name}") from exc
    resolved = profile.with_overrides(overrides or {})
    validate_profile(resolved)
    return resolved


def validate_profile(profile: BridgeProfile) -> None:
    if profile.supervisor_mode not in SUPERVISOR_MODES:
        raise ValueError(f"invalid supervisor mode: {profile.supervisor_mode}")
    if profile.executor_mode not in EXECUTOR_MODES:
        raise ValueError(f"invalid executor mode: {profile.executor_mode}")
    if profile.planner_mode not in PLANNER_MODES:
        raise ValueError(f"invalid planner mode: {profile.planner_mode}")
    if profile.prompt_mode not in PROMPT_MODES:
        raise ValueError(f"invalid prompt mode: {profile.prompt_mode}")
    if profile.replan_policy not in REPLAN_POLICIES:
        raise ValueError(f"invalid replan policy: {profile.replan_policy}")
    if profile.llm_provider not in PROVIDERS:
        raise ValueError(f"invalid LLM provider: {profile.llm_provider}")
    if profile.planner_provider not in PROVIDERS:
        raise ValueError(f"invalid planner provider: {profile.planner_provider}")
    if profile.executor_provider not in PROVIDERS:
        raise ValueError(f"invalid executor provider: {profile.executor_provider}")
    if profile.planner_timeout <= 0 or profile.executor_timeout <= 0:
        raise ValueError("planner/executor timeouts must be positive")
