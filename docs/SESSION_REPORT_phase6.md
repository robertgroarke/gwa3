# Phase 6 Session Report

## Changed

- Added observation-aware tool filtering in `bridge/tool_schema.py`.
- Updated `AgentLoop._tools_for_current_objective()` to pass the latest merged observation through the conditional selector.
- Player-trade mutation tools are exposed only when `trade.is_open` is true.
- Identify/salvage tools are exposed only when inventory has an unidentified or material-salvageable item.
- Dungeon-only Froggy loop control is exposed only inside known Bogroot dungeon maps.
- Added `bridge/tests/test_v_conditional_tools.py` and documented the test.

## Validated

- Conditional tool tests and action coverage:
  `python -m unittest bridge.tests.test_v_conditional_tools bridge.tests.test_r_action_coverage`
- Token-budget tests:
  `python -m unittest bridge.tests.test_o_token_budget`

## Deferred

- Small open-model hallucination measurement is not run locally in this slice. The deterministic test proves the exposed tool count drops by state, but model-specific hallucination-rate measurement needs a configured local small model and repeatable eval prompt set.
