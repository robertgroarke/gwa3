"""Runtime token/cost guard for the LLM bridge."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from time import monotonic
from typing import Mapping
from urllib.parse import urlparse


DEFAULT_HOURLY_TOKEN_CAP = 10_000_000
DEFAULT_SPIKE_WINDOW_SECONDS = 60.0
DEFAULT_SPIKE_MULTIPLIER = 1.5
DEFAULT_DOWNGRADE_SECONDS = 120.0
_LOCAL_HOSTS = {"", "localhost", "127.0.0.1", "::1"}


class TokenBudgetError(RuntimeError):
    """Base class for token budget failures."""


class RemoteLlmNotAllowed(TokenBudgetError):
    """Raised when a remote LLM backend is requested without opt-in."""


class TokenBudgetExceeded(TokenBudgetError):
    """Raised when the rolling token budget is exhausted."""


@dataclass
class TokenBudgetGuard:
    """Track LLM usage and enforce a rolling hourly token budget."""

    hourly_token_cap: int = DEFAULT_HOURLY_TOKEN_CAP
    allow_remote: bool = False
    remote_backend: bool = False
    backend_label: str = "openai-compatible"
    clock: callable = monotonic
    spike_window_seconds: float = DEFAULT_SPIKE_WINDOW_SECONDS
    spike_multiplier: float = DEFAULT_SPIKE_MULTIPLIER
    downgrade_seconds: float = DEFAULT_DOWNGRADE_SECONDS
    _usage_events: deque[tuple[float, int]] = field(default_factory=deque)
    _downgrade_until: float = 0.0

    @classmethod
    def for_openai_endpoint(
        cls,
        endpoint: str,
        *,
        hourly_token_cap: int = DEFAULT_HOURLY_TOKEN_CAP,
        allow_remote: bool = False,
    ) -> "TokenBudgetGuard":
        guard = cls(
            hourly_token_cap=hourly_token_cap,
            allow_remote=allow_remote,
            remote_backend=not is_local_llm_url(endpoint),
            backend_label=endpoint,
        )
        guard.validate_remote_policy()
        return guard

    @classmethod
    def for_codex_exec(
        cls,
        *,
        hourly_token_cap: int = DEFAULT_HOURLY_TOKEN_CAP,
        allow_remote: bool = False,
    ) -> "TokenBudgetGuard":
        guard = cls(
            hourly_token_cap=hourly_token_cap,
            allow_remote=allow_remote,
            remote_backend=True,
            backend_label="codex-exec",
        )
        guard.validate_remote_policy()
        return guard

    def validate_remote_policy(self) -> None:
        if self.remote_backend and not self.allow_remote:
            raise RemoteLlmNotAllowed(
                f"Remote LLM backend '{self.backend_label}' requires --allow-remote-llm."
            )
        if self.hourly_token_cap <= 0:
            raise TokenBudgetError("--llm-hourly-token-cap must be greater than zero.")

    @property
    def total_tokens_last_hour(self) -> int:
        self._prune()
        return sum(tokens for _, tokens in self._usage_events)

    @property
    def remaining_tokens_last_hour(self) -> int:
        return max(0, self.hourly_token_cap - self.total_tokens_last_hour)

    @property
    def tier1_only(self) -> bool:
        return self.clock() < self._downgrade_until

    def check_before_request(self) -> None:
        if self.remaining_tokens_last_hour <= 0:
            raise TokenBudgetExceeded(
                f"LLM token budget exhausted: {self.hourly_token_cap:,} tokens/hour."
            )

    def record_usage(self, usage: Mapping | None) -> int:
        tokens = extract_total_tokens(usage)
        if tokens <= 0:
            return 0

        now = self.clock()
        self._usage_events.append((now, tokens))
        self._prune(now)

        total = self.total_tokens_last_hour
        if total > self.hourly_token_cap:
            raise TokenBudgetExceeded(
                "LLM token budget exceeded: "
                f"{total:,}/{self.hourly_token_cap:,} tokens in the last hour."
            )

        self._update_downgrade_state(now)
        return tokens

    def _prune(self, now: float | None = None) -> None:
        now = self.clock() if now is None else now
        cutoff = now - 3600.0
        while self._usage_events and self._usage_events[0][0] < cutoff:
            self._usage_events.popleft()

    def _update_downgrade_state(self, now: float) -> None:
        window_cutoff = now - self.spike_window_seconds
        window_tokens = sum(
            tokens for timestamp, tokens in self._usage_events if timestamp >= window_cutoff
        )
        observed_per_second = window_tokens / max(self.spike_window_seconds, 1.0)
        baseline_per_second = self.hourly_token_cap / 3600.0
        if observed_per_second > baseline_per_second * self.spike_multiplier:
            self._downgrade_until = max(self._downgrade_until, now + self.downgrade_seconds)


def extract_total_tokens(usage: Mapping | None) -> int:
    if not usage:
        return 0
    for key in ("total_tokens", "tokens", "input_output_tokens"):
        value = usage.get(key)
        if isinstance(value, int):
            return max(0, value)
    prompt = usage.get("prompt_tokens") or usage.get("input_tokens") or 0
    completion = usage.get("completion_tokens") or usage.get("output_tokens") or 0
    if isinstance(prompt, int) and isinstance(completion, int):
        return max(0, prompt + completion)
    return 0


def is_local_llm_url(url: str) -> bool:
    parsed = urlparse(url)
    if parsed.scheme in {"", "file"}:
        return True
    return (parsed.hostname or "").lower() in _LOCAL_HOSTS
