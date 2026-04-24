# Froggy Shared Extraction Staging

This directory is a MARVIN-owned staging area for the Froggy-to-shared-helper refactor.

It exists specifically to avoid editing [FroggyHM.cpp](C:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/src/bot/FroggyHM.cpp) while that file is still inside another active work area.

## Goal

Reduce the eventual Froggy refactor to a narrow swap from file-local helpers to shared adapters by proving each shared slice in isolation first.

## Current Staging Slice

The staging layer now covers Froggy helpers that already have strong equivalents in the shared dungeon modules:

- `GetNearestWaypointIndex` -> `DungeonRoute::FindNearestWaypointIndex`
- `SendDialogWithRetry` -> `DungeonDialog::SendDialogWithRetry`
- `IsChestGadgetId` -> `DungeonInteractions::IsChestGadgetId`
- `FindNearestSignpost` -> `DungeonInteractions::FindNearestSignpost`
- `OpenNearbyChest` chest-tracker behavior -> `DungeonInteractions::OpenedChestTracker` + `DungeonBundle::OpenChestAndPickUpBundle`
- `OpenDungeonDoorAt` signpost-repeat behavior -> `DungeonBundle::InteractSignpostNearPoint`
- Froggy checkpoint restart/backtrack and door-checkpoint validation -> `DungeonRoute` + `DungeonCheckpoint`
- Froggy quest bootstrap constants -> `DungeonQuest`
- Froggy blessing detection and shrine interaction -> shared manager-facing adapter built on `DungeonInteractions`, `DungeonDialog`, `EffectMgr`, and `PlayerMgr`
- Froggy reward NPC hand-in and fallback dialog path -> `DungeonQuestRuntime`
- Froggy boss special-case chest/reward block -> composed staged boss-sequence helper
- Froggy waypoint-label behavior mapping -> staged waypoint execution planner aligned with `DungeonRoute::ClassifyWaypointLabel`

## Why This Staging Layer Exists

- It gives Froggy extraction code a separate compile surface.
- It allows tests to lock down semantics before touching live Froggy code.
- It narrows the future `FroggyHM.cpp` edit to replacing local helper bodies or call sites with staged shared adapters.

## Remaining Slices

1. Route-loop cutover preparation
   - Keep narrowing Froggy-only branching by staging any remaining local helper clusters that still sit directly inside `FollowWaypoints`.
2. Live cutover
   - Ownership is clear and live cutover has started. Continue replacing matching local helper bodies or call sites in `FroggyHM.cpp` with the staged/shared equivalents, then validate each slice on the MARVIN lane.
3. Duplicate removal
   - Delete Froggy-local helper bodies only after the staged/shared replacements have passed lane-appropriate validation in the live file.
