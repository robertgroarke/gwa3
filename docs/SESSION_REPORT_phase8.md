# Phase 8 - Run Summary Memory

## Changed

- Added runtime-only `RunSummaryMemory` storage for the last 20 run summaries.
- The agent prompt now includes prior summaries as durable context.
- Terminal run events now trigger a short summary turn when an LLM client is available, with deterministic fallback summaries if the model is unavailable.
- Supported terminal events include completed Froggy dungeon runs, maintenance completion, reward chest interaction, and party-defeat return-to-outpost recovery.

## Validated

- Added `bridge.tests.test_x_run_summary_memory` for persistence, prompt injection, summary-turn calls, and event detection.

## Deferred

- Live run validation is still reserved for the DISCO lane after the phased hardening work is locally green.
