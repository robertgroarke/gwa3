"""Configuration for the GWA3 LLM Bridge."""

import argparse
import os

# Named pipe path (must match gwa3 DLL)
PIPE_NAME = os.environ.get("GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm")

# Default LLM settings. Public builds default to Ollama's OpenAI-compatible
# localhost endpoint so cloud use is an explicit operator choice.
DEFAULT_LLM_URL = "http://localhost:11434/v1"
DEFAULT_MODEL = "qwen3.5:cloud"
DEFAULT_LLM_PROVIDER = "openai-compatible"
DEFAULT_LLM_HOURLY_TOKEN_CAP = 10_000_000
DEFAULT_PLANNER_MODEL = "qwen3.5:cloud"
DEFAULT_EXECUTOR_MODEL = "qwen3.5:cloud"
DEFAULT_PLANNER_PROVIDER = "openai-compatible"
DEFAULT_EXECUTOR_PROVIDER = "openai-compatible"

# Trade safety rails
TRADE_ACCEPT_VALUE_TOLERANCE = float(os.environ.get("GWA3_TRADE_ACCEPT_VALUE_TOLERANCE", "1.1"))
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


def parse_args():
    parser = argparse.ArgumentParser(description="GWA3 LLM Bridge autonomous agent")
    parser.add_argument(
        "--llm-provider",
        choices=["openai-compatible", "codex-exec"],
        default=DEFAULT_LLM_PROVIDER,
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
        default=DEFAULT_MODEL,
        help=f"Model name (default: {DEFAULT_MODEL})",
    )
    parser.add_argument(
        "--two-model",
        action="store_true",
        help="Enable planner/executor mode without changing legacy single-model usage.",
    )
    parser.add_argument(
        "--planner-provider",
        choices=["openai-compatible", "codex-exec"],
        default=DEFAULT_PLANNER_PROVIDER,
        help=f"Planner provider for --two-model (default: {DEFAULT_PLANNER_PROVIDER})",
    )
    parser.add_argument(
        "--executor-provider",
        choices=["openai-compatible", "codex-exec"],
        default=DEFAULT_EXECUTOR_PROVIDER,
        help=f"Executor provider for --two-model (default: {DEFAULT_EXECUTOR_PROVIDER})",
    )
    parser.add_argument(
        "--planner-model",
        default=DEFAULT_PLANNER_MODEL,
        help=f"Planner model for --two-model (default: {DEFAULT_PLANNER_MODEL})",
    )
    parser.add_argument(
        "--executor-model",
        default=DEFAULT_EXECUTOR_MODEL,
        help=f"Executor model for --two-model (default: {DEFAULT_EXECUTOR_MODEL})",
    )
    parser.add_argument(
        "--planner-timeout",
        type=float,
        default=10.0,
        help="Planner request timeout in seconds (default: 10.0)",
    )
    parser.add_argument(
        "--executor-timeout",
        type=float,
        default=10.0,
        help="Executor request timeout in seconds (default: 10.0 for cloud-backed local endpoints)",
    )
    parser.add_argument(
        "--autonomy",
        choices=[AUTONOMY_ADVISORY, AUTONOMY_TACTICAL, AUTONOMY_FULL],
        default=AUTONOMY_TACTICAL,
        help="Autonomy level (default: tactical)",
    )
    parser.add_argument(
        "--advisory",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--pipe",
        default=PIPE_NAME,
        help=f"Named pipe path (default: {PIPE_NAME})",
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
        default=int(os.environ.get("GWA3_LLM_HOURLY_TOKEN_CAP", DEFAULT_LLM_HOURLY_TOKEN_CAP)),
        help=(
            "Hard rolling token cap for LLM mode in tokens/hour "
            f"(default: {DEFAULT_LLM_HOURLY_TOKEN_CAP})"
        ),
    )
    parser.add_argument(
        "--allow-remote-llm",
        action="store_true",
        default=os.environ.get("GWA3_ALLOW_REMOTE_LLM", "").strip().lower()
        in {"1", "true", "yes", "on"},
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
    if args.advisory:
        args.autonomy = AUTONOMY_ADVISORY
    return args
