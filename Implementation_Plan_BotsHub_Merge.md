# Implementation Plan: Merging Latest BotsHub into GWA Censured

**Date:** 2026-03-25
**Objective:** Pull latest BotsHub improvements into the GWA Censured project without breaking the Froggy script
**Risk Level:** Medium-High (significant architectural divergence between codebases)

---

## Critical Findings

Before evaluating courses of action, these findings from the conflict analysis shape the entire strategy:

### 1. Your Trader Scan Pattern is STALE

| Version | TraderHook Pattern | Status |
|---------|-------------------|--------|
| Original (4cb3dd5) | `...6A5450` | Outdated |
| **Your current code** | `...6A5550` | **Stale — was correct, now outdated** |
| Latest BotsHub | `...6A5650` | Current (updated in commit ae94a11) |

**If the game client has updated since you last ran, your bot may already be broken.** The latest BotsHub fixed this in the "Assembly fixes after client updates" commit.

### 2. The ASM Architecture Fundamentally Diverged

The latest BotsHub completely refactored the memory scanning system:
- **Your code:** Uses `AddPatternToInjection()` (inline pattern scanning baked into the injected ASM blob)
- **Latest BotsHub:** Uses `AddScanPattern()` (named pattern system with a separate scanner, results mapped to labels)

This is not a simple find-and-replace. The entire scan registration, execution, and result retrieval pipeline changed.

### 3. Your Crafting System Has No Upstream Equivalent

The latest BotsHub has only a basic `CommandCraftItem` stub. Your entire crafting pipeline (CraftItem function, CraftItemSafe, CommandCraftItemEx2, CommandRequestCraftQuote, CommandCraftExecute, TradeID management, VirtualAllocEx material arrays) is **100% custom**. There is nothing to "merge" — your code must be preserved and adapted to work within whichever architecture you adopt.

### 4. GetAgentArray Convention Conflict

| Version | Convention |
|---------|-----------|
| Your code | 1-indexed, `$array[0] = count` |
| Latest BotsHub | 0-indexed, uses `For...In` |

Your Froggy script and maintenance code iterate using `For $i = 1 To $agents[0]`. Adopting the latest's convention means finding and updating every such loop.

### 5. Some Fixes Are Already Aligned

- Both you and upstream removed `ScanMapLoading` and `ScanLoggedIn`
- Both have `IsPlayerDead()` / `IsHeroDead()` (yours in GWA2.au3, theirs in Utils-Agents.au3)
- Both broke the circular dependency (you via `_GWA2_*` prefixed helpers, they via file splitting)

---

## Courses of Action

### COA 1: Surgical Cherry-Pick (LOW RISK)

**Strategy:** Keep your current architecture intact. Only pull in specific critical fixes and high-value features from the latest BotsHub by manually porting them into your existing files.

**What you'd adopt:**
- Updated scan patterns (trader hook `56`, CompassFlag, EnterMission) — ~5 line changes
- New ASM `mov`/`add`/`sub` instruction handlers from commit ae94a11 — ~200 lines into your GWA2.au3 assembler section
- `IsPlayerStuck()` and `TryToGetUnstuck()` — ~80 lines into your Utils.au3
- Quest helper functions (`TakeQuest`, `TakeQuestReward`, `IsQuestActive`, etc.) — ~100 lines into your Utils.au3
- `UseConset()` function — ~30 lines
- Individual bug fixes from upstream (Froggy quest interaction changes, etc.)

**What you'd skip:**
- The entire file-splitting reorganization (GWA2_Assembly.au3, Utils-Agents.au3, Utils-Storage.au3, etc.)
- The `AddScanPattern()` refactor
- The GUI extraction to BotsHub-GUI.au3
- The src/ directory reorganization
- The headless mode
- Any changes that would require reworking your crafting system

**Effort:** ~2-4 hours
**Risk:** Low — your code stays structurally the same, you just patch in fixes
**Downside:** You diverge further from upstream. Future merges become harder. You miss some improvements that are tightly coupled to the new architecture (e.g., the new assembly command improvements that depend on `AddScanPattern()`).

**Best for:** Getting back to a working state quickly with the latest game client compatibility.

---

### COA 2: Full Rebase onto Latest BotsHub (HIGH RISK)

**Strategy:** Start from the latest BotsHub codebase and re-apply all your custom modifications on top of it.

**Steps:**
1. Start with a clean copy of `BotsHub-latest/`
2. Re-implement your circular dependency solution (but adapted to the new split-file architecture)
3. Port your crafting system into `GWA2_Assembly.au3` (ASM procedures) and a new `GWA2_Crafting.au3` or into `GWA2.au3` (AutoIt-side CraftItem function)
4. Port `GetMapLoading()` and `GetLoggedIn()` into the latest's GWA2.au3
5. Adapt your `GetAgentArray()` 1-indexed change — or rewrite your iteration code to use 0-indexed / `For...In`
6. Port your 8 custom files (Utils-Maintenance.au3, Utils-Salvage.au3, GUI_Functions.au3, etc.) — updating all `#include` paths and function references to match the new file structure
7. Port the Froggy script, updating all function calls that changed signatures
8. Extensive testing of every code path

**Effort:** ~15-25 hours
**Risk:** High — many things can break in subtle ways. The split-file architecture means your crafting ASM must be injected through a different pipeline. The `AddScanPattern()` system works differently from `AddPatternToInjection()`, so your custom scan patterns need to be re-registered. Every `#include` path changes. Function signatures changed throughout.
**Upside:** You end up on the latest architecture. Future upstream changes are easier to merge. You get ALL improvements (headless mode, new bots, expanded quest system, conset automation, GUI improvements, etc.).

**Best for:** Long-term maintainability if you plan to keep this project alive for months/years.

---

### COA 3: Hybrid — New Architecture, Selective Features (MEDIUM RISK)

**Strategy:** Adopt the latest BotsHub's file structure and core engine (GWA2_Assembly.au3, split IDs, Utils-Agents.au3), but keep your custom GUI and maintenance systems rather than adopting BotsHub's GUI.

**Phase 1 — Core Engine Upgrade (~8-12 hours)**
1. Copy latest `lib/GWA2_Assembly.au3` into `GWA Censured/lib/`
2. Copy latest `lib/GWA2.au3` and merge your custom additions:
   - Add `GetMapLoading()`, `GetLoggedIn()` (user additions, latest doesn't have them)
   - Add `GetIsMerchantOpen()` (user addition)
   - Keep `CraftItem()` function (user addition — no upstream equivalent)
   - Drop `IsPlayerDead()`/`IsHeroDead()` (now in Utils-Agents.au3)
   - Resolve `GetAgentArray()` indexing (adopt 0-indexed, update your loops)
3. Copy latest `lib/GWA2_ID.au3` + sub-files (`GWA2_ID_Items.au3`, `GWA2_ID_Maps.au3`, `GWA2_ID_Quests.au3`, `GWA2_ID_Skills.au3`)
   - Your `Map_IDs.au3` and `Skill_IDs.au3` become redundant (latest has comprehensive versions)
   - Your range constants already moved to GWA2_ID.au3 — verify they exist in latest
4. Copy latest `lib/Utils-Agents.au3` (new file — gives you IsPlayerDead, stuck detection base, party utilities)
5. Port your crafting ASM procedures into `GWA2_Assembly.au3`:
   - Add `CommandCraftItemEx2`, `CommandRequestCraftQuote`, `CommandCraftExecute` labels
   - Add `TradeID/4` and `TradeID_Value/4` memory data slots
   - Register your custom scan patterns using the new `AddScanPattern()` API

**Phase 2 — Utility Layer Update (~4-6 hours)**
6. Merge latest `lib/Utils.au3` with your additions:
   - Keep your `SalvageAllItems()` overhaul
   - Keep your `IsPlayerAtMaxMalus()`, `IsRareSkin()`, `ShouldBlacklist()`, `DefaultShouldSalvageItem()`
   - Adopt latest's quest helpers (`TakeQuest`, `TakeQuestReward`, `IsQuestActive`, etc.)
   - Adopt latest's stuck detection (`IsPlayerStuck`, `TryToGetUnstuck`)
   - Adopt latest's `UseConset()` and conset automation
   - Adopt latest's `ResignAndReturnToOutpost()` with explicit map ID
   - Adopt latest's `RandomSleep()` improvements
7. Copy latest `lib/Utils-Storage.au3` (replaces Utils-Storage-Bot.au3)
   - Merge your NPC coordinate additions (Gadd's, Embark Beach traders, Xunlai chests)
   - Merge your sell-hardening (fail count, memory re-reads)
   - Merge your `GoToXunlaiChest()` function
8. Keep your custom files unchanged:
   - `Utils-Maintenance.au3` — update `#include` paths and function calls
   - `Utils-Salvage.au3` — update function calls if signatures changed
   - `GUI_Functions.au3` — keep as-is (you don't want BotsHub's GUI)
   - `UsefulMods.au3`, `RareSkins.au3`, `Skill_Types.au3` — keep as-is

**Phase 3 — Froggy Script Update (~2-3 hours)**
9. Update `Froggy_HM_v1.6.au3`:
   - Update `#include` paths for new file structure
   - Replace any quest interaction code with new `TakeQuest()`/`TakeQuestReward()` helpers
   - Add stuck detection hooks
   - Update any `GetAgentArray()` iteration patterns
   - Test each phase of the farm loop

**Effort:** ~14-21 hours total across 3 phases
**Risk:** Medium — each phase is testable independently. Phase 1 is the riskiest (ASM changes). Phases 2 and 3 are lower risk.
**Upside:** Modern architecture, easier future merges, get most valuable improvements. You keep your custom GUI and maintenance system.
**Downside:** Still significant effort. The crafting ASM port into the new `AddScanPattern()` system requires understanding both the old and new injection pipelines.

---

### COA 4: Parallel Stack with Shared Engine (LOW-MEDIUM RISK)

**Strategy:** Don't merge at all in the traditional sense. Instead, create a thin compatibility layer that lets your GWA Censured code call into the latest BotsHub's engine when beneficial, while keeping your existing code as the primary stack.

**Implementation:**
1. Copy latest `lib/GWA2_Assembly.au3` as `GWA Censured/lib/GWA2_Assembly_Latest.au3`
2. Create a `GWA Censured/lib/Compat.au3` bridge file that:
   - Loads the latest assembly backend for scan patterns and ASM generation
   - Wraps new utility functions (quest helpers, stuck detection) in your naming convention
   - Provides `GetAgentArray()` in your 1-indexed format using the latest's engine underneath
3. Update only the scan patterns in your GWA2.au3 (the ~5 critical byte changes)
4. Selectively `#include` new utility files (Utils-Agents.au3) where needed

**Effort:** ~6-10 hours
**Risk:** Low-Medium — you maintain two partially-overlapping codebases, which is messy but safe
**Upside:** Minimal disruption to working code. Can incrementally adopt more over time.
**Downside:** Technical debt. Two ASM backends coexisting. Confusing for future maintenance. The compatibility layer itself could have subtle bugs.

---

## Recommendation

### For immediate needs: **COA 1 (Surgical Cherry-Pick)**

If your bot is currently working or only needs scan pattern updates to work again, do COA 1 first. It takes 2-4 hours and gets you running with current game client compatibility. The specific changes:

1. Update 3 scan patterns in your GWA2.au3 (~5 minutes)
2. Port the new ASM instruction handlers from ae94a11 (~1-2 hours)
3. Cherry-pick quest helpers and stuck detection into Utils.au3 (~1-2 hours)

### For long-term health: **COA 3 (Hybrid) after COA 1**

Once you're stable on COA 1, plan COA 3 as a phased migration. Do it in three separate branches so each phase can be tested independently:
- **Branch 1:** Core engine upgrade (GWA2_Assembly.au3 adoption)
- **Branch 2:** Utility layer update (Utils.au3, Utils-Agents.au3, Utils-Storage.au3)
- **Branch 3:** Froggy script update

### Avoid COA 2 unless you have significant time

A full rebase is the "cleanest" result but the effort-to-risk ratio is poor. Too many things change simultaneously, making bugs hard to isolate.

### Avoid COA 4 unless you're time-constrained

The parallel stack approach sounds appealing but creates maintenance headaches. Two overlapping ASM backends is a recipe for subtle memory corruption bugs.

---

## Risk Matrix

| Risk | COA 1 | COA 2 | COA 3 | COA 4 |
|------|-------|-------|-------|-------|
| Breaks Froggy script | Low | High | Medium | Low |
| Breaks crafting system | None | High | Medium | Low |
| Breaks maintenance loop | None | High | Medium | Low |
| Game client incompatibility | **Fixes it** | **Fixes it** | **Fixes it** | Partially fixes |
| Future merge difficulty | Increases | Eliminates | Reduces | Increases |
| Effort (hours) | 2-4 | 15-25 | 14-21 | 6-10 |
| Can be done incrementally | Yes | No | Yes (3 phases) | Yes |
| Gets stuck detection | Partial | Full | Full | Partial |
| Gets quest helpers | Yes | Yes | Yes | Partial |
| Gets new bots | No | Yes | No | No |
| Gets headless mode | No | Yes | No | No |

---

## Immediate Action Items (COA 1)

If you want to start with the low-risk approach right now, here are the exact changes needed:

### Step 1: Update Scan Patterns in GWA2.au3 (CRITICAL)

```
TraderHook:  6A5550  →  6A5650
CompassFlag: 566A5C57  →  566A5D57
EnterMission: A900001000743A (offset 0x52)  →  83C902890A5D (offset 0x24)
```

### Step 2: Port New ASM Instruction Handlers

From commit ae94a11, add the generic `add reg,imm` / `sub reg,imm` / `mov` handlers to replace the per-register hardcoded handlers in your assembler section. This ensures any new ASM code (including your crafting procedures) assembles correctly.

### Step 3: Add Quest Helpers

Copy these functions from `BotsHub-latest/lib/Utils.au3` into your `GWA Censured/lib/Utils.au3`:
- `TakeQuest()`, `TakeQuestReward()`, `TakeQuestOrReward()`
- `IsQuestActive()`, `IsQuestReward()`, `IsQuestCompleted()`, `IsQuestNotFound()`
- `QuestStateMatches()`

### Step 4: Add Stuck Detection

Copy from `BotsHub-latest/lib/Utils.au3`:
- `IsPlayerStuck()`
- `TryToGetUnstuck()`
- `CheckStuck()`
- `CheckAndSendStuckCommand()`

### Step 5: Test

Run Froggy through a complete cycle: launch → farm → maintenance → repeat. Verify:
- Game client injection succeeds (scan patterns correct)
- Crafting still works (your custom ASM unaffected)
- Movement and combat work (no GetAgentArray issues since you didn't change it)
- Maintenance loop completes (selling, buying, depositing)

---

## Appendix: Files That Would Change Per COA

### COA 1 — Surgical Cherry-Pick
- `GWA Censured/lib/GWA2.au3` — scan patterns + ASM handlers
- `GWA Censured/lib/Utils.au3` — new helper functions appended

### COA 2 — Full Rebase
- Every file in `GWA Censured/lib/` replaced or heavily modified
- `GWA Censured/Froggy_HM_v1.6.au3` — updated includes + function calls
- All `#include` paths change

### COA 3 — Hybrid
**Phase 1:**
- `GWA Censured/lib/GWA2.au3` — merged with latest
- `GWA Censured/lib/GWA2_Assembly.au3` — new file from latest
- `GWA Censured/lib/GWA2_ID.au3` — replaced with latest + sub-files
- `GWA Censured/lib/Utils-Agents.au3` — new file from latest
- `GWA Censured/lib/Map_IDs.au3` — potentially removed (redundant)
- `GWA Censured/lib/Skill_IDs.au3` — potentially removed (redundant)

**Phase 2:**
- `GWA Censured/lib/Utils.au3` — merged with latest
- `GWA Censured/lib/Utils-Storage.au3` — new file replacing Utils-Storage-Bot.au3
- `GWA Censured/lib/Utils-Maintenance.au3` — updated includes/calls
- `GWA Censured/lib/Utils-Salvage.au3` — updated function calls

**Phase 3:**
- `GWA Censured/Froggy_HM_v1.6.au3` — updated includes + function calls

### COA 4 — Parallel Stack
- `GWA Censured/lib/GWA2_Assembly_Latest.au3` — new file
- `GWA Censured/lib/Compat.au3` — new bridge file
- `GWA Censured/lib/GWA2.au3` — scan pattern updates only
- `GWA Censured/lib/Utils-Agents.au3` — new file from latest (optional)
