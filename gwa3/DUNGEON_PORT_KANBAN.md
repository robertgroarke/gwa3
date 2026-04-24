# Dungeon Port Kanban

| Status | Card | Scope | Owner | Notes |
|---|---|---|---|---|
| `Done` | `DP-01 Shared DungeonTravel helper` | Add common AutoIt-style random district selection helper plus offline tests | `MARVIN` | Landed as new shared module without touching Froggy runtime behavior |
| `Done` | `DP-02 Shared waypoint model` | Introduce reusable waypoint struct, labels, route helpers, and label taxonomy | `MARVIN` | Implemented as `DungeonRoute` with offline tests |
| `Done` | `DP-03 Shared wipe/restart recovery` | Extract checkpoint policy lookup and retry/abort helpers | `MARVIN` | `DungeonCheckpoint` now resolves pass/abort/backtrack decisions with offline tests |
| `In Progress` | `DP-04 Port Rragar's Menagerie` | First full port on top of shared travel/route/recovery helpers | `MARVIN` | Runtime now covers checkpoint handling, shared NPC/dialog bootstrap, and Doomlore-to-Dalada startup flow |
| `In Progress` | `DP-05 Port Catacombs of Kathandrax` | Reuse Rragar-style checkpoint and key handling | `MARVIN` | Typed route/reward data, shared quest runtime, and a standalone executor are now compile-validated in the isolated dungeon harness |
| `In Progress` | `DP-06 Port Frostmaw's Burrows` | Reuse route/recovery helpers and validate family generality | `MARVIN` | Typed route/blessing/dialog/bootstrap module plus a standalone executor are in place; current source is still missing the `Lvl5` route body |
| `In Progress` | `DP-07 Port Ravens Point` | Add torch/chest/brazier interactions | `MARVIN` | Typed route data, torch/door/loot objectives, reward chest constants, shared quest-cycle data, and a standalone executor are in place with offline tests |
| `In Progress` | `DP-08 Port Arachnis Haunt` | Add staff/brazier/spider-egg interactions | `MARVIN` | Typed stage routes, staff objectives, spider-egg clusters, reward chest constants, shared quest-cycle data, and a standalone executor are in place with offline tests |
| `In Progress` | `DP-09 Froggy convergence refactor` | Move shared Froggy utilities out of `FroggyHM.cpp` and repoint Froggy | `MARVIN` | live cutover has started: waypoint lookup, checkpoint handling, blessing acquisition, chest/door interaction, reward hand-in, and Sparkfly bootstrap path now consume staged/shared helpers; remaining work is duplicate removal plus live validation |
| `Done` | `DP-10 Clean MARVIN full-build integration` | Validate `dllmain` module wiring and runtime stubs on a full `gwa3_marvin.dll` build | `MARVIN` | `gwa3_marvin.dll` builds cleanly again on the MARVIN preset after the shared module registry and Froggy refactor wiring |
