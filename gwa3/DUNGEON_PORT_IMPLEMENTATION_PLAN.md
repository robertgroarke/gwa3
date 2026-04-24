# Dungeon Port Implementation Plan

## Goal

Port the following AutoIt dungeon bots to the `gwa3` C++ bot stack while extracting the repeated logic into shared helpers instead of cloning Froggy-specific code:

- `Ravens Point`
- `Rragar's Menagerie`
- `Arachnis Haunt`
- `Catacombs of Kathandrax`
- `Frostmaw's Burrows`

## Constraints

- MARVIN-only work lane for planning and implementation.
- Do not reuse or modify another agent's build lane or launcher.
- Avoid broad refactors inside [FroggyHM.cpp](C:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/src/bot/FroggyHM.cpp) while another agent is actively debugging Froggy behavior.
- Prefer new shared files and narrow integrations first.

## Source Inventory

The AutoIt source set in `C:\Users\Robert\Documents\Updated GWA2 Scripts Nov 28 2024\Dungeons GWA Logic Censured NEW DEC06` shows strong overlap with the Froggy flow:

- Shared setup pattern: `Setup`, hero loadout, mode/title initialization.
- Shared travel pattern: `RndTravel`, `RunToDungeon_*`, `WaitMapLoading`.
- Shared combat-route pattern: `MoveandAggroEx`.
- Shared failure recovery: wipe detection, rez wait, morale checks, restart waypoint selection.
- Shared interactions: chest, signpost, bundle, brazier, keg, blast door, quest checkpoint.

## Target Architecture

### 1. Shared foundation helpers

- `DungeonTravel`
  - Random district selection from the AutoIt region/language table.
  - Travel plan construction separate from packet execution so it can be unit tested offline.
- `DungeonRoute`
  - Shared waypoint model with labels and fight radii.
  - Shared nearest-waypoint and checkpoint helpers.
- `DungeonRecovery`
  - Wipe, re-entry, morale, reconnect, and restart-waypoint logic.
- `DungeonInteractions`
  - Reusable helpers for chest tracking, signposts, keys, bundles, braziers, kegs, and blast doors.
- `DungeonRunContext`
  - Run counters, timers, and common outpost/explorable setup.

### 2. Dungeon-specific modules

- `FroggyHM`
  - Gradually reduced to Froggy-only routing and encounter rules.
- `RavensPoint`
  - Torch and brazier-specific bundle flow.
- `RragarsMenagerie`
  - Keg and blast-door flow.
- `ArachnisHaunt`
  - Staff/brazier/egg-specific flow.
- `Kathandrax`
  - Keg checkpoints and lava-route variants.
- `FrostmawsBurrows`
  - Similar checkpoint and route logic with Frostmaw-specific event handling.

## Port Sequence

### Phase 1: Foundations

1. Extract shared random-district travel helper.
2. Extract shared waypoint data model and label constants.
3. Extract wipe/restart helper logic.
4. Extract interaction helpers used by multiple dungeons.

### Phase 2: First family ports

1. Port `Rragar's Menagerie`.
2. Port `Catacombs of Kathandrax`.
3. Port `Frostmaw's Burrows`.

These three share the closest `RndTravel` and `MoveandAggroEx` structure and should validate the helper split quickly.

### Phase 3: Bundle-heavy ports

1. Port `Ravens Point`.
2. Port `Arachnis Haunt`.

These require extra bundle and brazier behavior beyond the common route engine.

### Phase 4: Froggy convergence

1. Move proven Froggy utility code into shared helpers.
2. Repoint Froggy to the helper libraries once the debugger owner is clear of active changes.
3. Remove duplicated route/recovery code from `FroggyHM.cpp`.

## Validation Strategy

- Offline unit tests for pure helper logic in `gwa3/tests`.
- Targeted injected tests only after helpers are stable.
- Single-lane live validation on MARVIN when a new dungeon bot is runnable.
- No cross-lane launches or shared-pipe testing.

## Task Breakdown

| ID | Task | Status | Notes |
|---|---|---|---|
| `DP-01` | Create shared `DungeonTravel` helper and offline tests | `done` | First extraction based on common `RndTravel` logic |
| `DP-02` | Define shared waypoint model and label taxonomy | `done` | `DungeonRoute` now covers waypoint shape, label kinds, nearest-index lookup, and generic restart/backtrack helpers |
| `DP-03` | Extract wipe/restart/checkpoint recovery helper | `done` | `DungeonCheckpoint` now provides shared checkpoint policy lookup, resolution, abort detection, and backtrack-start calculation |
| `DP-04` | Port `Rragar's Menagerie` to C++ using shared helpers | `in_progress` | Route data, blessing anchors, checkpoint retry policy, bundle/signpost helpers, Doomlore quest bootstrap, and first-pass runtime executor are now ported |
| `DP-05` | Port `Catacombs of Kathandrax` | `in_progress` | Typed route/reward data, shared quest-runtime helper, and a standalone runtime executor are now implemented and compile-validated in the isolated dungeon harness |
| `DP-06` | Port `Frostmaw's Burrows` | `in_progress` | Typed route/blessing/dialog/bootstrap constants plus a standalone runtime executor are now implemented; supplied AutoIt source still has no `Lvl5` route body, so runtime completion remains source-blocked |
| `DP-07` | Port `Ravens Point` | `in_progress` | Typed route data, torch/door/loot objectives, reward chest constants, selector aliases, and a standalone runtime executor are now compiled and offline-tested |
| `DP-08` | Port `Arachnis Haunt` | `in_progress` | Typed stage routes, staff objectives, spider-egg clusters, reward chest constants, selector aliases, and a standalone runtime executor are now compiled and offline-tested |

## Current Validation Snapshot

- `gwa3_dungeon_tests` now passes `89` offline tests on the MARVIN build tree.
- The shared helper layer now includes reusable repeated dialog loops, `DialogPlan`, `QuestCyclePlan`, `DungeonQuestRuntime`, and `DungeonBundle` helpers for two-phase quest flows and bundle/chest/signpost interactions.
- `DungeonQuest` is now exercised by `FrostmawsBurrows`, `Kathandrax`, `RavensPoint`, and `ArachnisHaunt`.
- `DungeonQuestRuntime` now owns shared quest bootstrap execution primitives: path-following, explicit zone-point resolution, zone-through-point transitions, and full bootstrap-plan execution.
- `BotModuleSelector` now recognizes `Kathandrax`, `FrostmawsBurrows`, `RavensPoint`, and `ArachnisHaunt` aliases for future runtime wiring.
- Standalone `KathandraxBot`, `FrostmawsBurrowsBot`, `RavensPointBot`, and `ArachnisHauntBot` executors now compile inside the isolated dungeon harness and set bot config correctly via offline registration tests.
- `gwa3_marvin.dll` builds cleanly again, and a shared `BotModuleRegistry` dispatcher now reduces the remaining `dllmain` integration step to a single call-site change.
- Froggy-shared extraction has expanded in the separate staging directory [froggy_shared](C:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/refactor_staging/froggy_shared), where `FroggySharedAdapters` now prove shared waypoint lookup, waypoint-behavior planning, dialog retry, chest gadget detection, signpost lookup, chest-open tracking, door-open interaction, checkpoint validation, blessing acquisition, reward hand-in, explicit bootstrap zone-point data, and a composed boss reward sequence.
- `FroggyHM.cpp` has begun live cutover on the MARVIN lane: waypoint lookup, checkpoint handling, blessing acquisition, chest/door interaction, reward hand-in, and the Sparkfly-to-Bogroot quest bootstrap path now consume staged/shared helpers instead of purely local logic.

## Current Blocker

- `Frostmaw's Burrows` still cannot be completed past the current level-4 stop because the supplied AutoIt source set does not include the `Lvl5` route body.
- Remaining Froggy convergence is no longer ownership-blocked, but still needs lane-appropriate live validation before duplicate helper bodies are removed wholesale.
