"""Run L02 and L03 in different orders to pin down the flake.

Matrix:
  A. L02 alone            -> expect PASS (baseline)
  B. L03 alone            -> expect PASS (baseline)
  C. L02, L02, L02        -> same test repeated, rules out 1st-run caching
  D. L03, L02             -> the known failing combination
  E. L02, L03             -> reverse order

Output is a compact summary so the flake pattern is visible at a glance.
"""

import asyncio
import os
import sys

os.environ["GWA3_PIPE_NAME"] = r"\\.\pipe\gwa3_llm_biscuit"
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bridge.tests.runner import run_single_test
from bridge.tests import test_m_quest_log_llm as m


L02 = ("test_llm_scripted_gemma_switches_active_quest",
       m.test_llm_scripted_gemma_switches_active_quest)
L03 = ("test_llm_scripted_gemma_requests_quest_info",
       m.test_llm_scripted_gemma_requests_quest_info)


async def run_sequence(label: str, sequence):
    print(f"\n=== {label} ===")
    for (name, func) in sequence:
        status, detail, elapsed = await run_single_test(name, func, timeout=30.0)
        marker = {"PASS": "PASS", "FAIL": "FAIL", "SKIP": "SKIP"}.get(status, status)
        print(f"  [{marker}] {name} ({elapsed:.1f}s) {detail}")


async def main():
    await run_sequence("A: L02 alone", [L02])
    await run_sequence("B: L03 alone", [L03])
    await run_sequence("C: L02 x3", [L02, L02, L02])
    await run_sequence("D: L03 then L02 (known flake)", [L03, L02])
    await run_sequence("E: L02 then L03", [L02, L03])


if __name__ == "__main__":
    asyncio.run(main())
