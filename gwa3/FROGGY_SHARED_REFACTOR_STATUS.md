# Froggy Shared Refactor Status

## Goal

Reduce `FroggyHM.cpp` to Froggy-specific route/state-machine behavior and move reusable dungeon logic into shared `gwa3::bot` helper modules that other dungeon bots can call directly.

Primary target outcome:

- shared movement/navigation helpers live outside Froggy
- shared combat/skill helpers live outside Froggy
- shared loot/item/vendor helpers live outside Froggy
- shared quest/dialog/checkpoint/chest/door helpers live outside Froggy
- `FroggyHM.cpp` becomes a thin consumer of those helpers instead of the source of common dungeon behavior

## Current Shared Helper Surface

The following shared or staged-shared modules now exist and are already part of the refactor path.

### Core dungeon helpers

- `include/gwa3/bot/DungeonTravel.h` / `src/bot/DungeonTravel.cpp`
  - district/random travel plan selection
- `include/gwa3/bot/DungeonRoute.h` / `src/bot/DungeonRoute.cpp`
  - waypoint model
  - waypoint label taxonomy
  - nearest-waypoint lookup
  - label classification
- `include/gwa3/bot/DungeonCheckpoint.h` / `src/bot/DungeonCheckpoint.cpp`
  - checkpoint retry/backtrack/abort policy
- `include/gwa3/bot/DungeonDialog.h` / `src/bot/DungeonDialog.cpp`
  - dialog retry and dialog sequence helpers
- `include/gwa3/bot/DungeonInteractions.h` / `src/bot/DungeonInteractions.cpp`
  - nearest NPC lookup
  - signpost lookup
  - chest/signpost utility helpers
- `include/gwa3/bot/DungeonBundle.h` / `src/bot/DungeonBundle.cpp`
  - chest, signpost, and bundle interaction helpers
- `include/gwa3/bot/DungeonQuest.h` / `src/bot/DungeonQuest.cpp`
  - quest/bootstrap plan data model
- `include/gwa3/bot/DungeonQuestRuntime.h` / `src/bot/DungeonQuestRuntime.cpp`
  - reusable quest/bootstrap execution helpers

### Shared navigation/combat/skill helpers

- `include/gwa3/bot/DungeonNavigation.h` / `src/bot/DungeonNavigation.cpp`
  - shared move/reissue/stuck-monitor primitives
  - callback-based `MoveToAndWait(...)`
  - agent-targeted movement helpers
- `include/gwa3/bot/DungeonCombat.h` / `src/bot/DungeonCombat.cpp`
  - nearest enemy lookup
  - enemy counting
  - hero flag/unflag helpers
  - generic enemy clear / aggro advance scaffolding
- `include/gwa3/bot/DungeonCombatRoutine.h` / `src/bot/DungeonCombatRoutine.cpp`
  - shared combat-step execution path
  - shared skill-candidate inspection used by builtin combat routing
- `include/gwa3/bot/DungeonSkill.h` / `src/bot/DungeonSkill.cpp`
  - shared skill cache/classification
  - shared target resolution
  - common cast gating / `CanUseSkill(...)` support

### Shared loot/inventory/vendor helpers

- `include/gwa3/bot/DungeonLoot.h` / `src/bot/DungeonLoot.cpp`
  - shared loot pickup logic
- `include/gwa3/bot/DungeonInventory.h` / `src/bot/DungeonInventory.cpp`
  - inventory counting and lookup helpers
- `include/gwa3/bot/DungeonItemActions.h` / `src/bot/DungeonItemActions.cpp`
  - identify, salvage, sell, deposit, and item-use action loops
- `include/gwa3/bot/DungeonItemPolicy.h` / `src/bot/DungeonItemPolicy.cpp`
  - sell/store/salvage policy decisions
- `include/gwa3/bot/DungeonVendor.h` / `src/bot/DungeonVendor.cpp`
  - merchant and Xunlai vendor/storage orchestration

### Froggy staging layer

- `refactor_staging/froggy_shared/FroggySharedAdapters.h`
- `refactor_staging/froggy_shared/FroggySharedAdapters.cpp`

This staging layer currently provides Froggy-compatible wrappers or planners for:

- nearest waypoint lookup
- dialog retry
- chest gadget detection
- nearest signpost lookup
- entry/bootstrap plan access
- blessing detection/acquisition
- reward claim flow
- boss reward sequence
- waypoint behavior resolution
- wipe restart / stuck backtrack computation
- checkpoint progress and retry resolution
- chest opening
- door opening
- generic waypoint traversal helpers

## What Has Already Been Cut Over From Froggy

The refactor has already moved substantial reusable behavior out of `src/bot/FroggyHM.cpp`.

Confirmed shared extraction areas:

- travel/district selection behavior
- waypoint model and label classification
- checkpoint retry and abort logic
- dialog retry and repeated dialog sequences
- nearest NPC/signpost/chest utility behavior
- reward hand-in and boss reward sequencing
- generic waypoint travel and reverse traversal helpers
- generic move/reissue/stuck-monitor navigation support
- generic enemy-clear / aggro advance scaffolding
- shared skill cache / target resolution / cast gating support
- shared loot, inventory, item-action, item-policy, merchant, and Xunlai flows

The isolated dungeon suite was built specifically to support this refactor and currently covers these modules outside Froggy directly.

## What Is Still Froggy-Local

`src/bot/FroggyHM.cpp` still contains a large amount of code and is not yet reduced to a thin consumer.

Major remaining Froggy-local areas:

### Froggy-specific runtime/state

- Froggy run counters and timers
- Froggy logging / debug trace state
- Froggy state-machine sequencing for the end-to-end run
- Froggy route arrays and map/dialog constants for Sparkfly and Bogroot

### Combat and skill policy still living in Froggy

The generic helpers exist, but Froggy still retains a large combat-policy surface, including:

- skill-role constants and role assignment conventions
- Froggy combat debug tracing and builtin decision dumps
- skill-id classification helpers such as:
  - hard interrupt
  - condition removal
  - hex removal
  - survival
  - speed boost
  - bindings / pressure spirits / precast
- Froggy combat priority pipeline, centered around:
  - `TryUseSkillWithRole(...)`
  - `UseAllSkillsWithRole(...)`
  - `FightTarget(...)`

This is the biggest remaining shared-combat extraction opportunity.

### Remaining navigation/orchestration wrappers

Even with shared navigation available, Froggy still holds live orchestration wrappers and route-driving logic such as:

- high-level aggro movement orchestration
- route-specific movement sequencing
- state-dependent move/clear decisions

Some of this should remain Froggy-specific, but the generic pieces are not fully separated yet.

### Remaining item/maintenance logic

Shared inventory/vendor/item layers exist, but Froggy still contains higher-level maintenance orchestration around:

- blessing / conset maintenance decisions
- maintenance sequencing
- crafting/stocking triggers
- Froggy-specific upkeep logic around vendor and storage paths

### Hero/template management

Froggy still owns:

- base64 skill-template decoding helpers
- hero config file loading
- hero-template runtime wiring

Some of that may stay Froggy-local if it is not broadly reusable, but it has not been deliberately partitioned yet.

## Current Refactor Boundary

The project is now in a mixed state:

- common primitives exist in shared modules
- Froggy uses some of them
- Froggy still duplicates or wraps a meaningful amount of higher-level combat, upkeep, and run-orchestration logic

In practice that means:

- the reusable lower layers are mostly in place
- the highest-value remaining work is not creating more primitives
- the highest-value remaining work is deleting duplicate policy/wrapper code from Froggy by cutting its remaining callers over to shared layers

## Work Remaining

### 1. Finish the shared combat-policy extraction

Move the remaining reusable combat policy above `DungeonCombatRoutine` into a dedicated shared layer.

Likely scope:

- role-based skill selection
- shared combat priority pipeline
- shared target preference helpers
- shared cast retry/aftercast handling
- shared spirit/precast behavior where it is not Froggy-specific

Expected result:

- `FightTarget(...)` becomes thin or disappears from Froggy
- the other dungeon bots gain a reusable combat-policy layer instead of depending on Froggy examples

### 2. Complete the live cutover from Froggy-local wrappers

Audit every remaining local helper in `FroggyHM.cpp` and decide one of:

- move fully to shared module
- keep intentionally Froggy-specific
- delete dead compatibility wrapper

Highest-priority candidates:

- remaining combat/skill wrappers
- remaining movement/orchestration wrappers
- remaining blessing/conset maintenance helpers
- remaining merchant/Xunlai orchestration wrappers

### 3. Remove duplicate local bodies after validation

Once each slice is verified, remove the local Froggy copy instead of leaving pass-through wrappers behind.

Success condition:

- fewer static helper bodies in `FroggyHM.cpp`
- clearer ownership of shared behavior in `Dungeon*` modules

### 4. Deliberately classify what should remain Froggy-specific

Not everything should move out.

The following likely remain in Froggy:

- Froggy route definitions
- Froggy quest/run sequence
- Froggy-specific logging and trace views
- dungeon-specific encounter handling that is not generic enough for reuse

This classification pass still needs to be made explicit.

### 5. Keep live validation aligned with refactor slices

The shared extraction work has already intersected with live Sparkfly movement/casting debugging. That means every new cutover needs two forms of validation:

- isolated dungeon test validation
- live Froggy validation on MARVIN

The live issues around Sparkfly command/cast presentation are separate from the structural refactor, but they can mask regressions if validation is sloppy.

## Suggested Next Extraction Order

1. Shared combat-policy layer
   - move remaining role-driven skill execution and priority logic out of Froggy
2. Froggy maintenance/upkeep layer split
   - separate reusable vendor/storage/consumable orchestration from Froggy-only sequencing
3. Remove duplicate local wrappers
   - delete helpers once all callers use shared code
4. Explicitly mark Froggy-only code
   - leave route/state-machine logic in Froggy on purpose

## Validation Status

The refactor is supported by the isolated dungeon test target and shared-module tests. The shared helper surface is no longer hypothetical; it is already compiled and tested outside Froggy.

Current practical state:

- shared dungeon helper modules exist and are used
- Froggy is partially converged onto them
- Froggy is not yet fully reduced
- the remaining work is mostly higher-level policy extraction and duplicate removal

## Bottom Line

The refactor is materially underway and has passed the “create shared primitives” stage.

What remains is the harder but more valuable half:

- move the remaining combat-policy and upkeep orchestration out of Froggy
- finish cutover of live callers
- delete the duplicate Froggy-local helpers
- leave only truly Froggy-specific route/state logic in `FroggyHM.cpp`
