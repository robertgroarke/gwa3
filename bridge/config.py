"""Configuration for the GWA3 LLM Bridge."""

import argparse
import os
from dataclasses import dataclass
from typing import Mapping

from .profiles import (
    DEFAULT_PROFILE_NAME,
    EXECUTOR_MODES,
    PLANNER_MODES,
    PROMPT_MODES,
    REPLAN_POLICIES,
    SUPERVISOR_MODES,
    profile_names,
    resolve_profile,
)


_TRUE_VALUES = {"1", "true", "yes", "on"}


def _env_bool(env: Mapping[str, str], name: str, default: bool = False) -> bool:
    raw = env.get(name)
    if raw is None:
        return default
    return raw.strip().lower() in _TRUE_VALUES


def _env_int(env: Mapping[str, str], name: str, default: int) -> int:
    raw = env.get(name)
    if raw is None or raw.strip() == "":
        return default
    return int(raw)


def _env_float(env: Mapping[str, str], name: str, default: float) -> float:
    raw = env.get(name)
    if raw is None or raw.strip() == "":
        return default
    return float(raw)

# Named pipe path (must match gwa3 DLL)
DEFAULT_PIPE_NAME = r"\\.\pipe\gwa3_llm"

# Default LLM settings
DEFAULT_LLM_URL = "http://localhost:11434/v1"
DEFAULT_MODEL = "qwen3.5:cloud"
DEFAULT_LLM_PROVIDER = "openai-compatible"
DEFAULT_LLM_HOURLY_TOKEN_CAP = 10_000_000
DEFAULT_PLANNER_MODEL = "qwen3.5:cloud"
DEFAULT_EXECUTOR_MODEL = "qwen3.5:cloud"
DEFAULT_PLANNER_PROVIDER = "openai-compatible"
DEFAULT_EXECUTOR_PROVIDER = "openai-compatible"


@dataclass(frozen=True)
class BridgeRuntimeConfig:
    """Typed process-wide settings read from environment variables."""

    pipe_name: str = DEFAULT_PIPE_NAME
    trade_accept_value_tolerance: float = 1.1
    llm_hourly_token_cap: int = DEFAULT_LLM_HOURLY_TOKEN_CAP
    allow_remote_llm: bool = False
    enable_two_model: bool = False
    froggy_autopilot_enabled: bool = False
    froggy_harness_fallback_enabled: bool = False
    llm_trace_enabled: bool = False
    dll_lane: str = ""
    openai_api_key: str | None = None
    run_summary_path: str | None = None

    @classmethod
    def from_env(cls, env: Mapping[str, str] | None = None) -> "BridgeRuntimeConfig":
        env = os.environ if env is None else env
        return cls(
            pipe_name=env.get("GWA3_PIPE_NAME", DEFAULT_PIPE_NAME),
            trade_accept_value_tolerance=_env_float(
                env,
                "GWA3_TRADE_ACCEPT_VALUE_TOLERANCE",
                1.1,
            ),
            llm_hourly_token_cap=_env_int(
                env,
                "GWA3_LLM_HOURLY_TOKEN_CAP",
                DEFAULT_LLM_HOURLY_TOKEN_CAP,
            ),
            allow_remote_llm=_env_bool(env, "GWA3_ALLOW_REMOTE_LLM"),
            enable_two_model=_env_bool(env, "GWA3_ENABLE_TWO_MODEL"),
            froggy_autopilot_enabled=_env_bool(env, "GWA3_FROGGY_AUTOPILOT"),
            froggy_harness_fallback_enabled=_env_bool(
                env,
                "GWA3_FROGGY_HARNESS_FALLBACK",
            ),
            llm_trace_enabled=_env_bool(env, "GWA3_LLM_TRACE"),
            dll_lane=env.get("GWA3_DLL_LANE", "").strip().lower(),
            openai_api_key=env.get("GWA3_OPENAI_API_KEY") or env.get("OPENAI_API_KEY"),
            run_summary_path=env.get("GWA3_RUN_SUMMARY_PATH"),
        )


RUNTIME_CONFIG = BridgeRuntimeConfig.from_env()

# Backward-compatible module constants. New runtime code should prefer
# BridgeRuntimeConfig injection where practical.
PIPE_NAME = RUNTIME_CONFIG.pipe_name

# Trade safety rails
TRADE_ACCEPT_VALUE_TOLERANCE = RUNTIME_CONFIG.trade_accept_value_tolerance
TRADE_HARD_REFUSAL_TERMS = (
    "soulbound",
    "soul bound",
    "account-bound",
    "account bound",
    "character-bound",
    "character bound",
    "customized",
    "customised",
    "dedicated",
    "per-character",
)
TRADE_WHISPER_REFUSAL_TERMS = (
    "/trade",
    "trust",
    "go first",
    "you first",
    "pay first",
    "give first",
    "send first",
    "trade first",
)

# Agent loop settings
MAX_HISTORY_MESSAGES = 50
OBSERVATION_WINDOW_SIZE = 5
AGENT_LOOP_INTERVAL = 0.1  # seconds between agent loop iterations

# Autonomy modes
AUTONOMY_ADVISORY = "advisory"
AUTONOMY_TACTICAL = "tactical"
AUTONOMY_FULL = "full"


def parse_args(runtime_config: BridgeRuntimeConfig | None = None):
    runtime_config = runtime_config or RUNTIME_CONFIG
    parser = argparse.ArgumentParser(description="GWA3 LLM Bridge autonomous agent")
    parser.add_argument(
        "--profile",
        choices=profile_names(),
        default=DEFAULT_PROFILE_NAME,
        help=f"LLM launch profile (default: {DEFAULT_PROFILE_NAME})",
    )
    parser.add_argument(
        "--llm-provider",
        choices=["openai-compatible", "codex-exec"],
        default=None,
        help=(
            "LLM provider backend. The default uses Ollama's OpenAI-compatible "
            "endpoint; codex-exec remains available only as an explicit opt-in."
        ),
    )
    parser.add_argument(
        "--llm-url",
        default=DEFAULT_LLM_URL,
        help=f"OpenAI-compatible API URL (default: {DEFAULT_LLM_URL})",
    )
    parser.add_argument(
        "--model",
        default=None,
        help=f"Model name (default: {DEFAULT_MODEL})",
    )
    parser.add_argument(
        "--two-model",
        action="store_true",
        help="Enable planner/executor mode without changing the legacy single-model defaults.",
    )
    parser.add_argument(
        "--planner-provider",
        choices=["openai-compatible", "codex-exec"],
        default=None,
        help=f"Planner provider for --two-model (default: {DEFAULT_PLANNER_PROVIDER})",
    )
    parser.add_argument(
        "--executor-provider",
        choices=["openai-compatible", "codex-exec"],
        default=None,
        help=f"Executor provider for --two-model (default: {DEFAULT_EXECUTOR_PROVIDER})",
    )
    parser.add_argument(
        "--planner-model",
        default=None,
        help=f"Planner model for --two-model (default: {DEFAULT_PLANNER_MODEL})",
    )
    parser.add_argument(
        "--executor-model",
        default=None,
        help=f"Executor model for --two-model (default: {DEFAULT_EXECUTOR_MODEL})",
    )
    parser.add_argument(
        "--planner-timeout",
        type=float,
        default=None,
        help="Planner request timeout in seconds (default: 10.0)",
    )
    parser.add_argument(
        "--executor-timeout",
        type=float,
        default=None,
        help="Executor request timeout in seconds (default: 10.0 for Qwen cloud)",
    )
    parser.add_argument(
        "--supervisor-mode",
        choices=sorted(SUPERVISOR_MODES),
        default=None,
        help="Override the active profile's supervisor mode.",
    )
    parser.add_argument(
        "--executor-mode",
        choices=sorted(EXECUTOR_MODES),
        default=None,
        help="Override the active profile's executor mode.",
    )
    parser.add_argument(
        "--planner-mode",
        choices=sorted(PLANNER_MODES),
        default=None,
        help="Override the active profile's planner scheduling mode.",
    )
    parser.add_argument(
        "--prompt-mode",
        choices=sorted(PROMPT_MODES),
        default=None,
        help="Override the active profile's planner prompt mode.",
    )
    parser.add_argument(
        "--replan-policy",
        choices=sorted(REPLAN_POLICIES),
        default=None,
        help="Override the active profile's replan policy.",
    )
    parser.add_argument(
        "--autonomy",
        choices=[AUTONOMY_ADVISORY, AUTONOMY_TACTICAL, AUTONOMY_FULL],
        default=AUTONOMY_ADVISORY,
        help="Autonomy level (default: advisory; tactical/full are experimental)",
    )
    parser.add_argument(
        "--advisory",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--pipe",
        default=runtime_config.pipe_name,
        help=f"Named pipe path (default: {DEFAULT_PIPE_NAME})",
    )
    parser.add_argument(
        "--objective",
        default=None,
        help="Standing objective for autonomous play (e.g., 'Farm Bogroot Growths HM repeatedly')",
    )
    parser.add_argument(
        "--kamadan-timeout",
        type=float,
        default=10.0,
        help="Per-source Kamadan HTTP/WebSocket timeout in seconds (default: 10.0)",
    )
    parser.add_argument(
        "--kamadan-cache-ttl",
        type=float,
        default=120.0,
        help="Kamadan search cache TTL in seconds (default: 120.0)",
    )
    parser.add_argument(
        "--codex-exec-timeout",
        type=float,
        default=180.0,
        help="Per-decision timeout for --llm-provider codex-exec in seconds (default: 180.0)",
    )
    parser.add_argument(
        "--llm-hourly-token-cap",
        type=int,
        default=runtime_config.llm_hourly_token_cap,
        help=(
            "Hard rolling token cap for LLM mode in tokens/hour "
            f"(default: {DEFAULT_LLM_HOURLY_TOKEN_CAP})"
        ),
    )
    parser.add_argument(
        "--allow-remote-llm",
        action="store_true",
        default=runtime_config.allow_remote_llm,
        help="Allow remote/cloud LLM backends. Localhost endpoints do not require this flag.",
    )
    parser.add_argument(
        "--http-host",
        default="127.0.0.1",
        help="Bridge HTTP/SSE host (default: 127.0.0.1)",
    )
    parser.add_argument(
        "--http-port",
        type=int,
        default=8765,
        help="Bridge HTTP/SSE port. Use 0 for an ephemeral port (default: 8765)",
    )
    args = parser.parse_args()
    overrides = {
        "llm_provider": args.llm_provider,
        "model": args.model,
        "planner_provider": args.planner_provider,
        "planner_model": args.planner_model,
        "executor_provider": args.executor_provider,
        "executor_model": args.executor_model,
        "planner_timeout": args.planner_timeout,
        "executor_timeout": args.executor_timeout,
        "supervisor_mode": args.supervisor_mode,
        "executor_mode": args.executor_mode,
        "planner_mode": args.planner_mode,
        "prompt_mode": args.prompt_mode,
        "replan_policy": args.replan_policy,
    }
    try:
        profile = resolve_profile(args.profile, overrides)
    except ValueError as exc:
        parser.error(str(exc))
    args.profile = profile.name
    args.profile_state = profile.public_state()
    args.two_model = bool(args.two_model or profile.two_model)
    args.llm_provider = profile.llm_provider
    args.model = profile.model
    args.planner_provider = profile.planner_provider
    args.planner_model = profile.planner_model
    args.executor_provider = profile.executor_provider
    args.executor_model = profile.executor_model
    args.planner_timeout = profile.planner_timeout
    args.executor_timeout = profile.executor_timeout
    args.supervisor_mode = profile.supervisor_mode
    args.executor_mode = profile.executor_mode
    args.planner_mode = profile.planner_mode
    args.prompt_mode = profile.prompt_mode
    args.replan_policy = profile.replan_policy
    args.preflight_executor = profile.preflight_executor
    if args.advisory:
        args.autonomy = AUTONOMY_ADVISORY
    return args
