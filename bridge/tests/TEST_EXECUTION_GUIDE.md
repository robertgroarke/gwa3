# Bridge Test Execution Guide

## Offline Contract Tests

Run public-safe bridge contract tests from the repository root:

```powershell
python -m unittest bridge.tests.test_o_protocol_contract bridge.tests.test_p_ipc_backpressure bridge.tests.test_q_handshake bridge.tests.test_r_action_coverage bridge.tests.test_s_route_contract bridge.tests.test_t_maintenance_contract bridge.tests.test_u_snapshot_diet bridge.tests.test_v_conditional_tools bridge.tests.test_w_trade_guard bridge.tests.test_x_run_summary_memory
```

These tests do not require a live Guild Wars client unless explicitly enabled by environment variable.

## Live Action Coverage

`bridge.tests.test_r_action_coverage` includes an optional live dispatch probe. It requires an isolated injected client and a matching named pipe:

```powershell
$env:GWA3_PIPE_NAME='\\.\pipe\gwa3_llm_<lane>'
$env:GWA3_LIVE_ACTION_COVERAGE='1'
python -m unittest bridge.tests.test_r_action_coverage
Remove-Item Env:\GWA3_LIVE_ACTION_COVERAGE
```

The live probe sends `{}` to schema tools whose missing-parameter path is expected to be safe and verifies each one returns an `action_result` other than `unknown_action`. Tools handled locally by the Python bridge and no-parameter tools with live side effects are covered by static registration checks instead.

## Route Contract

`bridge.tests.test_s_route_contract` validates the public Bogroot HM route script, Tier 1 snapshot route wiring, route status prompt text, and route-aware observation summary formatting. It is offline and does not require a live client.

## Maintenance Contract

`bridge.tests.test_t_maintenance_contract` verifies the character conset restock path pulls missing consets from Xunlai storage before falling back to Embark crafting. This prevents stored consets from being ignored when a crafter interaction is flaky.

## Snapshot Diet

`bridge.tests.test_u_snapshot_diet` validates the Phase 5 prompt and wire-size contract: context summaries show only the nearest five foes plus interrupt-priority casters, repeated ground-item details are suppressed until the item set changes, skillbar output is rendered as changed slots after the first full bar, and higher snapshot tiers advertise delta semantics.

## Conditional Tool Exposure

`bridge.tests.test_v_conditional_tools` validates the Phase 6 tool selector. Player-trade mutation tools are hidden until the trade window is open, identify/salvage tools are hidden until inventory contains eligible items, and dungeon-only helpers are hidden outside known dungeon maps.

## Trade Guard

`bridge.tests.test_w_trade_guard` validates the Phase 7 player-trade safety guard. It blocks accept-trade when the partner offer changes after submit, when value is outside tolerance, when player-offered items match hard refusal terms, and when whisper text contains common scam strings.

## Run Summary Memory

`bridge.tests.test_x_run_summary_memory` covers persistent last-20 run summaries, prompt injection of prior summaries, and terminal-event hooks for completed dungeon runs, maintenance completion, chest interaction, and defeat recovery.
