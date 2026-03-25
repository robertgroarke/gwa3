# BotsHub Changelog: Commit 4cb3dd5 → Latest (47b3348)

**Original commit:** `4cb3dd5` (2026-01-11) — "fix: Grenth Aura not coming out in Commendations farm"
**Latest commit:** `47b3348` (2026-03-20) — "fix: Kournans farm broken due to extra If"
**Total commits between versions:** ~130
**Overall diff:** 80 files changed, 34,383 insertions, 18,580 deletions

---

## Table of Contents

1. [Architecture & Organization](#1-architecture--organization)
2. [Core Library Changes (lib/)](#2-core-library-changes-lib)
3. [New Library Files](#3-new-library-files)
4. [BotsHub.au3 (Main Launcher)](#4-botshubau3-main-launcher)
5. [Configuration Changes](#5-configuration-changes)
6. [New Bots](#6-new-bots)
7. [Bot Scripts — Major Rewrites](#7-bot-scripts--major-rewrites)
8. [Bot Scripts — Significant Reworks](#8-bot-scripts--significant-reworks)
9. [Bot Scripts — Moderate Changes](#9-bot-scripts--moderate-changes)
10. [Cross-Cutting Changes (All Bots)](#10-cross-cutting-changes-all-bots)
11. [File Mapping (Old → New)](#11-file-mapping-old--new)

---

## 1. Architecture & Organization

### Directory Restructure

The flat `src/Farm-*.au3` structure was reorganized into categorized subdirectories:

| Category | Directory | Files |
|---|---|---|
| Combat farms | `src/farms/` | 19 |
| Missions/Dungeons | `src/missions/` | 11 |
| Chest/Speed runs | `src/runs/` | 3 |
| Title grinding | `src/titles/` | 1 |
| Utilities | `src/utilities/` | 3 |
| Vanquishes | `src/vanquishes/` | 6 |

### Library Modularization

The monolithic library files were split into focused modules:

- **GWA2.au3** (6662 → 2826 lines): All ASM/memory code extracted to `GWA2_Assembly.au3`
- **GWA2_ID.au3** (2349 → 457 lines): IDs split into `GWA2_ID_Items.au3`, `GWA2_ID_Maps.au3`, `GWA2_ID_Quests.au3`, `GWA2_ID_Skills.au3`
- **Utils.au3** (3545 → 2759 lines): Agent functions moved to `Utils-Agents.au3`, storage functions to `Utils-Storage.au3`
- **GUI extracted**: All GUI code moved from `BotsHub.au3` into `lib/BotsHub-GUI.au3`
- **Utils-OmniFarmer.au3** relocated from `lib/` to `src/utilities/OmniFarmer.au3`
- **Utils-Storage-Bot.au3** replaced by expanded `Utils-Storage.au3`

---

## 2. Core Library Changes (lib/)

### GWA2.au3 — Game API Layer

After extraction of ASM code, this file is now purely the high-level game API.

**New functions added:**
- `DestroyItem($item)` — item destruction capability
- `GetMapCampaign`, `GetMapRegion`, `GetMapRegionType` — map metadata queries
- `GetAreaInfoByID($mapID)` — area info lookup
- `GetItemByFilter($filterFunction, $filterParameter)` — generic filtered item search with predicates
- `GetRarity($item)`, `IsIdentified`, `IsUnidentified`, `IsUnidentifiedGoldItem` — item state queries
- `GetItemReq`, `GetItemAttribute`, `GetModByIdentifier`, `GetModStruct` — item mod inspection
- `GetCastTimeModifier` — cast time calculation with effects
- `GetWeaponAttackTime` — weapon speed lookup
- `CallTargetOnce($target)` — single-shot target call

**Functions moved out:** `MoveTo`, `GoToNPC`, `GoToSignpost`, `GoToAgent`, `ZoneMap`, `AcceptQuest`, `QuestReward`, `AbandonQuest`, all Toggle* UI functions, `EnableRendering`, `DisableRendering` — moved to Utils.au3 or Utils-Agents.au3

### GWA2_Headers.au3

- New headers: `$HEADER_SKILL_EQUIP` (0x3C), `$HEADER_ATTRIBUTE_LOAD` (0x10)
- New guild headers: `$HEADER_BUY_GUILD_CAPE`, `$HEADER_SHOW_HIDE`, `$HEADER_GUILD_ANNOUCEMENTS`, `$HEADER_GUILD_SET_OFFICER`
- `$HEADER_TRADE_CANCEL_OFFER` renamed to `$HEADER_TRADE_CHANGE_OFFER`

### Utils.au3 — Core Utility Functions

**New quest system:**
- `IsQuestActive`, `IsQuestCompleted`, `IsQuestNotFound`, `IsQuestPartiallyCompleted`, `IsQuestPrimary`, `IsQuestReward`, `IsQuestSecondary`, `QuestStateMatches` — comprehensive quest state checking
- `TakeQuest`, `TakeQuestReward` — dedicated quest interaction helpers
- `GetQuestEncryptedObjectives`, `GetQuestEncryptedObjectivesPtr` — quest data extraction

**New movement/combat:**
- `MoveAggroAndKillSafeTraps` — trap-safe combat movement variant
- `FlagMoveAggroAndKill` — hero-flagged movement + combat
- `IsPlayerStuck($minMovement, $stuckTicks, $reset)` — stuck detection system

**New party management:**
- `InvitePlayer($playerName)` — party invitation
- `Resign()` — resignation command
- `ResignAndReturnToOutpost($outpostID, $ignoreMapID)` — improved with explicit outpost parameter

**New utility functions:**
- `GetIsPointInPolygon($areaCoordinates, $X, $Y)` — polygon containment check
- `GetNearestItemByModelIDToAgent` — targeted item search
- `ComputeDistance($X1, $Y1, $X2, $Y2)` — standalone distance calculation
- `ConvertTimeToHourString`, `ConvertTimeToMinutesString` — time formatting
- `SafeEval($variableName)` — safe variable evaluation
- `BuildStructureOffsetsMap`, `ComputeStructureOffsetsMap` — struct helpers
- `AddToMapFromArrays`, `CloneMap`, `CloneDictMap` — map utility functions
- ASM helpers: `ASMNumber`, `RegisterNameTo8/16/32Code`, `FloatToInt`, `StringToAsciiBinary`, etc.
- Memory helpers: `WriteBinary`, `StructMemoryRead`, `FindInRange`, `ScanMemoryForPattern`, etc.

**Signature changes:**
- `RandomDistrictTravel($mapID, $fromDistrict, $toDistrict)` — new signature
- `TravelToOutpost` — default district now `'Random'`
- `DistrictTravel` — default now `'Random EU'`
- `RandomSleep` — new `$randomFactor` parameter
- `TakeQuestOrReward` — now uses `$statePredicate` function parameter instead of `$expectedState` integer
- `FanFlagHeroes` — new default range 250

### Utils-Items_Modstructs.au3

Major rework of item valuation system:
- `ContainsValuableUpgrades` and `HasPerfectMods` rewritten
- New inscription-by-weapon-type map system replaces flat arrays
- `RefreshValuableListsFromCache()` replaces `RefreshValuableListsFromInterface()`
- New functions: `IsWeaponUpgradeStruct`, `IsInscriptionStruct`, `TrimCleanupAndEval`
- `DefaultCreateValuableInscriptionsByWeaponTypeMap()` and `DefaultCreatePerfectModsByOSWeaponTypeMap()` replace hardcoded approaches

### Utils-Debugger.au3

- Memory protection check simplified to single bitmask operation
- `VirtualQueryEx` error handling improved
- `$ERROR_CODES` made `Const`

---

## 3. New Library Files

### GWA2_Assembly.au3 (3,962 lines)

Core engine extracted from GWA2.au3. Contains all low-level memory interaction:
- Process scanning and client management
- ASM code injection and pattern scanning
- All DLL struct templates (AGENT, BUFF, EFFECT, SKILLBAR, SKILL, ATTRIBUTE, BAG, etc.)
- Game command assemblers for skills, items, trading, maps, UI, party, crafting, salvage
- **New capabilities:** PE section parsing, `SafeEnqueue`, `AssemblerCreateCollectorExchangeCommand`, `AssemblerCreateRequestQuoteCommand`, `AssemblerCreateLoadFinished`, `AssemblerCreateFriendCommands`, `AssemblerCreateAttributeCommands`, `AssemblerCreateRenderingMod`

### GWA2_Assembly_Chatlog.au3 (249 lines)

Chat log interception via ASM injection. Hooks into the game's chat receive function and copies messages to shared memory buffer. Uses Windows `PostMessage` for GUI notification.

### BotsHub-GUI.au3 (1,895 lines)

Complete GUI framework with:
- Main window, tabs (main, run options, loot options, farm infos, team options)
- JSON-based configuration persistence
- Treeview-driven inventory management settings
- Multi-level logging console (`Debug`, `Info`, `Notice`, `Warn`, `WarnOnce`, `Error`, `Out`)

### Build_PW_Heroic-Refrain.au3 (215 lines)

Automated Paragon "Heroic Refrain" skill maintenance system. Manages shout rotation across party members. Designed for `AdlibRegister` on 11-15s timer. Automatically casts HR on party members missing the buff.

### GWA2_ID_Items.au3 (1,877 lines)

Item ID database with type IDs, weapon type arrays, max damage per level tables, and all item model IDs. Includes `$WEAPONS_MAX_DAMAGE_PER_LEVEL` map and `$MAP_ARMOR_TYPES`/`$MAP_WEAPON_TYPES`.

### GWA2_ID_Maps.au3 (2,778 lines)

Complete map database with campaign IDs, continent IDs, region types, region IDs, and all map location IDs.

### GWA2_ID_Quests.au3 (4,251 lines)

Every quest in Guild Wars with numeric IDs. Includes quest state constants (`NOT_FOUND`, `ACTIVE`, `REWARD`, `PARTIAL`, `CURRENT`, `PRIMARY`, `AREA_PRIMARY`).

### GWA2_ID_Skills.au3 (3,484 lines)

Complete skill ID database organized by profession. Includes weapon type constants and composite masks (`$ID_SKILL_MELEE_WEAPON`, `$ID_SKILL_RANGED_WEAPON`).

### Utils-Agents.au3 (858 lines)

Agent/party utility functions:
- Distance calculations: `GetDistance`, `GetDistanceToPoint`, `GetPseudoDistance`
- Party state: `CountAliveHeroes/PartyMembers`, `IsPlayerAlive/Dead`, `IsHeroAlive/Dead`, `IsPlayerAndPartyWiped`, `IsRunFailed`, `HasRezMemberAlive`
- Agent searching: `CountFoesInRangeOfAgent/Coords`, `GetFoesInRangeOfAgent/Coords`, `GetNearestNPCInRangeOfCoords`
- Party positioning: `GetIntoTeamRange`, `MoveToMiddleOfPartyWithTimeout`, `FindMiddleOfParty`, `FindMiddleOfFoes`

### Utils-Storage.au3 (2,692 lines)

Expanded inventory/storage management (replaces `Utils-Storage-Bot.au3`):
- Full inventory flow: `InventoryManagementBeforeRun`, `InventoryManagementMidRun`
- Item operations: identification, salvage, selling, buying, storage
- Database-driven item tracking via SQLite
- Kit management with remaining-use tracking
- Predicate-based item filtering: `DefaultShouldPickItem`, `DefaultShouldSalvageItem`, `DefaultShouldSellItem`, `DefaultShouldStoreItem`

---

## 4. BotsHub.au3 (Main Launcher)

### Key Changes

- **GUI extracted** to `lib/BotsHub-GUI.au3` — launcher is now logic-focused
- **Headless mode added** — supports running without GUI via command-line arguments for multi-instance orchestration
- **Farm dispatch refactored** — giant `Switch/Case` block replaced with data-driven `$farm_map` dictionary populated by `FillFarmMap()`
- **Configuration decoupled** — `$run_options_cache` map centralizes all run options; loot config loaded from separate JSON file
- **Per-hero build loading** — new `load_player_build`, `load_hero_N_build` flags control individual build template loading
- **Default district** changed from `"Random"` to `"Random EU"`; added `"Random US"`, `"Random Asia"` options
- **Heroes list** now alphabetically sorted with empty-string entries for "no hero" selection
- **`$STUCK` constant removed** from codebase

### Include Changes

| Removed | Added |
|---|---|
| All GUI-related AutoIt includes | `lib/GWA2_Assembly.au3` |
| `lib/Utils-OmniFarmer.au3` | `lib/Utils-Agents.au3` |
| `lib/Utils-Storage-Bot.au3` | `lib/Utils-Storage.au3` |
| `lib/JSON.au3` | `lib/BotsHub-GUI.au3` |

---

## 5. Configuration Changes

### Farm Configuration (Default Farm Configuration.json)

**New fields:**
- `main.loot_configuration` — links to separate loot config file
- `run.use_consets` — toggle for consumable set usage
- `team.load_all_builds`, `team.load_player_build`, `team.load_hero_1_build` through `team.load_hero_7_build` — granular build loading

**Removed fields:**
- `run.store_unids`, `run.store_leftovers`, `run.store_gold` — moved to loot config
- `run.buy_ectoplasm`, `run.buy_obsidian` — moved to loot config
- `run.save_weapon_slot` — replaced by `weapon_slot=0` meaning disabled

**Changed defaults:**
- `run.weapon_slot`: `1` → `0` (disabled)
- `run.district`: `"Random"` → `"Random EU"`

### Loot Configuration (Default Loot Configuration.json)

**New pickup options:**
- `"Gold": true`, `"Consumables": true`, `"Alcohols": true` — explicit top-level toggles

**Gold weapons per-requirement granularity:**
- Gold weapons expanded from simple boolean (`"Axe": true`) to per-requirement objects (`"Axe": {"Req 0": true, ..., "Req 13": true}`)

**New "Armor salvageables" section:**
- `"Blue"`, `"Purple"`, `"Gold"` sub-options (all default `false`)

**New "Trophies" section (under Pick Up Items):**
- 17 specific trophies with individual toggles (Gemstones, Drake Flesh, Skale Fin, Iboga Petal, Destroyer Core, Glacial Stone, Saurian Bone, Jade Bracelet, etc.)
- Nicholas trophy protection — trophies on Nicholas the Traveler's list are no longer salvaged unless explicitly ticked

**New "Dyes" section (under Sell Items):**
- Individual dye colors with sell toggles (Black/White kept by default, others sold)

---

## 6. New Bots

### Cathedral of Flames (src/farms/CoF.au3) — 215 lines
- **Author:** DeeperBlue
- **Class:** Dervish solo
- **Farms:** Golden Rin Relics, Diessa Chalices, bones
- **Location:** Cathedral of Flames dungeon

### Deldrimor Title Farm (src/missions/Deldrimor.au3) — 207 lines
- **Author:** Ian
- **Type:** Dungeon repeat with Snowman quest
- **Duration:** 10-20min per run
- **Farms:** Deldrimor title points

### Kurzick Drazach Thicket (src/vanquishes/Kurzick2.au3) — 177 lines
- **Author:** Ian
- **Type:** Alternative Kurzick faction farm (Drazach Thicket vs. Ferndale)

### Heroic Refrain Paragon Build (lib/Build_PW_Heroic-Refrain.au3) — 215 lines
- **Type:** Automated shout/refrain maintenance system
- **Works with:** Multiple existing bots that support paragon player

---

## 7. Bot Scripts — Major Rewrites

### Underworld (missions/Underworld.au3) — 356 → 1,649 lines (4.6x)

- Added co-author BuddyLeeX
- **Player build system:** Added skillbar templates for Rt/A and A/Rt builds with spirit skills
- **Configurable reaper quest system:** Individual toggles for 9 quests (Wrathful Spirits, Servants of Grenth, Four Horsemen, Terrorweb Queen, Imprisoned Spirits, Demon Assassin, Escort of Souls, Unwanted Guests, Nightman Cometh)
- **Old:** Only farmed initial Chamber area
- **New:** Can clear most/all of UW with dynamic quest routing
- **Duration:** 60min → 90min (150min with all quests)
- Player profession detection and build loading added

### Boreal Chest Run (runs/Boreal.au3) — Complete Class Change

- **Old:** Shadow Form Assassin
- **New:** Dervish chest runner with Pious Renewal/Pious Haste
- Optional skill detection at runtime (`$boreal_has_shroud_of_distress`, `$boreal_has_heart_of_shadow`)
- Health-based emergency shadow step and enemy detection
- Chest-finding strategy overhauled: counts all chests in compass range first, then opens iteratively
- New `BorealUnblock()` function and enemy model ID constants
- Run timeout constant added

---

## 8. Bot Scripts — Significant Reworks

### FoW Tower of Courage (farms/FoWTowerOfCourage.au3)

- Skillbar changed: Heart of Shadow → Death's Charge
- Farm strategy substantially reworked with new waypoints and killing logic
- Ranger-killing strategy completely rewritten using `GetFurthestNPCInRangeOfCoords` and `FindMiddleOfFoes`
- Added 30-second timer tracking
- Defend function renamed and reworked

### Gemstone Stygian (farms/GemstoneStygian.au3)

- Ranger hero skillbar changed with new template
- Trap-laying timing completely rewritten: replaced per-skill `IsRecharged()` wait loops with calculated fixed timing
- Added wave number parameter to mesmer/assassin job
- Aggro range increased 1200 → 1300

### Glint's Challenge (missions/GlintChallenge.au3)

- Duration: 40min → 30min
- **Glitch detection:** Timer tracks if fewer than 3 foes in compass range for 2.5 minutes, triggers `SweepAroundBabyDragonLocation()` to handle stuck foes
- Fight loop restructured for better death detection

### LDOA (titles/LDOA.au3)

- **Multi-profession support:** Dynamically determines dialog IDs based on primary profession (Mesmer, Necromancer, Elementalist, Monk, Warrior, Ranger) instead of hardcoded Elementalist-only
- Fight options map with priority mobs and no chest opening
- Updated info text describing actual leveling strategy
- `GetWeapons` → `GetBonusWeapons`

### FoW (missions/FoW.au3)

- Added hero flagging during combat
- Quest interactions rewritten using `TakeQuest()` / `TakeQuestReward()` helpers
- Now uses Legionnaire Summoning Crystal + Consets multiple times during the run
- Added `SetDisplayedTitle($ID_ASURA_TITLE)`
- Combat pathing and coordinates updated

### Lightbringer-Sunspear (farms/Lightbringer-Sunspear.au3)

- Renamed from `LightbringerFarm` to `LightbringerSunspearFarm`
- Duration: 25min → 18min
- Pathing updated with new waypoints
- Hardmode conset support added

---

## 9. Bot Scripts — Moderate Changes

| Bot | Key Changes |
|---|---|
| **Froggy** | Quest IDs changed from hex to named constants; quest interaction rewritten with `TakeQuest`/`TakeQuestReward` helpers; title display added |
| **SoO** | Quest interaction rewritten; map IDs renamed (`$ID_VLOXS_FALL` → `$ID_VLOXS_FALLS`); title display added |
| **Corsairs** | Duration 3min → 2min 15s; second hero now configurable; performance metrics comment added |
| **Kournans** | Hero indices now configurable constants; fixed syntax bug (`If Not ... And If` → `If Not ... And`); quest IDs to constants |
| **Vaettirs** | Coordinates changed float → integer; dynamic array sizing; kill spot repositioned |
| **Feathers** | Removed entire unused `MoveRun()` function; cleaned up redundant death checks |
| **Tasca** | OmniFarmer include path updated; ping caching in defend function |
| **Boreal** | (See Major Rewrites above) |
| **Follower** | Added explorable-area guard; variable casing changes |
| **GemstoneMargonite** | Quest interaction using helpers; `CommandAll` for hero movement |
| **Lightbringer** | Map ID renames; pathing simplified; hardmode conset |
| **Luxon** | Faction management refactored; `TakeFactionBlessing('luxon')` helper; 2 extra Oni waypoints; hardmode conset |
| **Kurzick** | Faction management refactored; `TakeFactionBlessing('kurzick')` helper; early vanquish exit; hardmode conset |
| **Mantids** | Hero skillbar changed; new template; added BraceYourself hero skill |

---

## 10. Cross-Cutting Changes (All Bots)

These patterns appear across nearly every bot script:

1. **Team setup refactored** — `TrySetupPlayerUsingGUISettings()` / `TrySetupTeamUsingGUISettings()` replaced with `IsTeamAutoSetup()` early-return or inline party-size validation
2. **Return to outpost** — `ReturnBackToOutpost()` → `ResignAndReturnToOutpost($MAP_ID)` with explicit map ID
3. **Sleep standardization** — `Sleep(N + GetPing())` → `RandomSleep(N)`; minimum raised from 20ms to 50ms
4. **Array modernization** — Fixed-size `$foes[N][M]` → dynamic `$foes[][]`
5. **Group killing** — `MoveAggroAndKillGroups($foes, start, end)` replaced with explicit `For` loop using `MoveAggroAndKillInRange`
6. **Variable naming** — PascalCase → camelCase throughout
7. **Loot pickup simplified** — Removed "tripled to secure looting" `For $i = 1 To 3` loops; single `PickUpItems()` call
8. **Quest IDs** — Magic hex values replaced with named constants (e.g., `0x1C9` → `$ID_QUEST_MISSING_DAUGHTER`)
9. **Map ID renames** — e.g., `$ID_NEXUS` → `$ID_THE_SHADOW_NEXUS`, `$ID_GADDS_CAMP` → `$ID_GADDS_ENCAMPMENT`
10. **Hardmode conset usage** — Many bots now have `If IsHardmodeEnabled() Then UseConset()` before combat
11. **String contractions expanded** — `don't` → `do not`, `doesn't` → `does not`, etc.
12. **`$STUCK` constant removed** — Return values changed from `$STUCK` to `$FAIL`

---

## 11. File Mapping (Old → New)

### Farms (src/farms/)

| Original | Latest |
|---|---|
| `src/Farm-Corsairs.au3` | `src/farms/Corsairs.au3` |
| `src/Farm-DragonMoss.au3` | `src/farms/DragonMoss.au3` |
| `src/Farm-EdenIris.au3` | `src/farms/EdenIris.au3` |
| `src/Farm-Feathers.au3` | `src/farms/Feathers.au3` |
| `src/Farm-FoWTowerOfCourage.au3` | `src/farms/FoWTowerOfCourage.au3` |
| `src/Farm-GemstoneMargonite.au3` | `src/farms/GemstoneMargonite.au3` |
| `src/Farm-GemstoneStygian.au3` | `src/farms/GemstoneStygian.au3` |
| `src/Farm-GemstoneTorment.au3` | `src/farms/GemstoneTorment.au3` |
| `src/Farm-Gemstones.au3` | `src/farms/Gemstones.au3` |
| `src/Farm-JadeBrotherhood.au3` | `src/farms/JadeBrotherhood.au3` |
| `src/Farm-Kournans.au3` | `src/farms/Kournans.au3` |
| `src/Farm-Lightbringer.au3` | `src/farms/Lightbringer-Sunspear.au3` ⚠️ |
| `src/Farm-Lightbringer2.au3` | `src/farms/Lightbringer.au3` ⚠️ |
| `src/Farm-Mantids.au3` | `src/farms/Mantids.au3` |
| `src/Farm-Minotaurs.au3` | `src/farms/Minotaurs.au3` |
| `src/Farm-Raptors.au3` | `src/farms/Raptors.au3` |
| `src/Farm-SpiritSlaves.au3` | `src/farms/SpiritSlaves.au3` |
| `src/Farm-Vaettirs.au3` | `src/farms/Vaettirs.au3` |
| *(none)* | `src/farms/CoF.au3` **NEW** |

⚠️ The two Lightbringer scripts **swapped names** during the rename.

### Missions (src/missions/)

| Original | Latest |
|---|---|
| `src/Farm-FoW.au3` | `src/missions/FoW.au3` |
| `src/Farm-Froggy.au3` | `src/missions/Froggy.au3` |
| `src/Farm-GlintChallenge.au3` | `src/missions/GlintChallenge.au3` |
| `src/Farm-MinisterialCommendations.au3` | `src/missions/MinisterialCommendations.au3` |
| `src/Farm-NexusChallenge.au3` | `src/missions/NexusChallenge.au3` |
| `src/Farm-SoO.au3` | `src/missions/SoO.au3` |
| `src/Farm-SunspearArmor.au3` | `src/missions/SunspearArmor.au3` |
| `src/Farm-Underworld.au3` | `src/missions/Underworld.au3` |
| `src/Farm-Voltaic.au3` | `src/missions/Voltaic.au3` |
| `src/Farm-WarSupplyKeiran.au3` | `src/missions/WarSupplyKeiran.au3` |
| *(none)* | `src/missions/Deldrimor.au3` **NEW** |

### Runs (src/runs/)

| Original | Latest |
|---|---|
| `src/Farm-Boreal.au3` | `src/runs/Boreal.au3` |
| `src/Farm-Pongmei.au3` | `src/runs/Pongmei.au3` |
| `src/Farm-Tasca.au3` | `src/runs/Tasca.au3` |

### Titles (src/titles/)

| Original | Latest |
|---|---|
| `src/Farm-LDOA.au3` | `src/titles/LDOA.au3` |

### Utilities (src/utilities/)

| Original | Latest |
|---|---|
| `src/Farm-Follower.au3` | `src/utilities/Follower.au3` |
| `src/Farm-TestSuite.au3` | `src/utilities/TestSuite.au3` |
| `lib/Utils-OmniFarmer.au3` | `src/utilities/OmniFarmer.au3` |

### Vanquishes (src/vanquishes/)

| Original | Latest |
|---|---|
| `src/Farm-Asuran.au3` | `src/vanquishes/Asuran.au3` |
| `src/Farm-Kurzick.au3` | `src/vanquishes/Kurzick.au3` |
| `src/Farm-Luxon.au3` | `src/vanquishes/Luxon.au3` |
| `src/Farm-Norn.au3` | `src/vanquishes/Norn.au3` |
| `src/Farm-Vanguard.au3` | `src/vanquishes/Vanguard.au3` |
| *(none)* | `src/vanquishes/Kurzick2.au3` **NEW** |

---

## Summary

The ~130 commits between your original pull and the latest version represent a substantial evolution:

- **3 new bots** (CoF, Deldrimor, Kurzick2) + Heroic Refrain paragon build system
- **2 major rewrites** (Underworld expanded 4.6x, Boreal changed class entirely)
- **10 new library files** from modularization of the codebase
- **Complete directory reorganization** from flat to categorized structure
- **GUI extracted** to standalone module with headless mode support
- **Configuration system** expanded with per-requirement weapon filtering, trophy management, armor salvage options, dye selling, and granular build loading
- **Assembly backend** extracted and expanded with new game command assemblers
- **Quest system** overhauled with comprehensive state checking and named constants
- **Stuck detection**, conset automation, and improved error handling across all bots
