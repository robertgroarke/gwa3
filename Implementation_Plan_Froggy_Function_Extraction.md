# Implementation Plan: Extract General Functions from Froggy into Reusable Libraries

**Date:** 2026-03-25  
**Primary script:** `GWA Censured/Froggy_HM_v1.6.au3`  
**Objective:** move general-purpose helpers out of Froggy and into reusable library files so other scripts can share them without copying logic

---

## Summary

`Froggy_HM_v1.6.au3` currently mixes three different layers:

1. **Froggy-specific run logic**  
   Route setup, Bogroot waypoint data, dungeon interactions, boss/chest flow, GUI launch wiring.

2. **Reusable bot utilities**  
   Loot filters, chest handling, run stats, travel wrappers, inventory counters, death-penalty consumable usage.

3. **A large embedded combat/skill subsystem ported from `GWA_Logic_Censured_NEW.au3`**  
   Skillbar caching, target selection, casting rules, skill classification, effect access, and party targeting helpers.

The Froggy script has **113 functions**. A significant portion are general enough to live in libraries. The largest concentration of reusable code starts around the `#Region Logic_Port` block and is effectively a private library embedded inside the script.

The correct refactor is **not** to move everything blindly. Some functions are still route-specific and should remain in Froggy or be converted into callbacks/config for a generic runner.

---

## Audit Findings

### 1. Froggy contains a parallel utility stack

The current `lib/` directory already has generic behavior for travel, inventory, aggro movement, party status, looting, and combat primitives in:

- `GWA Censured/lib/GWA2.au3`
- `GWA Censured/lib/Utils.au3`
- `GWA Censured/lib/Utils-Maintenance.au3`
- `GWA Censured/lib/GUI_Functions.au3`

Froggy duplicates or partially reimplements some of that behavior with its own wrappers:

- `CountFreeSlots`
- `TravelTo`
- `ResignAndReturn`
- `GetPicksCount`
- `PickupLootEx`
- `CheckForChest`
- `GetNumberOfEnemies`

That duplication is a maintenance risk because fixes have to be made in two places.

### 2. The “Logic Port” block is the highest-value extraction target

The following Froggy functions clearly came from `GWA_Logic_Censured_NEW.au3` and are reusable across other bots:

- `CacheSkillBar`
- `Fight`
- `UseSkills`
- `UseSkillSmart`
- `CanCast`
- `CanUse`
- `GetBestTargetPtr`
- `GetBestTargetBySkillSlot`
- `GetLowestAlly`
- `GetNoHexEnemy`
- `GetMostBalledCastingEnemy`
- `MostCondsAllyPtr`
- `MostHexedAllyPtr`
- skill classification helpers such as `IsHealSkill`, `IsPressureSkill`, `IsRuptSkill`, `IsPrecastSkill`, etc.
- compatibility/effect helpers such as `ID`, `GetEffectsPtr`, `GetSkillbarPtr`, `GetAdrenaline`, `GetHP`

This block should become library code first. It is both large and the most reusable.

### 3. Some functions look generic but are still Froggy-coupled

Examples:

- `MoveandAggroEx`
- `WipeManagement`
- `CheckForChest`
- `CanPickUpEx`
- `UpdateStats`

These functions contain Froggy-specific assumptions:

- Bogroot waypoint labels like `"Dungeon Key"`, `"Dungeon Door"`, `"Boss"`, `"Lvl1 to Lvl2"`
- Froggy GUI stat side effects
- specific chest/open behavior
- specific pickup policy
- map-specific wipe restart rules

These should be split into:

- a **generic engine**
- a **Froggy policy/config layer**

### 4. We should avoid increasing edits inside upstream-derived files

Earlier refactor notes in this repo already point toward separating custom code from BotsHub-derived files. This extraction should follow that direction.

**Default rule:** new shared functionality should go into `GWA Censured/lib/custom/` rather than expanding `GWA2.au3` or `Utils.au3`, unless the function is a tiny wrapper that clearly belongs in the core library.

---

## Target Library Layout

Create a new custom library layer under:

```text
GWA Censured/lib/custom/
```

Proposed files:

### 1. `BotCore-Effects.au3`

Low-level read helpers and compatibility wrappers:

- `ID`
- `GetX`
- `GetY`
- `X`
- `Y`
- `GetHP`
- `GetHasEnchantment`
- `HasEffect`
- `GetSkillEffectPtr`
- `GetEffectsPtr`
- `GetSkillbarPtr`
- `GetAdrenaline`
- `GetSkillPtr` only if we want a wrapper layer separate from `GWA2.au3`

### 2. `BotCore-SkillRules.au3`

Skill typing and behavior classification:

- `IsSkillType`
- `IsWeaponRange`
- `IsWeaponSpell`
- `IsEnchantmentSkill`
- `IsAttackSkill`
- `IsShoutSkill`
- `IsEchoRefrainskill`
- `IsChantSkill`
- `IsHealSkill`
- `IsPartyHealSkill`
- `IsSelfPrehealSkill`
- `IsBondSkill`
- `IsCondRemoveSkill`
- `IsHexRemoveSkill`
- `IsHexAndConditionRemoveSkill`
- `IsEnchantRemoveSkill`
- `IsBindingSkill`
- `IsPressureSpiritSkill`
- `IsPressureSkill`
- `IsSpeedBoost`
- `IsSurvivalSkill`
- `IsHexSpell`
- `IsConditionSpell`
- `IsRuptSkill`
- `IsHardRuptSkill`
- `IsDisguiseskill`
- `IsPrecastSkill`
- `IsSelfHealSkill`
- `IsSelfOnlyHealSkill`
- `IsHealOtherSkill`
- `IsHealMySelfSkill`
- `IsHealAllySkill`
- `IsHexRemovalSkill`
- `IsConditionRemovalSkill`
- `IsConditionAndHexRemovalSkill`

### 3. `BotCore-Combat.au3`

Combat decision logic that depends on the two modules above:

- `CacheSkillBar`
- `GetBestTargetPtr`
- `GetBestMeleeTarget`
- `GetNearestSpiritPtrToAgent`
- `GetNearestMinionPtrToAgent`
- `GetNearestDeadAllyPtrToAgent`
- `UseSkillSmart`
- `Fight`
- `UseSkills`
- `CanCast`
- `CanAttack`
- `CanUse`
- `GetBestTargetBySkillSlot`
- `GetLowestAlly`
- `GetNoHexEnemy`
- `GetBalledEnchantedEnemy`
- `GetMostBalledCastingEnemy`
- `NeedEchoAlly`
- `MostCondsAllyPtr`
- `MostHexedAllyPtr`
- `GetNumberOfEnemies`
- `Wipe`
- `GetPartyHealth`
- `IsKnocked`
- `AgentHasEffect`

### 4. `BotCore-Loot.au3`

Pickup and chest behavior:

- `CheckForChest`
- `PickupLootEx`
- `CanPickUpEx`
- `GetPicksCount`
- `CountFreeSlots`

This file should be designed around **policy callbacks** so Froggy-specific stat updates do not stay embedded in shared code.

### 5. `BotCore-Travel.au3`

Reusable travel/recovery wrappers:

- `TravelTo`
- `ResignAndReturn`
- generic death-penalty consumable helper replacing `usedp`, `usedp9`, `Usedp1`-`Usedp5`

### 6. `BotCore-RunStats.au3`

Run timing/state tracking:

- `AvgRunTime`
- `BestRunTime`
- `UpdateStats`
- `CurrentRunTime`
- `TotalRunTime`

This should operate on a state struct/map instead of raw Froggy globals.

### 7. `BotCore-Waypoints.au3`

Generic waypoint helpers only:

- `GetNearestWaypointIndex`
- a new generic waypoint runner extracted from `MoveandAggroEx`
- a new generic travel-and-fight step extracted from `AggroMoveToEX`

`WipeManagement` should not move as-is. It should become a callback policy supplied by each script.

---

## What Stays in Froggy

These functions should remain in Froggy because they are route- or script-specific:

- `CloseEvent`
- `LaunchEvent`
- `onStart`
- `onStop`
- `onResume`
- `Setup`
- `RunToDungeon`
- `TakeQuest0`
- `BogrootLvl1`
- `BogrootLvl2`
- `GetDungeonKeyEx`
- `OpenDungeonDoor`
- `Boss`
- `ReverseToSparkflySwamp`

`LoadHeroConfigFromFile` is reusable, but it is close enough to setup/orchestration that it can either:

- stay in Froggy for phase 1, or
- later move to `lib/custom/BotCore-HeroSetup.au3`

### Functions that need redesign before extraction

These should not be moved verbatim:

- `MoveandAggroEx`
- `WipeManagement`
- `AggroMoveToEX`
- `CheckForChest`
- `CanPickUpEx`
- `UpdateStats`

Each mixes reusable flow with Froggy-specific rules.

---

## Function Migration Map

| Current function group | Target library | Extraction style |
|---|---|---|
| `ID`, `GetX`, `GetY`, `GetHP`, `GetEffectsPtr`, `GetSkillbarPtr`, `GetAdrenaline`, `HasEffect` | `BotCore-Effects.au3` | Move mostly as-is, normalize naming/comments |
| Skill classifiers and role checks | `BotCore-SkillRules.au3` | Move as-is first, clean up second |
| `CacheSkillBar`, `CanCast`, `CanUse`, `UseSkillSmart`, target selection helpers | `BotCore-Combat.au3` | Move together as one subsystem |
| `Wipe`, `GetPartyHealth`, ally targeting helpers | `BotCore-Combat.au3` | Move with combat subsystem |
| `CountFreeSlots`, `GetPicksCount` | `BotCore-Loot.au3` or `BotCore-Effects.au3` | Move early, low risk |
| `PickupLootEx`, `CheckForChest`, `CanPickUpEx` | `BotCore-Loot.au3` | Split engine from policy |
| `TravelTo`, `ResignAndReturn` | `BotCore-Travel.au3` | Move after low-level wrappers are stable |
| `usedp` family | `BotCore-Travel.au3` | Replace with data-driven consumable loop |
| `AvgRunTime`, `BestRunTime`, `CurrentRunTime`, `TotalRunTime`, `UpdateStats` | `BotCore-RunStats.au3` | Convert from globals to a state map |
| `GetNearestWaypointIndex` | `BotCore-Waypoints.au3` | Move directly |
| `AggroMoveToEX`, `MoveandAggroEx`, `WipeManagement` | `BotCore-Waypoints.au3` + Froggy callbacks | Refactor, not straight move |

---

## Refactor Principles

### 1. Extract by subsystem, not one function at a time

The combat functions are tightly coupled through globals:

- `$SkillBarCache`
- `$SkillbarSlot`
- `$BestTargetPtr`
- enum indices like `$all`, `$ptr`, `$energyreq`

These should move together into a single module with a clearly owned state object rather than being peeled out individually.

### 2. Replace Froggy globals with explicit state

Current shared logic depends on script globals such as:

- `$mBasePointer`
- `$Outpost`
- `$Language`
- `$BestTargetPtr`
- `$GUI_RunCounter`
- `$GUI_FailCounter`
- `$CumulatedTime`
- `$nCurrentRunTime`
- `$nBestRunTime`
- `$nTotalRunTime`
- `$OpenedChestAgentIDs`

Reusable libraries should accept:

- parameters,
- maps,
- small structs,
- or registered callback functions

instead of reading Froggy globals directly.

### 3. Split engine from policy

Good extraction candidates are generic loops. The Froggy-specific parts should become callbacks or config.

Examples:

- `PickupLootEx` should call a configurable `ShouldPickItem($item)` predicate
- chest handling should use a configurable `OnChestFound($agent)` callback
- waypoint navigation should use a map of labeled actions or callbacks instead of hardcoded labels inside the runner
- wipe recovery should use a `GetRestartWaypoint($mapId, $nearestWaypoint, $lastWaypoint)` callback

### 4. Keep custom shared code outside upstream-derived files

Unless a function is obviously foundational, prefer:

```text
GWA Censured/lib/custom/*.au3
```

over adding more custom code directly into:

- `GWA2.au3`
- `Utils.au3`

This preserves future BotsHub mergeability.

---

## Implementation Phases

## Phase 0: Create the custom library layer

**Goal:** establish the include structure without behavior changes.

Steps:

1. Create `GWA Censured/lib/custom/`
2. Add a single aggregator include, for example:
   - `GWA Censured/lib/Froggy_Shared.au3`
3. Have Froggy include the aggregator instead of directly including each new custom module
4. Keep Froggy behavior unchanged

**Deliverable:** library structure exists, no logic moved yet

---

## Phase 1: Extract low-risk helpers

**Goal:** remove the easiest generic functions first.

Move first:

- `CountFreeSlots`
- `GetPicksCount`
- `TravelTo`
- `ResignAndReturn`
- `usedp`, `usedp9`, `Usedp1`-`Usedp5`
- `ID`
- `GetX`
- `GetY`
- `X`
- `Y`
- `GetHP`
- `HasEffect`
- `GetSkillEffectPtr`
- `GetEffectsPtr`
- `GetSkillbarPtr`
- `GetAdrenaline`

Notes:

- Replace the `usedp` family with one data-driven function that loops over a configured list of DP-removal item model IDs.
- Keep the public function names initially for compatibility.

**Risk:** low

---

## Phase 2: Extract skill classification

**Goal:** move all skill typing logic into a reusable module.

Move:

- all `Is*Skill*` classification functions
- all condition/hex/heal/rupture classification helpers

Notes:

- This phase is mostly pure logic and should not require behavioral changes.
- Use it to remove hardcoded classification code from Froggy before moving combat behavior.

**Risk:** low-medium

---

## Phase 3: Extract the combat subsystem

**Goal:** move the `Logic_Port` combat engine into shared libraries.

Move together:

- `CacheSkillBar`
- `GetBestTargetPtr`
- `GetBestTargetBySkillSlot`
- `UseSkillSmart`
- `Fight`
- `UseSkills`
- `CanCast`
- `CanAttack`
- `CanUse`
- ally/enemy targeting helpers

Required cleanup:

1. Create a combat state container for:
   - `SkillBarCache`
   - `SkillbarSlot`
   - `BestTargetPtr`
   - `PressureSpiritSkills`
2. Remove direct dependence on Froggy globals where possible
3. Keep a thin compatibility layer in Froggy during migration

**Risk:** medium-high

This is the most important phase and should be completed before the waypoint engine extraction.

---

## Phase 4: Extract loot and chest handling

**Goal:** make pickup/chest logic reusable without keeping Froggy-specific stat side effects embedded.

Refactor before moving:

- `CanPickUpEx` becomes:
  - a generic predicate engine
  - plus a Froggy-specific rules/config layer
- `PickupLootEx` becomes generic pickup flow
- `CheckForChest` becomes generic chest detection/open flow with configurable open radius and post-open pickup behavior

Design requirement:

- item stats updates such as `GUI_SetGolds`, `GUI_SetTomes`, `GUI_SetBlackDyes`, `GUI_SetDroppedLockpicks`
  must move to callbacks or optional hooks

**Risk:** medium

---

## Phase 5: Extract waypoint/navigation engine

**Goal:** separate generic route traversal from Bogroot-specific actions.

Refactor `MoveandAggroEx` into:

1. a generic waypoint runner:
   - move to waypoint
   - detect wipe
   - detect stuck state
   - invoke action for special waypoint labels or IDs

2. a Froggy route policy:
   - label handlers for `"Dungeon Key"`, `"Dungeon Door"`, `"Boss"`, `"Lvl1 to Lvl2"`
   - wipe restart rules for Bogroot maps
   - route arrays

3. a generic step function extracted from `AggroMoveToEX`

`WipeManagement` should become a route callback rather than a shared utility.

**Risk:** high

This should happen only after combat and loot extraction are stable.

---

## Phase 6: Extract run statistics

**Goal:** move timing/stat tracking into a small reusable runtime module.

Move/refactor:

- `AvgRunTime`
- `BestRunTime`
- `UpdateStats`
- `CurrentRunTime`
- `TotalRunTime`

Design requirement:

- replace direct reliance on Froggy globals and GUI functions with:
  - a state map,
  - and UI callback hooks

This makes the same runtime module usable by other bots with different GUIs.

**Risk:** low-medium

---

## Expected End State

After the refactor:

- `Froggy_HM_v1.6.au3` should mostly contain:
  - startup wiring
  - Bogroot route data
  - Froggy-specific quest/dungeon/boss orchestration
  - minimal script-local config

- reusable combat/skill/effect/loot/travel helpers should live under `GWA Censured/lib/custom/`

- future scripts should be able to share the extracted modules without copying blocks out of Froggy or `GWA_Logic_Censured_NEW.au3`

---

## Testing Strategy

Each phase should be validated with:

1. `Au3Check` on the extracted library file
2. `Au3Check` on `GWA Censured/Froggy_HM_v1.6.au3`
3. smoke validation that Froggy still:
   - launches
   - loads hero config
   - enters Bogroot
   - fights
   - loots
   - recovers from wipe
4. focused harness tests where possible for:
   - skill classification
   - DP item selection
   - pickup predicate logic
   - waypoint restart selection

---

## Recommended Execution Order

If we implement this plan, the best order is:

1. Phase 0: create `lib/custom/` and shared include aggregator
2. Phase 1: low-risk helpers
3. Phase 2: skill classification
4. Phase 3: combat subsystem
5. Phase 4: loot/chest handling
6. Phase 5: waypoint/navigation engine
7. Phase 6: run stats

This order removes the biggest shared code from Froggy early while avoiding the most fragile route logic until the shared combat layer is stable.
