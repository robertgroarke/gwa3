# Phase 10 - Monolith Cleanup

## Changed

- Split the `ActionExecutor` dispatch table into functional registration helpers for movement, combat, interaction, quests, party, travel, items, trade/crafting, Froggy, bot control, and utility actions.
- Moved bot-control and utility action handlers into `ActionExecutorUtility.cpp` behind a private `ActionExecutorInternal.h` dispatch contract.
- Moved movement action handlers into `ActionExecutorMovement.cpp` with the same private dispatch contract.
- Moved combat action handlers into `ActionExecutorCombat.cpp`, keeping 0-based bridge slot handling and map-readiness behavior intact.
- Moved interaction and dialog action handlers into `ActionExecutorInteraction.cpp`.
- Moved quest action handlers into `ActionExecutorQuest.cpp`.
- Updated the action coverage test to scan all `ActionExecutor*.cpp` sources, so future physical splits remain covered.
- Kept handler bodies in place to avoid churn in live-client-sensitive action implementations.

## Validated

- The action coverage test remains the primary guard because it proves schema tools still resolve to registered C++ handlers.
- Post-live-validation split gate:
  - `cmake --build build --config Release --target gwa3_tests gwa3`
  - `.\build\bin\Release\gwa3_tests.exe`
  - `cmake --build build --config Release --target gwa3_dungeon_tests`
  - `.\build\bin\Release\gwa3_dungeon_tests.exe`
  - `cmake --build build_injected --config Release --target gwa3`
  - `cmake --build build_disco --config Release --target injector gwa3`
  - `python -m unittest bridge.tests.test_o_protocol_contract bridge.tests.test_p_ipc_backpressure bridge.tests.test_q_handshake bridge.tests.test_r_action_coverage bridge.tests.test_s_route_contract bridge.tests.test_t_maintenance_contract bridge.tests.test_u_snapshot_diet bridge.tests.test_v_conditional_tools bridge.tests.test_w_trade_guard bridge.tests.test_x_run_summary_memory`
  - `git diff --check`
  - redundant GWA3 alias scan

## Deferred

- `GameSnapshot.cpp` physical file splitting is intentionally deferred. Its current SEH/json boundary is delicate, and the existing per-builder isolation is safer than moving memory-reading helpers without live validation evidence.
- Full `ActionExecutor.cpp` physical category split is still incremental. Combat, movement, interaction, quest, bot-control, and utility handlers are now split; larger trade, Froggy, and route handlers remain in the monolith because they are tightly coupled to shared local helpers and hook-sensitive managers.
