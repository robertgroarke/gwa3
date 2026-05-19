# Phase 7 Session Report

## Changed

- Added `bridge/trade_guard.py`.
- Added config-tunable trade safety constants in `bridge/config.py`:
  - accept value tolerance,
  - hard refusal terms for bound/customized/per-character item text,
  - whisper refusal terms for common scam phrasing.
- Wrapped `accept_trade` in `AgentLoop` so unsafe accepts return structured `trade_unsafe:<reason>` results without sending a game action.
- Recorded the partner offer signature after successful `submit_trade_offer` so later `accept_trade` detects partner-side changes.
- Wrapped `send_whisper` so trade-scam phrasing is refused before reaching the game client.
- Added `bridge/tests/test_w_trade_guard.py` and documented it.

## Validated

- Trade guard and conditional tool tests:
  `python -m unittest bridge.tests.test_w_trade_guard bridge.tests.test_v_conditional_tools`
- Token-budget and action-coverage tests:
  `python -m unittest bridge.tests.test_o_token_budget bridge.tests.test_r_action_coverage`

## Deferred

- Kamadan median ingestion is represented by an injectable `kamadan_medians` map in the guard and tested deterministically. Wiring live item-name-to-median extraction from search results remains a future improvement because the current trade snapshot does not consistently expose item names for both parties.
