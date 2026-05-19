# Session Report: Phase 4 Route Visibility

## Changed

- Added the Bogroot HM route contract and route schema under `routes/`.
- Added `RouteWalker` to expose current Bogroot route phase, step, next waypoint, deviation, and last native Froggy outcome.
- Added `snapshot.route` to Tier 1 snapshots so advisory mode sees route state even between richer snapshot tiers.
- Surfaced route progress and deviation recovery actions in bridge observation summaries.
- Updated the Froggy prompt guidance to prefer wait/query/no-op when native route control is already progressing without deviation.
- Updated character conset restock maintenance to withdraw missing conset stacks from Xunlai before falling back to crafting.

## Validated

- Added offline route contract coverage for route JSON shape, snapshot wiring, route constant usage, observation summary text, and prompt guidance.
- Added offline maintenance contract coverage to ensure stored consets are used before crafting.
- Ran the targeted bridge regression set:
  - `python -m unittest bridge.tests.test_o_protocol_contract bridge.tests.test_p_ipc_backpressure bridge.tests.test_q_handshake bridge.tests.test_r_action_coverage bridge.tests.test_s_route_contract bridge.tests.test_t_maintenance_contract`
- Built and ran the C++ test targets:
  - `cmake --build build --config Release --target gwa3_tests gwa3`
  - `.\build\bin\Release\gwa3_tests.exe`
  - `cmake --build build --config Release --target gwa3_dungeon_tests`
  - `.\build\bin\Release\gwa3_dungeon_tests.exe`
- Built the injected and isolated-lane DLLs:
  - `cmake --build build_injected --config Release --target gwa3`
  - `cmake --build build_disco --config Release --target injector gwa3`
- Completed one live isolated-lane Froggy HM validation run through maintenance, town setup, Sparkfly route, Tekks entry, both Bogroot HM levels, boss chest, reward claim, and return to outpost.
- The live run exercised the maintenance storage fallback: missing character conset armor was withdrawn from Xunlai and maintenance returned success without unnecessary crafting.
- The live run recovered from two Level 2 wipes using the existing checkpoint/DP-removal flow and still completed successfully.

## Deferred

- No Phase 4 route-visibility items are deferred.
