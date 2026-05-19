# Phase 9 - Game Data Regeneration CI

## Changed

- `bridge/generate_gamedata.py` now accepts `--source-root`, `--output`, and `--check`.
- `--check` regenerates into a temporary file and fails with a unified diff when committed `bridge/gamedata.py` is stale.
- CI now checks out the public BotsHub source and runs the gamedata freshness check before builds/tests.
- Regenerated `bridge/gamedata.py` from the same public source CI uses.

## Validated

- `python bridge/generate_gamedata.py --check --source-root <public BotsHub clone>` passes.

## Deferred

- No live-client validation is needed for generated lookup-table freshness.
