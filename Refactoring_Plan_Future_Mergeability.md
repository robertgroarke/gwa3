# Refactoring Plan: Future BotsHub Mergeability

**Goal:** Restructure the GWA Censured codebase so that pulling new BotsHub updates is a straightforward file-replace operation rather than a complex merge.

**Core Principle:** _Separate what's ours from what's theirs._ If BotsHub files can be dropped in without touching our custom code, merges become trivial.

---

## The Problem Today

Your custom code is **woven into** BotsHub files at ~30 insertion points:

| Category | Count | Example |
|----------|-------|---------|
| Crafting ASM embedded in GWA2.au3 | 6 blocks | CommandCraftItemEx2, TradeID data slots, CreateCommands additions |
| Custom functions in GWA2.au3 | ~15 funcs | CraftItem, IsPlayerDead, GetIsMerchantOpen, UseSkillEx, etc. |
| Circular dep workarounds | ~10 items | _GWA2_FillArray, logging stubs, removed #includes |
| Convention conflicts | 3 critical | GetAgentArray indexing, MemoryRead signatures, function renames |
| Custom functions in Utils.au3 | ~8 funcs | SalvageAllItems overhaul, IsRareSkin, ShouldBlacklist, etc. |
| NPC coordinates in Utils-Storage-Bot.au3 | ~15 entries | Gadd's, Embark Beach traders, Xunlai chests |

When BotsHub releases an update, you currently must hand-merge every one of these touchpoints. One missed merge or one changed line number breaks everything.

---

## The Target State

```
GWA Censured/lib/
  ├── botshub/                    # BotsHub files — DROP-IN REPLACEABLE
  │   ├── GWA2.au3                # Upstream, unmodified
  │   ├── GWA2_Assembly.au3       # Upstream, unmodified
  │   ├── GWA2_ID.au3             # Upstream, unmodified
  │   ├── GWA2_ID_Items.au3       # Upstream, unmodified
  │   ├── GWA2_ID_Maps.au3        # Upstream, unmodified
  │   ├── GWA2_ID_Quests.au3      # Upstream, unmodified
  │   ├── GWA2_ID_Skills.au3      # Upstream, unmodified
  │   ├── Utils.au3               # Upstream, unmodified
  │   ├── Utils-Agents.au3        # Upstream, unmodified
  │   ├── Utils-Storage.au3       # Upstream, unmodified
  │   ├── Utils-Debugger.au3      # Upstream, unmodified
  │   ├── Utils-Items_Modstructs.au3  # Upstream, unmodified
  │   └── JSON.au3                # Upstream, unmodified
  │
  ├── custom/                     # OUR code — never touched by BotsHub updates
  │   ├── GWA2_Crafting.au3       # Crafting system (CraftItem, CraftItemSafe, ASM injection hooks)
  │   ├── GWA2_Extensions.au3     # Custom GWA2 functions (IsPlayerDead, UseSkillEx, etc.)
  │   ├── Utils-Maintenance.au3   # Maintenance automation (already separate)
  │   ├── Utils-Salvage.au3       # Salvage logic (already separate)
  │   ├── NPC_Coordinates.au3     # Custom NPC locations (Gadd's, Embark Beach, etc.)
  │   ├── GUI_Functions.au3       # Froggy GUI (already separate)
  │   ├── UsefulMods.au3          # Mod tables (already separate)
  │   ├── RareSkins.au3           # Rare skin table (already separate)
  │   └── Skill_Types.au3         # Skill type constants (already separate)
  │
  └── Froggy_Includes.au3         # Master include file that wires everything together
```

**Updating BotsHub becomes:** Delete `botshub/`, copy in new files from upstream, done.

---

## Which COA Best Supports This?

| COA | Future Merge Ease | Why |
|-----|-------------------|-----|
| **COA 1** (current) | Poor | Custom code still mixed into BotsHub files |
| **COA 2** (full rebase) | Good, but fragile | Achieves alignment, but doesn't prevent re-entangling over time |
| **COA 3** (hybrid) | **Best** | Adopts upstream architecture AND establishes the separation boundary |
| **COA 4** (parallel) | Poor | Two overlapping engines, worst of both worlds |

**COA 3 is the clear winner**, but with a specific twist: we explicitly create the `botshub/` vs `custom/` separation during the migration, ensuring the boundary is maintained going forward.

---

## Refactoring Phases

### Phase 0: Establish the Separation (No Functional Changes)

**Goal:** Create the directory structure and master include file without changing any logic.

1. Create `GWA Censured/lib/botshub/` and `GWA Censured/lib/custom/`
2. Move existing custom-only files into `custom/`:
   - `Utils-Maintenance.au3`, `Utils-Salvage.au3`, `GUI_Functions.au3`
   - `UsefulMods.au3`, `RareSkins.au3`, `Skill_Types.au3`
3. Create `Froggy_Includes.au3` master include that pulls in everything in the right order
4. Update `Froggy_HM_v1.6.au3` to use single `#include 'lib/Froggy_Includes.au3'`
5. Run Au3Check — must match baseline exactly

**Effort:** ~1-2 hours
**Risk:** Very low — just moving files and updating paths

---

### Phase 1: Extract Custom Functions from GWA2.au3

**Goal:** Move all user-added functions out of GWA2.au3 into `custom/GWA2_Extensions.au3`.

**Functions to extract (CAN be separated — no internal GWA2 state needed):**

| Function | Lines | Notes |
|----------|-------|-------|
| `IsPlayerDead()` | 2323-2326 | Pure agent query |
| `IsHeroDead()` | 2329-2340 | Pure agent query |
| `GetCastTimeModifier()` | 2348-2389 | Pure calculation |
| `UseSkillEx()` | 2403-2423 | Wrapper around UseSkill |
| `UseSkillTimed()` | 2426-2452 | Wrapper around UseSkill |
| `UseHeroSkillEx()` | 2466-2486 | Wrapper around UseHeroSkill |
| `UseHeroSkillTimed()` | 2489-2513 | Wrapper around UseHeroSkill |
| `GetIsMerchantOpen()` | 1362-1364 | Pure query |
| `GetItemIDFromModelID()` | 1482-1490 | Pure query |
| `GetMerchantItemPtrByModelId()` | 1494-1509 | Pure query |
| `ClearAttributes()` | 2672-2691 | Command wrapper |
| `Disconnected()` | 6720-6766 | State check |
| `_GWA2_GetAlmostInRangeOfAgent()` | 6929-6938 | Movement helper |
| `_GWA2_GetInventoryItemPtrByModelId()` | 6940-6960 | Inventory search |
| `_GWA2_CountItemInBagsByModelID()` | 6962-6977 | Inventory count |

Create `custom/GWA2_Extensions.au3`:
```autoit
#include-once
#include '../botshub/GWA2.au3'
; All custom GWA2 extensions go here
```

**Effort:** ~2-3 hours
**Risk:** Low — moving functions between files, updating callers
**Test:** Au3Check on every file, dry-load test

---

### Phase 2: Extract Crafting System

**Goal:** Isolate the crafting system into `custom/GWA2_Crafting.au3`.

This is the hardest extraction because crafting code is embedded in 3 upstream functions:
1. `CreateData()` — adds `TradeID/4` and `TradeID_Value/4` memory slots
2. `CreateCommands()` — adds `CommandCraftItemEx2`, `CommandRequestCraftQuote`, `CommandCraftExecute` ASM procedures
3. `InitializeGameClientData()` — adds TradeID verification and address caching

**Strategy: Hook Pattern**

Instead of modifying upstream functions, use BotsHub's existing extension mechanism. The upstream code already has:
```autoit
If IsDeclared('g_b_Write') Then Extend_Write()
If IsDeclared('g_b_AssemblerWriteDetour') Then Extend_AssemblerWriteDetour()
```

We expand this pattern. Create hook points that the crafting module registers:

```autoit
; custom/GWA2_Crafting.au3
#include-once
Global $g_b_CraftingExtension = True

; Called from CreateData() hook
Func Extend_CreateData()
    _('TradeID/4')
    _('TradeID_Value/4')
EndFunc

; Called from CreateCommands() hook
Func Extend_CreateCommands()
    _('CommandCraftItemEx2:')
    ; ... ASM code ...
    _('CommandRequestCraftQuote:')
    ; ... ASM code ...
    _('CommandCraftExecute:')
    ; ... ASM code ...
EndFunc

; Called from InitializeGameClientData() hook
Func Extend_InitializeGameClientData()
    Global $trade_id_addr = GetValue('TradeID')
    Global $trade_id_value_addr = GetValue('TradeID_Value')
EndFunc

; The CraftItem function
Func CraftItem($modelID, $amount, $gold, ...)
    ; ... full crafting logic ...
EndFunc
```

**The upstream GWA2.au3 needs exactly 3 one-line additions** (hook calls):
```autoit
; In CreateData(), after the last data slot:
If IsDeclared('g_b_CraftingExtension') Then Extend_CreateData()

; In CreateCommands(), after the last command:
If IsDeclared('g_b_CraftingExtension') Then Extend_CreateCommands()

; In InitializeGameClientData(), after ModifyMemory():
If IsDeclared('g_b_CraftingExtension') Then Extend_InitializeGameClientData()
```

These 3 lines are the ONLY modifications to upstream GWA2.au3. They're trivial to re-add after any upstream update, and they're safe no-ops if the crafting module isn't loaded (the `IsDeclared` check prevents errors).

**Effort:** ~4-6 hours (most complex phase)
**Risk:** Medium — ASM injection ordering matters, must test thoroughly
**Test:** Full dry-load + Keystone validation of crafting ASM opcodes

---

### Phase 3: Resolve Convention Conflicts

**Goal:** Align with upstream conventions so their code can run unmodified.

#### 3a. Restore `#include 'Utils.au3'` in GWA2.au3

Upstream keeps this include. Restoring it eliminates:
- `_GWA2_FillArray()` workaround
- Logging stubs in Utils-Debugger.au3
- The entire category C of modifications

If circular dependency was the concern: AutoIt's `#include-once` directive prevents double-inclusion. As long as both files have `#include-once` at the top (which they do), the circular include is safe. Upstream relies on this.

**Action:** Re-add `#include 'Utils.au3'` to GWA2.au3, delete `_GWA2_FillArray` and other workarounds, verify no actual circular dependency crash occurs.

#### 3b. Adopt Upstream's GetAgentArray (0-indexed)

Convert to 0-indexed and update all callers:

```autoit
; Find all: For $i = 1 To $agents[0]
; Replace:  For $i = 0 To UBound($agents) - 1

; Or even better, adopt For...In style:
; For $agent In $agents
```

Run a codebase-wide search for `$agents[0]`, `$array[0]` patterns that reference the count header, and convert each one.

**Affected files:** Utils.au3, Utils-Maintenance.au3, Froggy_HM_v1.6.au3, GWA_Logic_Censured_NEW.au3

#### 3c. MemoryRead Signature Decision

This is the biggest mechanical change. Two options:

**Option A: Keep your signature, add a wrapper**
```autoit
; In custom/GWA2_Compat.au3:
; Wrapper that matches your calling convention but calls upstream's
Func MemoryReadCompat($address, $type = 'dword')
    Return MemoryRead(GetProcessHandle(), $address, $type)
EndFunc
```
Then rename all your calls from `MemoryRead()` to `MemoryReadCompat()`. Upstream's `MemoryRead()` stays unmodified.

**Option B: Adopt upstream's signature**
Change all ~200+ call sites to pass `$processHandle` as first argument. This is a big mechanical change but results in zero wrapper overhead and perfect upstream compatibility.

**Recommendation:** Option A for now (faster), Option B long-term. The wrapper has negligible performance impact for a bot.

**Effort:** ~4-6 hours for all Phase 3
**Risk:** Medium — the GetAgentArray change ripples through many files
**Test:** Au3Check on all files, grep for remaining `$agents[0]` patterns

---

### Phase 4: Adopt Upstream File Splitting

**Goal:** Replace your monolithic GWA2.au3 + GWA2_ID.au3 with upstream's split files.

1. Copy upstream's `GWA2.au3` (2815 lines) into `botshub/`
2. Copy upstream's `GWA2_Assembly.au3` into `botshub/`
3. Copy upstream's `GWA2_ID.au3` + 4 sub-files into `botshub/`
4. Copy upstream's `Utils.au3`, `Utils-Agents.au3`, `Utils-Storage.au3` into `botshub/`
5. Add the 3 crafting hook lines to `botshub/GWA2.au3` (from Phase 2)
6. Delete your old monolithic files
7. Delete `Map_IDs.au3`, `Skill_IDs.au3` (now redundant with upstream's comprehensive versions)
8. Update all `#include` paths

**Effort:** ~3-4 hours
**Risk:** Medium — many include paths change, some function names may differ
**Test:** Au3Check on every file, full dry-load, Keystone regression

---

### Phase 5: Adopt Custom NPC Coordinates Cleanly

**Goal:** Extract your NPC coordinate additions from Utils-Storage-Bot.au3 into a separate data file.

Create `custom/NPC_Coordinates.au3`:
```autoit
#include-once
; Custom NPC coordinates not in upstream BotsHub
; Gadd's Encampment, Embark Beach traders, Xunlai chests, etc.

Func GetCustomNPCCoordinates($townID, $npcType, $name = '')
    ; ... your coordinate lookup logic ...
EndFunc
```

Then the upstream `Utils-Storage.au3` stays unmodified. Your maintenance code calls `GetCustomNPCCoordinates()` instead of the modified `NPCCoordinatesInTown()`.

**Effort:** ~1-2 hours
**Risk:** Low
**Test:** Au3Check, verify maintenance town navigation logic

---

## Phase Summary

| Phase | Description | Effort | Risk | Prereqs |
|-------|-------------|--------|------|---------|
| **0** | Directory structure + master include | 1-2h | Very Low | None |
| **1** | Extract custom functions from GWA2.au3 | 2-3h | Low | Phase 0 |
| **2** | Extract crafting system with hook pattern | 4-6h | Medium | Phase 1 |
| **3** | Resolve convention conflicts | 4-6h | Medium | Phase 0 |
| **4** | Adopt upstream file splitting | 3-4h | Medium | Phases 1-3 |
| **5** | Extract NPC coordinates | 1-2h | Low | Phase 4 |
| **Total** | | **15-23h** | | |

---

## Testing Strategy Per Phase

Each phase follows the same pattern from the COA 1 test plan:

1. **Before:** Record Au3Check baseline for all affected files
2. **During:** Make changes incrementally, Au3Check after each file
3. **After:** Full Au3Check on Froggy, Keystone regression, dry-load test
4. **Commit:** One commit per phase, independently revertible

---

## After Refactoring: How a Future BotsHub Update Works

1. Clone latest BotsHub
2. Copy all files into `lib/botshub/`, replacing existing
3. Re-add 3 crafting hook lines to `botshub/GWA2.au3` (documented, 3 lines total)
4. Run Au3Check — if clean, done
5. If Au3Check shows new errors:
   - Check if upstream renamed/removed a function your custom code calls
   - Update `custom/` files to match — YOUR code, YOUR responsibility, clear scope

**Estimated time for future merges: 30 minutes to 2 hours** (vs 15-25 hours today).

---

## What NOT to Refactor

These are fine as-is and shouldn't be changed:

- **Utils-Maintenance.au3** — already separate, no upstream counterpart
- **Utils-Salvage.au3** — already separate
- **GUI_Functions.au3** — already separate, intentionally different from BotsHub's GUI
- **UsefulMods.au3**, **RareSkins.au3** — pure data files, already separate
- **Froggy_HM_v1.6.au3** — top-level script, only needs include path updates

---

## Recommended Execution Order

```
COA 1 (done) → Phase 0 → Phase 1 → Phase 3a → Phase 3b → Phase 2 → Phase 4 → Phase 5 → Phase 3c
```

Phase 3a (restore #include) should come before Phase 2 (crafting extraction) because the crafting code currently uses `_GWA2_FillArray` which would be unnecessary after restoring the include.

Phase 3c (MemoryRead signatures) is last because it's the most mechanical and can be done with search-and-replace tooling.
