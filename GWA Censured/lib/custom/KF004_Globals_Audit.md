# KF-004: Froggy Global Variable Audit

**File:** `Froggy_HM_v1.6.au3`
**Total globals:** 46 (29 mutable, 14 const, 8 dead/unused, 11 local candidates)

---

## Combat State

| Global | Read By | Written By | Notes |
|---|---|---|---|
| `$BestTargetPtr` | Fight, UseSkills, UseSkillSmart, CanUse | CanAttack, GetBestTargetBySkillSlot | Side-effect mutation in query function |
| `$SkillBarCache[9][23]` | UseSkillSmart, UseSkills, CanCast, CanUse, GetBestTargetBySkillSlot | CacheSkillBar | **BUG: re-declared as [9][20] at line 1302** |
| `$SkillbarSlot[3500]` | UseSkillSmart, CanUse | CacheSkillBar | Re-declared as [10000] at line 1303 |
| `$Skillbar[9]` | **DEAD CODE** | never | Delete |
| `$PressureSpiritSkills` | **never read** | CacheSkillBar (write-only) | Delete |
| Enum `$all..$echoes` (0..22) | All SkillBarCache consumers | const | 23 members but cache re-declared with only 20 cols — **OOB bug** |

## Run State

| Global | Read By | Written By |
|---|---|---|
| `$BotRunning` | Main loop | onStart, onStop, onResume |
| `$GUI_RunCounter` | RunToDungeon, AvgRunTime, TakeQuest0 | TakeQuest0 via GUI_SetRunCounter |
| `$GUI_FailCounter` | AvgRunTime | (set via GUI lib) |
| `$nBestRunTime` | BestRunTime | BestRunTime |
| `$nCurrentRunTime` | AvgRunTime, BestRunTime, CurrentRunTime | TakeQuest0 (TimerInit) |
| `$nTotalRunTime` | TotalRunTime | onStart (TimerInit) |
| `$bRunFailed` | BestRunTime | TakeQuest0, MoveandAggroEx |
| `$CumulatedTime` | AvgRunTime | AvgRunTime (local candidate) |
| `$AvgRunTime` | (output only) | AvgRunTime (local candidate) |

## Loot State

| Global | Read By | Written By |
|---|---|---|
| `$OpenedChestAgentIDs[1]` | CheckForChest | Main loop (reset), CheckForChest (_ArrayAdd) |
| `$gPickupCoins` | CanPickUpEx | (declaration only) |

## Config / GUI State

| Global | Notes |
|---|---|
| `$BOTNAME`, `$VERSION`, `$AUTHORS` | Constants for GUI |
| `$Form1`, `$Combo_Label`, `$Char_Combo`, `$Launch_Button` | GUI control IDs |
| `$g_BotHasLaunched` | Launch gate |
| `$Character_Select` | Local candidate — only used in LaunchEvent |

## Travel / Map State

| Global | Notes |
|---|---|
| `$Outpost` (638) | Gadds Encampment map ID |
| `$NearestWaypoint` | **Should be Local** — written by 3 functions, corruption risk |
| `$LastWaypoint` | Local candidate — only in MoveandAggroEx |
| Quest/dialog constants (10) | Read-only |
| `$gReturnMap`, `$district_name`, `$unlit` | **DEAD — delete** |
| `$HERO_ID_MERCENARY_*` | **DEAD — delete** |

## GWA2 API State

| Global | Notes |
|---|---|
| `$mBasePointer` | Set in LaunchEvent, read by GetSkillEffectPtr, GetEffectsPtr, GetSkillbarPtr |
| `$iVanguardTitle`, `$iNornTitle`, `$iAsuraTitle`, `$iDeldrimorTitle` | Title tracking |
| `$Summon_Spirits` | **DEAD — delete** |

## Critical Bugs Found

1. **SkillBarCache OOB**: Enum has 23 members but cache re-declared as [9][20] — indices 20-22 are out of bounds
2. **$NearestWaypoint global mutation**: 3 functions write the same global — corruption risk on re-entry
3. **$BestTargetPtr side-effect**: Query function GetBestTargetBySkillSlot mutates global as side-effect
