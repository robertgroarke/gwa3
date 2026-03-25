# BotsHub User Modifications Report

**Scope:** All modifications made to BotsHub files after pulling commit `4cb3dd5` (2026-01-11)
**Comparison base:** `BotsHub-original/` (clean checkout at 4cb3dd5)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [GWA2.au3 — Core Engine Modifications](#2-gwa2au3--core-engine-modifications)
3. [Utils.au3 — Utility Function Modifications](#3-utilsau3--utility-function-modifications)
4. [GWA2_ID.au3 — ID Database Modifications](#4-gwa2_idau3--id-database-modifications)
5. [Utils-Debugger.au3 — Debug System Modifications](#5-utils-debuggerau3--debug-system-modifications)
6. [Utils-Storage-Bot.au3 — Storage Bot Modifications](#6-utils-storage-botau3--storage-bot-modifications)
7. [Utils-Items_Modstructs.au3 — Mod Struct Modifications](#7-utils-items_modstructsau3--mod-struct-modifications)
8. [BotsHub.au3 — Launcher Modifications](#8-botshubau3--launcher-modifications)
9. [Farm-Vaettirs.au3 — Vaettirs Bot Modifications](#9-farm-vaettirsau3--vaettirs-bot-modifications)
10. [New Files — GWA Censured Library](#10-new-files--gwa-censured-library)
11. [Line-Ending-Only Files (No Functional Changes)](#11-line-ending-only-files-no-functional-changes)

---

## 1. Executive Summary

The modifications fall into five major categories:

1. **Circular dependency elimination** — GWA2.au3, GWA2_ID.au3, and Utils-Debugger.au3 all removed their `#include 'Utils.au3'` to break circular imports. Each file gained local helper functions prefixed with `_GWA2_`, `_GWA2_ID_`, or `_Debugger_` respectively.

2. **Broken scan pattern fixes** — Two critical memory scan patterns (`ScanMapLoading`, `ScanLoggedIn`) were non-functional in the current game client. Both were replaced with alternative detection methods. Two trader scan patterns were updated with corrected byte sequences for the current client version.

3. **Crafting system rewrite** — The entire crafting pipeline was rebuilt: new `CRAFT_ITEM_STRUCT` layout, three new/rewritten ASM injection procedures, proper `TransactionFunction(3)` calling convention, TradeID memory management, and a complete rewrite of the AutoIt-side `CraftItem()` function.

4. **Salvage/maintenance system** — A comprehensive maintenance automation system was built across multiple new files (`Utils-Maintenance.au3`, `Utils-Salvage.au3`, `UsefulMods.au3`, `RareSkins.au3`) handling identification, salvage with mod extraction, selling, crafting consumables, and NPC navigation.

5. **GUI and bot framework** — A custom GUI (`GUI_Functions.au3`) with statistics tracking, settings persistence, and mod selection was created for the Froggy farming script.

### Files with Functional Changes

| File | Change Magnitude | Key Changes |
|------|-----------------|-------------|
| **GWA2.au3** | Major | Crafting rewrite, scan pattern fixes, dependency fixes, 8 new functions |
| **Utils.au3** | Moderate | New salvage logic, new functions, include changes |
| **GWA2_ID.au3** | Moderate | Dependency fix, constants relocated, helper functions added |
| **Utils-Debugger.au3** | Moderate | Dependency fix, debug mode enabled, logging improvements |
| **Utils-Storage-Bot.au3** | Moderate | NPC coordinates, sell retry logic, new functions |
| **Utils-Items_Modstructs.au3** | Minor | Made `$MAP_WEAPON_MODS` mutable |
| **BotsHub.au3** | Minor | Bug fix for inventory management ordering, log reduction |
| **Farm-Vaettirs.au3** | Minor | Gameplay tuning, performance optimizations |
| **38 other src/ files** | None | Line-ending differences only |

### New Files (No BotsHub Counterpart)

| File | Lines | Purpose |
|------|-------|---------|
| **Utils-Maintenance.au3** | ~600+ | Full maintenance automation (town trips, selling, buying, crafting) |
| **Utils-Salvage.au3** | ~200+ | Salvage decision logic, mod extraction, sell filtering |
| **GUI_Functions.au3** | ~800+ | Complete Froggy bot GUI with settings and statistics |
| **Map_IDs.au3** | ~666 | Comprehensive Guild Wars map ID constants |
| **Skill_IDs.au3** | ~530+ | Comprehensive skill ID constants |
| **RareSkins.au3** | ~180 entries | Rare/valuable weapon skin lookup table |
| **UsefulMods.au3** | ~450+ | Weapon mod (133) and armor mod (183) data tables |
| **Skill_Types.au3** | ~24 | Skill type classification constants |

---

## 2. GWA2.au3 — Core Engine Modifications

### 2.1 Circular Dependency Fix

- **Removed** `#include 'Utils.au3'` — prevents circular dependency since Utils.au3 includes GWA2.au3
- **Added** `#include <Math.au3>` — compensates for math functions previously pulled in transitively
- **Added 6 local helper functions** (prefixed `_GWA2_`) to replace Utils.au3 dependencies:
  - `_GWA2_FillArray(ByRef $array, $value)` — fills array elements
  - `_GWA2_GetAlmostInRangeOfAgent($targetAgent, $proximity)` — move within range of target
  - `_GWA2_GetInventoryItemPtrByModelId($modelID)` — search bags 1-4 by ModelID (offset 44)
  - `_GWA2_CountItemInBagsByModelID($modelID)` — count item quantity (offset 76) across bags 1-4
  - `Extend_Write()` — empty placeholder stub
  - `Extend_AssemblerWriteDetour()` — empty placeholder stub

### 2.2 Removed Global Variables

- **`$is_logged_in`** — removed; `GetLoggedIn()` now uses agent existence check
- **`$map_loading`** — removed; `GetMapLoading()` now reads `$instance_info_ptr` directly

### 2.3 New Global Variables

- **`$trade_id_addr`** — memory address for TradeID (used by crafting ASM)
- **`$trade_id_value_addr`** — memory address for TradeID value
- **`$CRAFT_MATS_STRUCT_MEMORY`** — remote process memory buffer for material IDs
- **`$CRAFT_QTYS_STRUCT_MEMORY`** — remote process memory buffer for material quantities

### 2.4 Broken Scan Pattern Fixes

| Pattern | Status | Replacement |
|---------|--------|-------------|
| `ScanMapLoading` (`2480ED0000000000`) | **Removed** | `GetMapLoading()` reads `$instance_info_ptr` directly |
| `ScanLoggedIn` (`C705ACDE740000000000C3CCCCCCCC`) | **Removed** | `GetLoggedIn()` checks `GetMyID() > 0 AND GetAgentExists()` |

### 2.5 Updated Scan Patterns (Game Version Compatibility)

| Pattern | Old Bytes | New Bytes | Location |
|---------|-----------|-----------|----------|
| ScanBuyItem (trader function) | `83FF10761468D2210000` | `83FF10761468FE210000` | Byte at offset 8: `D2` → `FE` |
| ScanTraderHook | `8D4DFC51576A5450` | `8D4DFC51576A5550` | Byte at offset 6: `54` → `55` |

### 2.6 Crafting System Rewrite

#### CRAFT_ITEM_STRUCT Redesigned

- **Old:** `SafeDllStructCreate('ptr;dword;dword;ptr;dword;dword')` — 6 fields, `Const`
- **New:** `DllStructCreate("ptr;dword;dword;dword;dword;ptr;ptr")` — 7 fields, mutable
- Added two `ptr` fields for material ID and quantity array pointers

#### CraftItem() Function — Complete Rewrite

- Added `GetIsMerchantOpen()` precondition check
- Iterates merchant item list to find item by ModelID AND capture its **index** in the merchant list
- Writes item index to TradeID: `MemoryWrite($trade_id_value_addr, $itemIndex)` then `MemoryWrite($trade_id_addr, $trade_id_value_addr)`
- Changed command target from `CommandCraftItemEx` to `CommandCraftItemEx2`
- All helper calls use local `_GWA2_` prefixed functions
- Fixed variable scoping: `$craftingMaterialType`, `$craftingMaterialStruct`, `$craftingMaterialStructPtr`, `$deadlock` now properly declared `Local`

#### ASM Injection — New/Rewritten Procedures

| Procedure | Status | Purpose |
|-----------|--------|---------|
| `CommandCraftItemEx` | **Rewritten** | New stack layout with `lea edx,dword[eax+8]` for materials pointer, single-indirection TradeID load, explicit `BuyItemBase` push, `CraftItemFunction` call, `add esp,10` cleanup |
| `CommandCraftItemEx2` | **Rewritten** | Clean `TransactionFunction(3)` implementation: 9 arguments (opcode 3, TotalCost, GiveCount, MatIDsArray_PTR, MatQtysArray_PTR, 0, RecvCount=1, &MerchantItemID, &AmountToCraft), `add esp,24` cleanup |
| `CommandRequestCraftQuote` | **New** | Requests crafting price quote via `RequestQuoteFunction` with opcode 3 (CrafterBuy) |
| `CommandCraftExecute` | **New** | Executes craft transaction via `TransactionFunction` with opcode 3 |

#### New Memory Data Slots

- `TradeID/4` — 4 bytes in injected memory for trade ID address
- `TradeID_Value/4` — 4 bytes for trade ID value

#### TraderStart Detour Renamed

- `WriteDetour('TraderStart', 'TraderProc')` → `WriteDetour('TraderStart', 'TraderHookProc')`

### 2.7 New Functions

| Function | Purpose |
|----------|---------|
| `GetIsMerchantOpen()` | Returns `GetMerchantItemsSize() > 0` |
| `IsPlayerDead()` | Checks `Effects` bitmask `0x0010` on player agent |
| `IsHeroDead($heroID)` | Checks `Effects` bitmask `0x0010` on hero agent |

### 2.8 Modified Functions

| Function | Change |
|----------|--------|
| `MemoryRead()` | Added `If @error Then SetError(1); Return 0` after ReadProcessMemory |
| `GetMapLoading()` | Rewritten: reads `$instance_info_ptr` instead of `$map_loading`. Returns 0=Outpost, 1=Explorable, 2=Loading |
| `GetLoggedIn()` | Rewritten: checks `GetMyID() > 0 AND GetAgentExists($myID)` |
| `GetAgentArray()` | Changed from 0-indexed to 1-indexed with `$array[0] = $count` |
| `GetAgentDanger()` | Fixed broken `For $member In $party` loop — `$i` was never defined, `$` sigil was missing on `partyMemberDangers` |
| `Attack()` | Added `If Not IsDllStruct($agent) Then Return False` null safety |
| `UseSkillEx()` | `GetAlmostInRangeOfAgent` → `_GWA2_GetAlmostInRangeOfAgent` |
| `ChangeOffer()` | Hardcoded header `0x02` instead of `$HEADER_TRADE_CHANGE_OFFER` constant |
| `ModifyMemory()` | Added post-call TradeID verification with retry on failure |

---

## 3. Utils.au3 — Utility Function Modifications

### 3.1 Include Changes

| Removed | Added |
|---------|-------|
| `#include <WinAPIDiag.au3>` | `#include 'RareSkins.au3'` |
| | `#include <GUIConstantsEx.au3>` |
| | `#include 'Utils-Salvage.au3'` |

### 3.2 Constants Relocated

- Range constants (`$RANGE_ADJACENT` through `$RANGE_COMPASS`) moved to GWA2_ID.au3
- `$bags_count` moved to GWA2_ID.au3
- `MapFromArray` calls renamed to `_GWA2_ID_MapFromArray`

### 3.3 New Constants

- `$SUCCESS = True`, `$FAIL = False`, `$PAUSE = 2`
- `$STUCK = 6`
- `$ID_CRASH_ITEM_1856 = 0`
- Various GUI control globals for bot framework integration

### 3.4 New Functions

| Function | Purpose |
|----------|---------|
| `IsPlayerAtMaxMalus()` | Returns `True` when death penalty is at -60% |
| `IsRareSkin($itemID)` | Looks up `$aRareSkin` array to identify valuable skins |
| `ShouldBlacklist($itemID)` | Blacklists kits, tomes, crash items, rare skins from salvage |
| `DefaultShouldSalvageItem($item)` | Checks validity, excludes kits (crash-verified), checks `IsMaterialSalvageable` and `IsTrophy()` |

### 3.5 SalvageAllItems() — Major Overhaul

- Added **identification loop** before salvaging (skips rare skins)
- Added **`IsRareSkin()` check** in salvage loop
- Added **`HasUsefulMod()` priority path**: performs `StartSalvage()` + `SalvageMod()` for valuable mods
- Added **disconnect detection** (`GetMapLoading() == 2`) after every salvage operation
- Added **`CanSell()` checks** before processing trophies/materials
- Added **`ShouldBlacklist()` checks** for non-trophy/non-material items
- Added **`IsDllStruct()` null checks** throughout
- Fixed trophy stack salvage loop variable collision (`$k` used for both outer and inner loops)
- Added extensive debug logging via `Out()`

### 3.6 Other Modifications

- `CheckPickupItem()` — uses `IsDeclared("CheckPickupWeapon")` with `Call()` for dynamic dispatch
- `FindEmptySlots()` — removed erroneous `[]` from variable declaration
- `GetSalvageKit()` and `BuySalvageKitInTown()` — added `$townID` parameter (default Eye of the North)
- `IsPlayerDead()` and `IsHeroDead()` — commented out (moved to GWA2.au3)
- Sleep timing optimizations in `ConsumeAlcohol`/`Sweets` — removed ping-dependent delays
- `IdentifyItems()` — log message `'Identifying all items'` → `'Identifying items'`

---

## 4. GWA2_ID.au3 — ID Database Modifications

### 4.1 Circular Dependency Fix

- **Removed** `#include 'Utils.au3'`
- All `MapFromArray()`, `MapFromArrays()`, `MapFromDoubleArray()` calls renamed to `_GWA2_ID_` prefix (~40+ call sites)
- Festival detection functions renamed to `_GWA2_ID_` prefix

### 4.2 Constants Moved Here From Utils.au3

- `$ID_REGION_CHINESE = -2`
- All range constants (`$RANGE_ADJACENT` = 144, `$RANGE_NEARBY` = 250, ... `$RANGE_COMPASS` = 5000) and their squared variants
- `$bags_count = 4`

### 4.3 New Local Helper Functions

- `_GWA2_ID_MapFromArray($keys)` — creates map with all values = 1
- `_GWA2_ID_MapFromDoubleArray($keysAndValues)` — creates map from 2D array
- `_GWA2_ID_MapFromArrays($keys, $values)` — creates map from parallel arrays
- `_GWA2_ID_IsAnniversaryCelebration()` — date check April 22 - May 6
- `_GWA2_ID_IsDragonFestival()` — date check June 27 - July 4
- `_GWA2_ID_IsChristmasFestival()` — date check Dec 19 - Jan 2

### 4.4 Bug Fixes

- `$ID_PLANT_FIBERS` renamed to `$ID_PLANT_FIBER` (singular, consistency fix)
- `SHIELD_MAX_DAMAGE_PER_LEVEL` array — removed trailing element (14 → 13 entries, off-by-one fix)
- Map type declarations — removed erroneous `[]` brackets from `Global Const` assignments

---

## 5. Utils-Debugger.au3 — Debug System Modifications

### 5.1 Dependency Fixes

- **Removed** `#include 'Utils.au3'`
- **Added** `#include <FileConstants.au3>`
- `MapFromArrays(...)` renamed to `_Debugger_MapFromArrays(...)` with local implementation
- `GetCharacterName()` calls wrapped with `IsDeclared()` + `Call()` fallback to `'Unknown'`
- `LogCriticalError()` changed from `Debug($msg)` to `ConsoleWrite($msg & @CRLF)`
- `DebuggerReadProcessMemory()` uses `IsDeclared("GetProcessHandle")` with `Call()` fallback

### 5.2 Debug Mode Enabled

- **`$DEBUG_MODE`** changed from `False` to `True`

### 5.3 Logging Improvements

- `DebuggerStartLogging()` — explicitly creates log directory, handles file open failure
- `DebuggerStopLogging()` — resets `$log_handle = -1` after close

### 5.4 Performance Optimization

- `SafeDllCall13()` — parameter `$call` string only built on error or when context logging is enabled, not on every invocation

### 5.5 New Logging Wrapper Functions

| Function | Level |
|----------|-------|
| `Info($msg)` | `[INFO]` |
| `Warn($msg)` | `[WARN]` |
| `Error($msg)` | `[ERROR]` |
| `Debug($msg)` | `[DEBUG]` (only if `$DEBUG_MODE`) |
| `Notice($msg)` | `[NOTICE]` |
| `WarnOnce($msg)` | `[WARN]` |
| `DebuggerOut($msg, $file, $trace)` | Wraps `DebuggerLog()` |

---

## 6. Utils-Storage-Bot.au3 — Storage Bot Modifications

### 6.1 New Function

- **`GoToXunlaiChest($town)`** — travels to outpost, uses city speed boost, navigates to Xunlai chest by name lookup, opens it

### 6.2 NPC Coordinate Updates

| Location | NPC | Old Coordinates | New Coordinates |
|----------|-----|----------------|-----------------|
| Eye of the North | Merchant | 2158, -2006 | 2233, -2009 |
| Eye of the North | Material Trader | 2997, -2271 | 2933, -2236 |
| Eye of the North | Rare Material Trader | 2928, -2452 | 2865, -2406 |

### 6.3 New NPC Coordinates Added

| Location | NPC | Coordinates |
|----------|-----|-------------|
| Gadd's Encampment | Merchant | -8374, -22491 |
| Gadd's Encampment | Basic Material Trader | -9097, -23353 |
| Gadd's Encampment | Rare Material Trader | -9136, -23153 |
| Embark Beach | Edwin (consumable trader) | 3515, 369 |
| Embark Beach | Kwat (consumable trader) | 3596, 107 |
| Embark Beach | Alcus Nailbiter (consumable trader) | 3704, -163 |
| Embark Beach | Eyja (consumable trader, default) | 3336, 627 |
| Embark Beach | Xunlai Chest | 2283, -2134 |
| Gadd's Encampment | Xunlai Chest | -10481, -22787 |

### 6.4 NPCCoordinatesInTown() Enhanced

- Added `$name` parameter for selecting specific NPCs by name
- Added Consumables Trader section supporting named traders at Embark Beach
- Added Xunlai Chest section

### 6.5 Sell Functions Hardened

- `SellMaterials()` and `SellRareMaterials()` now re-read actual quantity from game memory after each sell
- Track `$failCount` — exits loop after 3 consecutive failures with warning
- Increased sleep between operations: `GetPing() + 200` → `GetPing() + 500`

### 6.6 Other Changes

- `TravelToOutpost()` calls — removed `$district_name` parameter (~10 sites)
- `StoreItemsInChest()` — return value changed from `$PAUSE` to bare `Return`
- `DefaultShouldSalvageItem` renamed to `StorageBot_ShouldSalvageItem` (default moved to Utils.au3)
- SQL update bug fix — corrected off-by-one in field index mapping for weapon mod struct updates
- Removed `Info('Balancing character''s gold level')` log message

---

## 7. Utils-Items_Modstructs.au3 — Mod Struct Modifications

### Single Change

- **`$MAP_WEAPON_MODS`** changed from `Global Const` to `Global` (mutable)
- Allows runtime modification of the weapon mods map, needed by the dynamic salvage/mod extraction system

---

## 8. BotsHub.au3 — Launcher Modifications

### 8.1 Bug Fix: Inventory Management Ordering

- **Old order:** Check inventory space → `InventoryManagementBeforeRun()` → Mid-run inventory
- **New order:** Mid-run inventory → Check inventory space → `InventoryManagementBeforeRun()`
- Comment: *"Must do mid-run inventory management before normal one else we will go back to town"*
- Prevents unnecessary town trip when mid-run management could free enough slots

### 8.2 Race Condition Fix

- In UNINITIALIZED state handler: GUI button text/color now set **before** `$runtime_status = 'RUNNING'`
- Prevents main loop from acting on RUNNING status before GUI reflects the state

### 8.3 Log Noise Reduction

- Removed 5 `Info()` calls: `'Initializing...'`, `'Starting...'` (x2), `'Pausing...'`, `'Restarting...'`

---

## 9. Farm-Vaettirs.au3 — Vaettirs Bot Modifications

### 9.1 Gameplay Tuning

- Waypoint array reduced 31 → 30 entries; final approach uses dedicated `MoveTo(12480, -17336, 0)` for precise wall-blocking
- Foe detection range increased 1200 → 1400 to prevent missing stragglers
- Added "prodigy" as alternative insignia option

### 9.2 Logic Fixes

- Title display: two independent if-statements → proper `If/Else/EndIf` block (mutual exclusivity between Monk/non-Monk)
- Dead-player checks: replaced `If IsPlayerDead() Then Return` inside loops with `IsPlayerAlive()` as while-loop condition
- Removed redundant dead-check at end of kill phase

### 9.3 Performance Optimizations

- Shroud of Distress: profession check moved to front of condition (short-circuit)
- Skill casting: `TimerDiff()` checks (cheap) moved before `IsRecharged()` calls (expensive memory reads)
- Applied to: Mantra of Earth, Obsidian Flesh, Balthazar's Aura, Kirin's Wrath, Symbol of Wrath

---

## 10. New Files — GWA Censured Library

These files exist only in `GWA Censured/lib/` with no BotsHub counterpart.

### 10.1 Utils-Maintenance.au3 (~600+ lines)

**Purpose:** Full maintenance automation system — the backbone of long-run bot sustainability.

**Key orchestration function:** `PerformMaintenance($force, $buyConsumables)`
- Travels to maintenance town (Gadd's Encampment by default)
- Claims items from unclaimed bag
- Salvages Amphibian Tongues
- Identifies all unidentified items
- Deposits excess gold
- Sells items and materials
- Buys ID kits and salvage kits
- Stores tomes
- Optionally crafts consumables (consets) at Embark Beach

**Consumable crafting:** `BuyConsumablesInEmbarkBeach()`
- Multi-cycle crafting at Embark Beach
- Navigates to specific traders: Eyja (Grails), Edwin (Powerstones/Scrolls), Kwat (Essences), Alcus (Armor)
- Calculates required materials per craft cycle
- Handles material purchasing from traders

**Low-level crafting:** `CraftItemSafe($modelID, $amount, $gold, $materialsArray)`
- Allocates game-process memory via `VirtualAllocEx`
- Builds material arrays in remote process memory
- Writes TradeID addresses
- Populates `CRAFT_ITEM_STRUCT`
- Enqueues ASM command

**Other key functions:**
- `RunDiagnostics()` — checks inventory slots, kit counts, gold; returns True if maintenance needed
- `BuyMaterialIfMissing()` / `BuyMaterialSafe()` — trader quote/buy loop with auto-refuel
- `GoToMaterialTrader()` / `GoToConsumableTrader()` / `GoToRareMaterialTrader()` — NPC navigation
- `StoreItemsInXunlaiStorageSafe()` — filtered storage with named predicate function
- `BuyEctosWithExcessGold()` — buys Ectos when bank gold exceeds 900k to avoid 1M cap
- `DepositEctosToBank()` — priority deposit: material storage → existing stacks → empty slots
- `CombineItemStacks()` — merges partial stacks (< 250)
- `SalvageAmphibianTongues()` — targeted salvage of specific trophy type
- `ClaimSpecificItem()` — moves items from unclaimed bag (7) to inventory

### 10.2 Utils-Salvage.au3 (~200+ lines)

**Purpose:** Salvage decision logic and item property access via direct memory reads.

**Key functions:**

| Function | Purpose |
|----------|---------|
| `StartSalvage($aItem, $useRareKit)` | Finds best available kit (Superior > Expert > basic) and starts salvage |
| `CanSell($aItem)` | Complex sell filter: blacklists valuable trophies (Destroyer Core, Amphibian Tongue, etc.), protects greens, upgrades, kits, dyes, keys; protects low-req weapons (Req5-9 shields, Req8 offhands/swords); protects rare skins |
| `HasUsefulMod($aItem)` | Checks item's mod struct against `$array_armormods` and `$array_weaponmods` to identify mods worth extracting |
| `GetIsIDed($aItem)` | Reads identification bit from item memory (offset +40) |
| `GetItemPtr($aItem)` | Polymorphic pointer resolver (handles DllStruct, Ptr, Int) |
| `GetQuantity($aItem)` | Reads stack quantity (offset +76) |
| `GetItemValue($aItem)` | Reads item value (offset +36) |
| `IsRareSkinSafe($modelID)` | Bounds-checked lookup into `$aRareSkin` array |

**Item type constants defined:** 20 weapon/item type IDs plus a `$TYPE_ID[12]` lookup array.

### 10.3 GUI_Functions.au3 (~800+ lines)

**Purpose:** Complete GUI framework for the Froggy farming bot.

**Features:**
- Main window (390x450) with Start/Stop/Resume buttons
- Settings group: Add Heroes checkbox, hero config dropdown (reads `hero_configs/*.txt`), Consets, Stones, Open Chests, Pick Up Golds, Salvage
- Console output (scrolling rich-text edit)
- General Statistics panel: Deldrimor/Asura/Norn/Vanguard reputation points, lockpicks, wipes, best/average/total run time
- Drop Statistics panel: rare skins, golds, lockpicks, chests, dyes, tomes
- Status bar with run counter and fail percentage
- File menu: Settings (opens INI in Notepad), Mod Settings (opens ModSelection dialog), Open Dir, Exit
- INI-based settings persistence (`Settings.ini`)
- **ModSelection dialog**: tabbed interface (Popular Mods, Damage, HP/Enchanting, Armor) with 133x11 checkbox grid for weapon type × mod combinations, saved to `Mod_Settings.ini`

**Key functions:** `GUI_Create()`, `GUI_Print()`, `Out()`, `GUI_SetRunCounter()`, `GUI_SetFailCounter()`, `GUI_SetRunTime()`, `ModSelection()`, `SpecialEvents()` (saves mod preferences)

### 10.4 Map_IDs.au3 (~666 lines)

**Purpose:** Comprehensive Guild Wars map ID constant definitions. Covers all campaigns (Prophecies, Factions, Nightfall, Eye of the North), PvP arenas, guild halls, seasonal events, and War in Kryta/Winds of Change content.

**Key IDs for bot usage:** `$Gadds_Encampment = 638`, `$Embark_Beach = 857`, `$Eye_of_the_North_Outpost = 642`, `$Lions_Arch = 55`

### 10.5 Skill_IDs.au3 (~530+ constants)

**Purpose:** Complete skill ID database covering all 10 professions, PvE-only skills, speed buffs, condition/hex/enchantment removal skills, and resurrection skills. Used by hero build loading and combat logic.

### 10.6 RareSkins.au3 (~180 entries)

**Purpose:** Sparse lookup array (`$aRareSkin[60000]`) mapping weapon Model IDs to names for ~180 rare/valuable skins. Categories include shields, swords, axes, staves, scepters, wands, bows, scythes, daggers, and dungeon-specific drops. Many entries are commented out — user has curated which skins they consider worth protecting.

### 10.7 UsefulMods.au3 (~450+ lines)

**Purpose:** Two data tables encoding every salvageable mod in Guild Wars:

- **`$array_weaponmods[133][15]`** — 133 weapon mods (HCT, HSR, energy, damage, elemental, slayer, health, attribute, armor mods) with per-weapon-type salvage flags across 11 weapon types
- **`$array_armormods[183][5]`** — 183 armor mods (all attribute runes Minor/Major/Superior for all 10 professions, all insignias, universal runes) with model IDs and hex mod-struct patterns

Used by `HasUsefulMod()` in Utils-Salvage.au3 to identify mods worth extracting via expert salvage.

### 10.8 Skill_Types.au3 (~24 lines)

**Purpose:** 20 skill type constants (Stance=3, Hex=4, Spell=5, Enchantment=6, Signet=7, Condition=8, Well=9, Skill=10, Ward=11, Glyph=12, Attack=14, Shout=15, Preparation=19, Trap=21, Ritual=22, ItemSpell=24, WeaponSpell=25, Chant=27, EchoRefrain=28, Disguise=29).

---

## 11. Line-Ending-Only Files (No Functional Changes)

The following 38 `src/Farm-*.au3` files and 6 `lib/` files differ **only in line endings** (CRLF vs LF) with zero functional changes:

**Bot scripts (all 38):** Farm-Asuran, Farm-Boreal, Farm-Corsairs, Farm-DragonMoss, Farm-EdenIris, Farm-Feathers, Farm-FoW, Farm-FoWTowerOfCourage, Farm-Follower, Farm-Froggy, Farm-GemstoneMargonite, Farm-GemstoneStygian, Farm-GemstoneTorment, Farm-Gemstones, Farm-GlintChallenge, Farm-JadeBrotherhood, Farm-Kournans, Farm-Kurzick, Farm-LDOA, Farm-Lightbringer, Farm-Lightbringer2, Farm-Luxon, Farm-Mantids, Farm-MinisterialCommendations, Farm-Minotaurs, Farm-NexusChallenge, Farm-Norn, Farm-Pongmei, Farm-Raptors, Farm-SoO, Farm-SpiritSlaves, Farm-SunspearArmor, Farm-Tasca, Farm-TestSuite, Farm-Underworld, Farm-Voltaic, Farm-Vanguard, Farm-WarSupplyKeiran

**Library files (6):** GWA2_Headers.au3, JSON.au3, SQLite.au3, SQLite.dll.au3, Utils-OmniFarmer.au3, Utils-Items_Modstructs.au3 (except the single `Const` removal)
