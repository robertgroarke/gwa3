"""Wrapper that sets the env then invokes the bridge test runner.
Git-bash on Windows mangles `\\.\pipe\...` env values, so we set the
variable inside Python instead of at the shell level."""
import asyncio
import os
import sys

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.tests.runner import run_all


if __name__ == "__main__":
    filter_pattern = sys.argv[1] if len(sys.argv) > 1 else None
    exit_code = asyncio.run(run_all(filter_pattern=filter_pattern))
    sys.exit(exit_code)
