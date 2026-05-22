"""Prompt asset loading for bridge agent loops."""

from __future__ import annotations

from functools import lru_cache
from pathlib import Path


_PROMPT_DIR = Path(__file__).with_name("prompts")


@lru_cache(maxsize=None)
def load_prompt(filename: str) -> str:
    """Load a checked-in prompt file by name."""
    path = _PROMPT_DIR / filename
    return path.read_text(encoding="utf-8").strip()


SYSTEM_PROMPT = load_prompt("agent_system.md")
FROGGY_HIGH_LEVEL_RUNBOOK = load_prompt("froggy_high_level_runbook.md")
PLANNER_SYSTEM_PROMPT = load_prompt("planner_system.md")
EXECUTOR_SYSTEM_PROMPT = load_prompt("executor_system.md")
RUN_SUMMARY_SYSTEM_PROMPT = load_prompt("run_summary_system.md")
