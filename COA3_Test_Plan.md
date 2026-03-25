# COA 3 Incremental Test Plan

**Objective:** Validate each refactoring phase to ensure the Froggy script and maintenance system remain functional throughout the separation of custom code from BotsHub library code.

---

## Testing Toolchain

| Tool | Purpose |
|------|---------|
| **Au3Check.exe** | Syntax/include validation — catches undefined functions, bad arg counts, missing includes |
| **AutoIt3.exe** | Runtime execution — dry-load tests, function existence verification |
| **Python 3.13 + Keystone** | x86 opcode cross-validation (for Phase 2 crafting ASM) |
| **grep/Grep tool** | Dependency audits — verify no dangling function references |

---

## Baselines

### Au3Check Error Counts (Pre-Refactoring)

These error counts must NOT increase during any phase. Decreases are acceptable (and expected as we clean up the architecture).

| File | Baseline Errors | Notes |
|------|----------------|-------|
| **Froggy_HM_v1.6.au3** | **202** | Full include chain, many from transitive deps |
| **GWA2.au3** | **3** | DllStructSetData args (2), Out() undefined (1) — all in Utils-Debugger |
| **Utils.au3** | **12** | Undeclared GUI globals, runtime-resolved functions |
| **Utils-Debugger.au3** | **7** | Missing $kernel_handle, $MEMORY_INFO_STRUCT_TEMPLATE, Out() |
| **GUI_Functions.au3** | **8** | Runtime-resolved globals |
| **Utils-Maintenance.au3** | **201** | Cascaded from deep dependency chain |
| **Utils-Storage-Bot.au3** | **201** | Cascaded from deep dependency chain |
| **Utils-Salvage.au3** | **5** | Minor |
| **Utils-Items_Modstructs.au3** | **23** | Cascaded |
| **GWA2_ID.au3** | **0** | Clean |
| **GWA2_Headers.au3** | **0** | Clean |
| **All data files** | **0** | Map_IDs, Skill_IDs, Skill_Types, RareSkins, UsefulMods, JSON |

### Include Chain (Pre-Refactoring)

```
Froggy_HM_v1.6.au3
├── lib/GWA2_Headers.au3
├── lib/GWA2.au3
│   ├── GWA2_Headers.au3 (once)
│   ├── GWA2_ID.au3 → <Array.au3>
│   ├── Utils-Debugger.au3 → <FileConstants.au3>
│   └── <Math.au3>
├── lib/Utils.au3
│   ├── <array.au3>, RareSkins.au3, <GUIConstantsEx.au3>
│   ├── GWA2_Headers.au3, GWA2_ID.au3, GWA2.au3 (all once)
│   ├── Utils-Debugger.au3 (once)
│   └── Utils-Salvage.au3 → GWA2.au3(once), UsefulMods.au3, RareSkins.au3(once)
├── lib/JSON.au3
├── lib/SQLite.au3, SQLite.dll.au3
├── lib/Map_IDs.au3
├── lib/Skill_IDs.au3
├── lib/Skill_Types.au3
├── lib/Utils-Debugger.au3 (once)
├── lib/GUI_Functions.au3 → (AutoIt GUI libs)
├── lib/Utils-Maintenance.au3
│   ├── Utils-Storage-Bot.au3 → (SQLite, GWA2, Utils, Items_Modstructs, Debugger)
│   └── Utils-Salvage.au3 (once)
└── <GUIConstantsEx.au3>, <StaticConstants.au3>, <WindowsConstants.au3>
```

### Function Counts

| File | Functions |
|------|-----------|
| GWA2.au3 | 432 |
| Utils.au3 | 196 |
| GUI_Functions.au3 | 62 |
| Utils-Storage-Bot.au3 | 57 |
| Utils-Maintenance.au3 | 33 |
| Utils-Debugger.au3 | 30 |
| Utils-Salvage.au3 | 8 |
| GWA2_ID.au3 | 6 |
| **Total** | **824** |

### Critical Function Call Sites

These functions are actively called by the Froggy script and must remain resolvable after every phase:

| Function | Called From | Call Count |
|----------|------------|------------|
| `Disconnected()` | Froggy lines 622, 835, 1427 | 3 |
| All GWA2 API functions | Froggy (via Utils.au3 and direct calls) | 100+ |
| `PerformMaintenance()` | Froggy (main loop) | 1+ |
| `RunDiagnostics()` | Froggy (main loop) | 1+ |
| `GUI_*()` functions | Froggy (throughout) | 50+ |
| `Out()` / `Info()` / `Warn()` | Froggy + all libs | 100+ |

---

## Test Levels

### Level 1: Au3Check Per-File
Run Au3Check on a single modified file.
**Pass:** Error count ≤ baseline for that file.

### Level 2: Au3Check Full Chain (Froggy)
Run Au3Check on Froggy_HM_v1.6.au3 (validates the entire transitive include tree).
**Pass:** Error count ≤ 202 (baseline).

### Level 3: Dangling Reference Audit
Grep all `.au3` files in the project for calls to functions that were moved/renamed. Verify every call site resolves to a valid `Func` definition in the include chain.
```bash
# For each moved function:
grep -rn "FunctionName(" "GWA Censured/" --include="*.au3" | grep -v "^.*Func FunctionName("
# Each call site must have a matching Func in the include chain
```
**Pass:** Zero dangling references.

### Level 4: Include Chain Integrity
Verify no circular includes cause crashes, and all files in the chain can be loaded:
```autoit
; tests/test_include_chain.au3
#RequireAdmin
#include '../GWA Censured/lib/Froggy_Includes.au3'
ConsoleWrite("INCLUDE CHAIN: OK" & @CRLF)
Exit 0
```
**Pass:** Exit code 0, "OK" in stdout.

### Level 5: Keystone ASM Validation
For Phase 2 (crafting extraction), validate that the extracted ASM procedures produce identical opcodes to the current embedded versions.
**Pass:** All crafting instruction opcodes match.

### Level 6: Byte-for-Byte Function Equivalence
After extracting a function to a new file, verify it is character-identical to the version that was removed from the old file (no accidental edits during the move).
```bash
# Extract function from old file, extract from new file, diff
diff <(sed -n '/^Func FunctionName/,/^EndFunc/p' old_file.au3) \
     <(sed -n '/^Func FunctionName/,/^EndFunc/p' new_file.au3)
```
**Pass:** Zero diff lines.

---

## Phase-by-Phase Test Procedures

### Phase 0: Establish Directory Structure

**Changes:**
- Create `lib/botshub/` and `lib/custom/` directories
- Move already-separate custom files into `lib/custom/`
- Create `lib/Froggy_Includes.au3` master include
- Update Froggy's includes to use master include
- Update internal `#include` paths in moved files

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 0.1 | L2 | Au3Check on Froggy | ≤ 202 errors |
| 0.2 | L1 | Au3Check on each moved file | ≤ baseline per file |
| 0.3 | L3 | Grep for old include paths (`lib/GUI_Functions`, `lib/Utils-Maintenance`, etc.) | Zero references to old paths |
| 0.4 | L4 | Dry-load test (include chain loads without crash) | Exit 0 |

**Rollback trigger:** Any new Au3Check error. Any old include path still referenced. Include chain crash.

**Gate:** All 4 tests pass → GO Phase 1.

---

### Phase 1: Extract Custom Functions from GWA2.au3

**Changes:**
- Create `lib/custom/GWA2_Extensions.au3`
- Move 15 standalone custom functions out of GWA2.au3
- Add `#include 'GWA2_Extensions.au3'` to master include (after GWA2.au3)

**Functions being moved:**

| Function | Risk | Notes |
|----------|------|-------|
| IsPlayerDead | Low | Not called by Froggy |
| IsHeroDead | Low | Not called by Froggy |
| GetCastTimeModifier | Low | Not called by Froggy |
| UseSkillEx | Low | Not called by Froggy |
| UseSkillTimed | Low | Not called by Froggy |
| UseHeroSkillEx | Low | Not called by Froggy |
| UseHeroSkillTimed | Low | Not called by Froggy |
| GetIsMerchantOpen | Low | Called by CraftItem (staying in GWA2 for now) |
| GetItemIDFromModelID | Low | Not called by Froggy |
| ClearAttributes | Low | Not called by Froggy |
| **Disconnected** | **Medium** | **Called by Froggy at 3 sites** |
| _GWA2_GetAlmostInRangeOfAgent | Low | Internal helper |
| _GWA2_GetInventoryItemPtrByModelId | Low | Called by CraftItem |
| _GWA2_CountItemInBagsByModelID | Low | Called by CraftItem |
| Extend_Write, Extend_AssemblerWriteDetour | Low | Placeholder stubs |

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 1.1 | L6 | Byte-for-byte equivalence of each moved function | Zero diff per function |
| 1.2 | L1 | Au3Check on GWA2.au3 (after removal) | ≤ 3 errors |
| 1.3 | L1 | Au3Check on GWA2_Extensions.au3 (new file) | Record new baseline |
| 1.4 | L2 | Au3Check on Froggy | ≤ 202 errors |
| 1.5 | L3 | Dangling reference audit for all 15 functions | Zero danglers — every call site still resolves |
| 1.6 | L3 | Specific: grep Froggy for `Disconnected(` — verify it resolves via include chain | Found in GWA2_Extensions.au3 |

**Rollback trigger:** Any function body differs after move. Any new Au3Check error. Disconnected() not resolvable from Froggy.

**Gate:** All 6 tests pass → GO Phase 3a.

---

### Phase 3a: Restore `#include 'Utils.au3'` in GWA2.au3

**Changes:**
- Re-add `#include 'Utils.au3'` to GWA2.au3
- Remove `_GWA2_FillArray()` from GWA2.au3 (now available from Utils.au3's `FillArray`)
- Update calls from `_GWA2_FillArray` to `FillArray`
- Remove logging stubs from Utils-Debugger.au3 that were workarounds (if `Info`/`Warn`/`Error` now resolve through the include chain)

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 3a.1 | L4 | Dry-load test — no infinite recursion from circular includes | Exit 0, no hang |
| 3a.2 | L1 | Au3Check on GWA2.au3 | Errors ≤ 3 (may decrease) |
| 3a.3 | L2 | Au3Check on Froggy | ≤ 202 errors (may decrease) |
| 3a.4 | L3 | Grep for `_GWA2_FillArray` anywhere in codebase | Zero references |
| 3a.5 | L1 | Au3Check on Utils-Debugger.au3 | Errors ≤ 7 (may decrease if stubs removed) |

**Critical risk:** If `#include-once` does NOT prevent the circular dependency (contrary to upstream's assumption), this phase will cause an infinite loop. Test 3a.1 catches this immediately.

**Rollback trigger:** Dry-load hangs or crashes. Any new Au3Check error. Remaining references to `_GWA2_FillArray`.

**Gate:** All 5 tests pass → GO Phase 3b.

---

### Phase 3b: Adopt GetAgentArray 0-indexed Convention

**Changes:**
- Revert `GetAgentArray()` in GWA2.au3 to return 0-indexed array (matching upstream)
- Find and update ALL callers that use `$array[0]` as count or `For $i = 1 To $array[0]`

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 3b.1 | L3 | Grep for `\[0\]` patterns that reference agent array counts | Zero remaining 1-indexed patterns |
| 3b.2 | L3 | Grep for `For $i = 1 To.*\[0\]` | Zero matches |
| 3b.3 | L1 | Au3Check on GWA2.au3 | ≤ baseline |
| 3b.4 | L2 | Au3Check on Froggy | ≤ 202 |
| 3b.5 | L1 | Au3Check on Utils.au3 | ≤ 12 |
| 3b.6 | L1 | Au3Check on Utils-Maintenance.au3 | ≤ 201 |

**High-risk files to check for callers:**
- Utils.au3 (196 functions — many may iterate agent arrays)
- Utils-Maintenance.au3
- Froggy_HM_v1.6.au3
- GWA2.au3 internal callers (GetAgentDanger, etc.)

**Rollback trigger:** Any new Au3Check error. Any remaining 1-indexed iteration pattern found.

**Gate:** All 6 tests pass → GO Phase 2.

---

### Phase 2: Extract Crafting System

**Changes:**
- Create `lib/custom/GWA2_Crafting.au3`
- Move crafting globals ($CRAFT_ITEM_STRUCT, $CRAFT_MATS_STRUCT_MEMORY, etc.)
- Move CraftItem(), CraftItemSafe() functions
- Move crafting ASM procedures (CommandCraftItemEx2, CommandRequestCraftQuote, CommandCraftExecute) into `Extend_CreateCommands()` hook
- Move TradeID data slots into `Extend_CreateData()` hook
- Move TradeID init into `Extend_InitializeGameClientData()` hook
- Add 3 hook call lines to GWA2.au3

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 2.1 | L5 | Keystone: validate all crafting ASM opcodes match pre-move versions | Zero mismatches |
| 2.2 | L6 | Byte-for-byte: CraftItem() function body identical after move | Zero diff |
| 2.3 | L1 | Au3Check on GWA2.au3 (with hooks added) | ≤ baseline |
| 2.4 | L1 | Au3Check on GWA2_Crafting.au3 (new file) | Record baseline |
| 2.5 | L2 | Au3Check on Froggy | ≤ 202 |
| 2.6 | L3 | Dangling reference audit: CraftItem, CraftItemSafe, all crafting functions | Zero danglers |
| 2.7 | L5 | Extract crafting ASM from both old and new code, compare assembled bytes | Byte-identical output |

**Test 2.7 detail — ASM Equivalence Test:**
```python
# Before Phase 2: capture the hex output of crafting ASM procedures
# After Phase 2: capture again from the new hook-based location
# Diff the two hex strings — must be identical
```

We'll build a Python script that:
1. Extracts all `_('...')` calls between `CommandCraftItemEx2:` and the next label
2. Uses Keystone to assemble each instruction
3. Concatenates the bytes
4. Compares pre-move vs post-move

**Rollback trigger:** Any opcode mismatch. Any new Au3Check error. Any dangling CraftItem reference.

**Gate:** All 7 tests pass → GO Phase 4.

---

### Phase 4: Adopt Upstream File Splitting

**Changes:**
- Copy upstream split files into `lib/botshub/` (GWA2.au3, GWA2_Assembly.au3, GWA2_ID.au3 + 4 sub-files, Utils.au3, Utils-Agents.au3, Utils-Storage.au3, Utils-Debugger.au3, Utils-Items_Modstructs.au3, JSON.au3)
- Add 3 crafting hook lines to botshub/GWA2.au3
- Apply MemoryRead compatibility wrapper (Phase 3c — see below)
- Delete old monolithic files
- Delete redundant data files (Map_IDs.au3, Skill_IDs.au3 — now in upstream's GWA2_ID_*.au3)
- Update master include (Froggy_Includes.au3) to reference new paths
- Merge custom Utils additions (SalvageAllItems overhaul, quest helpers, stuck detection, etc.) into either upstream Utils.au3 or a custom/Utils_Extensions.au3

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 4.1 | L1 | Au3Check on every file in `lib/botshub/` | Record new baselines |
| 4.2 | L1 | Au3Check on every file in `lib/custom/` | ≤ existing baselines |
| 4.3 | L2 | Au3Check on Froggy | ≤ 202 (may decrease significantly) |
| 4.4 | L4 | Dry-load test with new include structure | Exit 0 |
| 4.5 | L3 | Grep for references to deleted files (Map_IDs.au3, Skill_IDs.au3, Utils-Storage-Bot.au3) | Zero references |
| 4.6 | L3 | Grep for old `lib/` include paths (without `botshub/` or `custom/` prefix) | Zero references |
| 4.7 | L5 | Full Keystone regression (65 opcodes + crafting opcodes) | All pass |
| 4.8 | L3 | Verify all Froggy function calls resolve: grep each unique function call in Froggy, verify a Func definition exists in the include chain | Zero unresolved |

**This is the highest-risk phase.** Many things change simultaneously. If possible, break it into sub-phases:
- 4a: Copy upstream files into botshub/ (no deletions yet — both old and new coexist)
- 4b: Switch Froggy_Includes.au3 to reference botshub/ files
- 4c: Delete old files
- 4d: Verify clean state

**Rollback trigger:** Any new Au3Check error. Include chain crash. Unresolved function references.

**Gate:** All 8 tests pass → GO Phase 5.

---

### Phase 5: Extract NPC Coordinates

**Changes:**
- Create `lib/custom/NPC_Coordinates.au3`
- Move custom NPC coordinate data (Gadd's Encampment, Embark Beach traders, Xunlai chests) from Utils-Storage.au3 (or Utils-Storage-Bot.au3) into new file
- Update callers in Utils-Maintenance.au3

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 5.1 | L6 | Byte-for-byte: coordinate data identical after move | Zero diff |
| 5.2 | L1 | Au3Check on NPC_Coordinates.au3 | 0 errors |
| 5.3 | L2 | Au3Check on Froggy | ≤ existing |
| 5.4 | L3 | Grep for hardcoded coordinates in botshub/ files | Zero custom coordinates in botshub/ |

**Rollback trigger:** Any new error. Coordinate data mismatch.

**Gate:** All 4 tests pass → GO Phase 3c.

---

### Phase 3c: MemoryRead Signature Compatibility

**Changes:**
- Create `MemoryReadCompat($address, $type)` wrapper that calls upstream's `MemoryRead($processHandle, $address, $type)`
- Similarly for `MemoryReadPtrCompat`, `MemoryWriteCompat`
- Find and replace all custom-code calls from `MemoryRead(` to `MemoryReadCompat(`
- Leave botshub/ files untouched (they use upstream's signature internally)

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 3c.1 | L3 | Grep custom/ files for bare `MemoryRead(` (without Compat) | Zero in custom/ (all converted) |
| 3c.2 | L3 | Grep botshub/ files for `MemoryReadCompat(` | Zero in botshub/ (none converted) |
| 3c.3 | L1 | Au3Check on each custom/ file | ≤ existing baselines |
| 3c.4 | L2 | Au3Check on Froggy | ≤ existing |

**Rollback trigger:** Any call-site missed. Any new error.

---

## Decision Gate Summary

| Phase | GO Criteria | NO-GO Action |
|-------|-------------|--------------|
| Phase 0 | 0.1-0.4 all pass | Fix include paths, retry |
| Phase 1 | 1.1-1.6 all pass | Revert function moves, investigate which broke |
| Phase 3a | 3a.1-3a.5 all pass | Revert #include restoration (circular dep confirmed) |
| Phase 3b | 3b.1-3b.6 all pass | Revert indexing change, fix missed callers |
| Phase 2 | 2.1-2.7 all pass | Revert crafting extraction, investigate ASM mismatch |
| Phase 4 | 4.1-4.8 all pass | Revert to pre-Phase-4, investigate which upstream file breaks chain |
| Phase 5 | 5.1-5.4 all pass | Revert coordinate extraction |
| Phase 3c | 3c.1-3c.4 all pass | Fix missed call sites |

---

## Git Strategy

```
coa3/refactor-for-mergeability
  ├── "Phase 0: Establish botshub/custom directory structure"
  ├── "Phase 1: Extract 15 custom functions into GWA2_Extensions.au3"
  ├── "Phase 3a: Restore #include Utils.au3, remove circular dep workarounds"
  ├── "Phase 3b: Adopt 0-indexed GetAgentArray convention"
  ├── "Phase 2: Extract crafting system with hook pattern"
  ├── "Phase 4a: Copy upstream split files into botshub/"
  ├── "Phase 4b: Switch includes to botshub/ paths"
  ├── "Phase 4c: Delete old monolithic files"
  ├── "Phase 5: Extract NPC coordinates into custom/"
  └── "Phase 3c: Add MemoryRead compatibility wrappers"
```

Each commit is atomic and independently revertible. Phase 4 is split into 3 sub-commits for safety.

---

## What We Cannot Test Without a Game Client

| Gap | Risk | Mitigation |
|-----|------|------------|
| Crafting hooks fire in correct ASM injection order | High | Keystone byte-comparison pre/post move |
| Extension hooks called at runtime | Medium | IsDeclared() guards — safe no-op if not loaded |
| MemoryRead wrapper returns correct values | Medium | Wrapper is trivial (adds one arg), verified by Au3Check |
| NPC navigation works with extracted coordinates | Low | Coordinates are data — byte-for-byte verified |
| GetAgentArray callers iterate correctly | Medium | Grep audit catches all callers; logic is simple |
| Include chain initializes in correct order | Medium | Dry-load test catches initialization failures |

---

## Estimated Total Test Execution Time

| Phase | Tests | Est. Time |
|-------|-------|-----------|
| Phase 0 | 4 tests | ~5 min |
| Phase 1 | 6 tests | ~15 min |
| Phase 3a | 5 tests | ~10 min |
| Phase 3b | 6 tests | ~15 min |
| Phase 2 | 7 tests | ~30 min (Keystone + ASM extraction) |
| Phase 4 | 8 tests | ~30 min |
| Phase 5 | 4 tests | ~5 min |
| Phase 3c | 4 tests | ~10 min |
| **Total** | **44 tests** | **~2 hours** |
