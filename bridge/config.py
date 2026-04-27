"""Configuration for the GWA3 LLM Bridge."""

import argparse
import os

# Named pipe path (must match gwa3 DLL)
PIPE_NAME = os.environ.get("GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm")

# Default LLM settings
DEFAULT_LLM_URL = "http://localhost:8000/v1"
DEFAULT_MODEL = "gemma-4-32b-it"

# Agent loop settings
MAX_HISTORY_MESSAGES = 50
OBSERVATION_WINDOW_SIZE = 5
AGENT_LOOP_INTERVAL = 0.1  # seconds between agent loop iterations

# Autonomy modes
AUTONOMY_ADVISORY = "advisory"
AUTONOMY_TACTICAL = "tactical"
AUTONOMY_FULL = "full"


def parse_args():
    parser = argparse.ArgumentParser(description="GWA3 LLM Bridge — Gemma 4 Agent")
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
        "--autonomy",
        choices=[AUTONOMY_ADVISORY, AUTONOMY_TACTICAL, AUTONOMY_FULL],
        default=AUTONOMY_TACTICAL,
        help="Autonomy level (default: tactical)",
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
    return parser.parse_args()
