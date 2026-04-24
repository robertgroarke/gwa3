# Agent Work Registry

This file is the source of truth for who is working on what.

Use it together with `AGENT_ACCOUNT_REGISTRY.md` to avoid both account collisions and task collisions.

## Rules

1. Before starting, check whether the subsystem or file area is already claimed.
2. If you take a task, add or update a row first.
3. Keep the scope concrete enough to avoid overlap.
4. When you stop working on a task, mark it `available` or remove the row if it was temporary.
5. If another agent already owns a live area, do not enter it without an explicit handoff.

## Status Meanings

- `available`: no active owner
- `active`: currently owned by an agent
- `blocked`: owned but waiting on an external dependency or decision
- `handoff`: ready for another agent to continue

## Work Map

| Work Area | Status | Owner | Scope | Primary Files | Notes |
|---|---|---|---|---|---|
| `agent-coordination` | `handoff` | `BISCUIT` | account registry, work registry, lane safety, supporting scripts/tests | `AGENTS.md`, `AGENT_ACCOUNT_REGISTRY.md`, `AGENT_WORK_REGISTRY.md`, `scripts/agent_registry.py`, `scripts/agent_work_registry.py`, `gwa3/bridge/tests/test_h_trade_harness_config.py`, `gwa3/bridge/tests/test_i_agent_registry.py`, `gwa3/bridge/tests/test_j_agent_registry_cli.py`, `gwa3/bridge/tests/test_k_agent_work_registry.py` | coordination infrastructure is in place |
| `arachnis-haunt-blumpkins-validation` | `active` | `CODEX` | user-directed BLUMPKINS Arachnis Haunt workable-state push; keep edits narrow around Arachnis runtime, shared dungeon helpers it depends on, and integration-style validation | `gwa3/src/bot/ArachnisHaunt.cpp,gwa3/src/bot/ArachnisHauntBot.cpp,gwa3/include/gwa3/bot/ArachnisHaunt.h,gwa3/include/gwa3/bot/ArachnisHauntBot.h,gwa3/src/bot/Dungeon*.cpp,gwa3/include/gwa3/bot/Dungeon*.h,gwa3/tests/test_arachnis_haunt.cpp,gwa3/tests/test_arachnis_haunt_bot.cpp,gwa3/src/tests/IntegrationTestEpic14.cpp,gwa3/src/tests/IntegrationTest.cpp,gwa3/CMakeLists.txt` | Operator override from user while broader dungeon-port-shared-helpers area remains owned by MARVIN |
| `consumable-crafting-debug` | `active` | `MARVIN` | FroggyHM shared-helper cutover and validation on MARVIN lane; live refactor takeover after BISCUIT stopped | `gwa3/src/bot/FroggyHM.cpp,gwa3/CONSUMABLE_CRAFTING_DEBUG.md,gwa3/src/dllmain.cpp,gwa3/tools/injector.cpp,gwa3/src/tests/IntegrationTestInternal.h,gwa3/src/tests/IntegrationTestSession.cpp,gwa3/include/gwa3/core/SmokeTest.h` | - |
| `dungeon-port-shared-helpers` | `active` | `MARVIN` | shared dungeon helper extraction plus typed and standalone runtime ports for Rragar, Kathandrax, Frostmaw, Ravens, and Arachnis with offline MARVIN-lane validation and shared dungeon bot module registry wiring | `gwa3/DUNGEON_PORT_IMPLEMENTATION_PLAN.md,gwa3/DUNGEON_PORT_KANBAN.md,gwa3/include/gwa3/bot/DungeonTravel.h,gwa3/src/bot/DungeonTravel.cpp,gwa3/include/gwa3/bot/DungeonRoute.h,gwa3/src/bot/DungeonRoute.cpp,gwa3/include/gwa3/bot/DungeonCheckpoint.h,gwa3/src/bot/DungeonCheckpoint.cpp,gwa3/include/gwa3/bot/DungeonDialog.h,gwa3/src/bot/DungeonDialog.cpp,gwa3/include/gwa3/bot/DungeonQuest.h,gwa3/src/bot/DungeonQuest.cpp,gwa3/include/gwa3/bot/DungeonQuestRuntime.h,gwa3/src/bot/DungeonQuestRuntime.cpp,gwa3/include/gwa3/bot/DungeonBundle.h,gwa3/src/bot/DungeonBundle.cpp,gwa3/include/gwa3/bot/BotModuleSelector.h,gwa3/src/bot/BotModuleSelector.cpp,gwa3/include/gwa3/bot/BotModuleRegistry.h,gwa3/src/bot/BotModuleRegistry.cpp,gwa3/include/gwa3/bot/RragarsMenagerie.h,gwa3/src/bot/RragarsMenagerie.cpp,gwa3/include/gwa3/bot/RragarsMenagerieBot.h,gwa3/src/bot/RragarsMenagerieBot.cpp,gwa3/include/gwa3/bot/Kathandrax.h,gwa3/src/bot/Kathandrax.cpp,gwa3/include/gwa3/bot/KathandraxBot.h,gwa3/src/bot/KathandraxBot.cpp,gwa3/include/gwa3/bot/FrostmawsBurrows.h,gwa3/src/bot/FrostmawsBurrows.cpp,gwa3/include/gwa3/bot/FrostmawsBurrowsBot.h,gwa3/src/bot/FrostmawsBurrowsBot.cpp,gwa3/include/gwa3/bot/RavensPoint.h,gwa3/src/bot/RavensPoint.cpp,gwa3/include/gwa3/bot/RavensPointBot.h,gwa3/src/bot/RavensPointBot.cpp,gwa3/include/gwa3/bot/ArachnisHaunt.h,gwa3/src/bot/ArachnisHaunt.cpp,gwa3/include/gwa3/bot/ArachnisHauntBot.h,gwa3/src/bot/ArachnisHauntBot.cpp,gwa3/tests/dungeon_tests_main.cpp,gwa3/tests/test_dungeon_travel.cpp,gwa3/tests/test_dungeon_route.cpp,gwa3/tests/test_dungeon_checkpoint.cpp,gwa3/tests/test_dungeon_dialog.cpp,gwa3/tests/test_dungeon_quest.cpp,gwa3/tests/test_dungeon_quest_runtime.cpp,gwa3/tests/test_dungeon_bundle.cpp,gwa3/tests/test_rragars_menagerie.cpp,gwa3/tests/test_kathandrax.cpp,gwa3/tests/test_kathandrax_bot.cpp,gwa3/tests/test_frostmaws_burrows.cpp,gwa3/tests/test_frostmaws_burrows_bot.cpp,gwa3/tests/test_ravens_point.cpp,gwa3/tests/test_ravens_point_bot.cpp,gwa3/tests/test_arachnis_haunt.cpp,gwa3/tests/test_arachnis_haunt_bot.cpp,gwa3/tests/test_bot_module_selector.cpp,gwa3/tests/test_bot_module_registry.cpp,gwa3/CMakeLists.txt` | - |
| `froggy-hm-e2e-beastrit` | `active` | `MARVIN` | FroggyHM live refactor takeover; BEASTRIT session stopped, MARVIN continuing helper convergence in FroggyHM.cpp | `gwa3/src/tests/IntegrationTest.cpp,gwa3/src/bot/FroggyHM.cpp,gwa3/tools/run_froggy_test.ps1,gwa3/bridge/tests/marvin_harness.py,gwa3/bridge/tests/__main__.py,gwa3/bridge/tests/test_h_marvin_harness.py` | - |
| `froggy-shared-refactor-staging` | `active` | `MARVIN` | staged Froggy shared-function extraction in a separate directory, using adapters over Dungeon* helpers without editing active FroggyHM.cpp | `gwa3/refactor_staging/froggy_shared/FROGGY_SHARED_EXTRACTION_PLAN.md,gwa3/refactor_staging/froggy_shared/FroggySharedAdapters.h,gwa3/refactor_staging/froggy_shared/FroggySharedAdapters.cpp,gwa3/tests/test_froggy_shared_adapters.cpp,gwa3/tests/dungeon_tests_main.cpp,gwa3/CMakeLists.txt` | - |
| `hook-marker-instrumentation` | `available` | `-` | MAP file generation + CrashDiag MAP parser for RVA-to-function-name resolution in crash stacks, HookMarker VEH-safe DumpOnCrash, HookScope on PacketSendTap + EngineDetour asm tick | `gwa3/CMakeLists.txt,gwa3/src/core/HookMarker.h,gwa3/src/core/HookMarker.cpp,gwa3/src/core/CrashDiag.h,gwa3/src/core/CrashDiag.cpp,gwa3/src/packets/CtoS.cpp` | - |
| `kamadan-bridge` | `handoff` | `BISCUIT` | get_trader_quotes, search/trader tool dispatch, bridge prompt/test hardening | `gwa3/bridge/kamadan_client.py`, `gwa3/bridge/agent_loop.py`, `gwa3/bridge/tests/test_g_kamadan.py` | bridge work is stable; avoid live trade coupling here |
| `player-trade-validation` | `active` | `CODEX` | player-trade quantity-dialog automation, native offer path, Py4GW/GWToolbox comparison, focused stackable trade tests | `gwa3/src/managers/TradeMgr.cpp,gwa3/src/managers/UIMgr.cpp,gwa3/bridge/tests/test_f_player_trade.py,gwa3/bridge/tests/trade_harness.py,gwa3/src/llm/ActionExecutor.cpp,gwa3/src/llm/GameSnapshot.cpp` | - |
| `quest-log` | `active` | `BISCUIT` | Quest log reading and manipulation: snapshot exposure, abandon/select quest actions, LLM bridge integration, unit + integration + bridge tests | `gwa3/include/gwa3/managers/QuestMgr.h,gwa3/src/managers/QuestMgr.cpp,gwa3/src/llm/GameSnapshot.cpp,gwa3/src/llm/ActionExecutor.cpp,gwa3/bridge/tool_schema.py,gwa3/bridge/tests/test_m_quest_log.py,gwa3/src/packets/CtoS.cpp,gwa3/include/gwa3/packets/CtoS.h` | - |
| `ravens-point-beastrit-validation` | `active` | `BEASTRIT` | user-directed BEASTRIT Raven's Point workable-state push and integration-style validation; keep changes narrow around Raven runtime/shared helpers/tests | `gwa3/src/bot/RavensPoint.cpp,gwa3/src/bot/RavensPointBot.cpp,gwa3/include/gwa3/bot/RavensPoint.h,gwa3/tests/test_ravens_point.cpp,gwa3/tests/test_ravens_point_bot.cpp,gwa3/tests/dungeon_tests_main.cpp,gwa3/src/tests/IntegrationTestEpic14.cpp,gwa3/src/tests/IntegrationTest.cpp,gwa3/CMakeLists.txt` | Operator override from user while broader dungeon-port area remains owned by MARVIN |
| `rragars-ravens-disco-integration` | `active` | `DISCO` | DISCO lane Rragars Menagerie workable-state push plus Raven-style integration test wiring; shared dungeon helper cutover only where needed for Rragars/Ravens runtime reuse | `gwa3/src/bot/RragarsMenagerie.cpp,gwa3/src/bot/RragarsMenagerieBot.cpp,gwa3/include/gwa3/bot/RragarsMenagerie.h,gwa3/src/bot/RavensPoint.cpp,gwa3/src/bot/RavensPointBot.cpp,gwa3/include/gwa3/bot/RavensPoint.h,gwa3/src/tests/IntegrationTestEpic14.cpp,gwa3/tools/run_ravens_point_test.ps1,gwa3/tests/test_rragars_menagerie.cpp,gwa3/tests/test_ravens_point.cpp,gwa3/tests/test_ravens_point_bot.cpp,gwa3/CMakeLists.txt` | User-directed DISCO override; avoid unrelated Froggy/runtime changes and coordinate around existing MARVIN/BEASTRIT dungeon claims. |

## Suggested Usage

```powershell
python scripts/agent_work_registry.py list
python scripts/agent_work_registry.py claim --area "my-area" --owner "BISCUIT" --scope "what I am changing" --files "path1,path2"
python scripts/agent_work_registry.py release --area "my-area"
```

Use [scripts/agent_work_registry.py](C:\Users\Robert\Documents\GWA Censured X BotsHub\scripts\agent_work_registry.py) instead of hand-editing the table when possible.
