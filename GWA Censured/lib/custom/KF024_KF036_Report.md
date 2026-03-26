# KF-024 / KF-036 Compatibility Analysis Report

## KF-024: Froggy Combat Compatibility Wrappers

### Methodology
Compared every combat function call site in `Froggy_HM_v1.6.au3` against the
function signatures already extracted into `lib/custom/BotCore-Combat.au3`.

### Function-by-function comparison

| Function | Froggy signature | BotCore-Combat signature | Match? |
|---|---|---|---|
| `Fight()` | `($aAggroRange = 1000, $careful = False)` | `($aAggroRange = 1000, $careful = False)` | EXACT |
| `UseSkills()` | `($aAggroRange = 1000, $skilltype = $all)` | `($aAggroRange = 1000, $skilltype = $all)` | EXACT |
| `UseSkillSmart()` | `($aSkillSlot, $aTarget = -2, $aTimeout = 6000, $aSkillbarPtr = 0)` | `($aSkillSlot, $aTarget = -2, $aTimeout = 6000, $aSkillbarPtr = 0)` | EXACT |
| `CanCast()` | `($aSkillSlot = 0)` | `($aSkillSlot = 0)` | EXACT |
| `CanAttack()` | `($aRange = 1320)` | `($aRange = 1320)` | EXACT |
| `CanUse()` | `($aSkillSlot, $aAggroRange = 1320)` | `($aSkillSlot, $aAggroRange = 1320)` | EXACT |
| `CacheSkillBar()` | `()` | `()` | EXACT |
| `GetBestTargetPtr()` | `($aRange = 1350, $casting = False, $nohex = False, $enchanted = False)` | `($aRange = 1350, $casting = False, $nohex = False, $enchanted = False)` | EXACT |
| `GetBestTargetBySkillSlot()` | `($aSkillSlot, $aAggroRange = 1320)` | `($aSkillSlot, $aAggroRange = 1320)` | EXACT |
| `GetLowestAlly()` | `($excludeself = False)` | `($excludeself = False)` | EXACT |
| `GetNumberOfEnemies()` | `($aRange = 1200)` | `($aRange = 1200)` | EXACT |
| `GetPartyHealth()` | `($aParty = GetParty())` | `($aParty = GetParty())` | EXACT |
| `Wipe()` | `($aPartyPtr = GetParty())` | `($aPartyPtr = GetParty())` | EXACT |
| `GetBestMeleeTarget()` | `($aRange = 250)` | `($aRange = 250)` | EXACT |
| `GetNoHexEnemy()` | `($aRange = 1320)` | `($aRange = 1320)` | EXACT |
| `GetBalledEnchantedEnemy()` | `($aRange)` | `($aRange)` | EXACT |
| `GetMostBalledCastingEnemy()` | `($aRange = 1320)` | `($aRange = 1320)` | EXACT |
| `IsKnocked()` | `()` (implicit `$aAgent = -2`) | `($aAgent = -2)` | EXACT |
| `AgentHasEffect()` | `($aSkillID, $aAgentID = -2)` | `($aSkillID, $aAgentID = -2)` | EXACT |
| `GetNearestSpiritPtrToAgent()` | `($aAgent = -2)` | `($aAgent = -2)` | EXACT |
| `GetNearestMinionPtrToAgent()` | `($aAgent = -2)` | `($aAgent = -2)` | EXACT |
| `GetNearestDeadAllyPtrToAgent()` | `($aAgent = -2)` | `($aAgent = -2)` | EXACT |
| `NeedEchoAlly()` | `($aSkillID)` | `($aSkillID)` | EXACT |
| `MostCondsAllyPtr()` | `($excludeself = False)` | `($excludeself = False)` | EXACT |
| `MostHexedAllyPtr()` | `($excludeself = False)` | `($excludeself = False)` | EXACT |

### Call sites verified

- `Fight($aFightRange)` at line 827 -- 1-arg call, matches default `$careful = False`.
- `CacheSkillBar()` at lines 154, 425, 472 -- 0-arg call, matches.
- `Wipe()` at lines 609, 651, 841, 1399, 1405, 1415 -- 0-arg call, uses default `GetParty()`.
- `UseSkills($aAggroRange, $all)` at line 1397 -- 2-arg call, matches.
- `UseSkillSmart($aSkillSlot, $BestTargetPtr)` at line 1407 -- 2-arg call, matches.
- `CanCast($aSkillSlot)` and `CanCast()` at lines 1377, 1415, 1449, 1458 -- all match.
- `CanAttack($aAggroRange)` at line 1388 -- 1-arg call, matches.
- `CanUse($aSkillSlot, $aAggroRange)` at line 1407 -- 2-arg call, matches.
- `GetBestTargetPtr($aRange)` at lines 1447, 1557, 1565, 1594, 1598, 1602 -- 1-4 arg calls, all match.
- `GetBestTargetBySkillSlot($aSkillSlot, $aAggroRange)` at line 1459 -- 2-arg call, matches.
- `GetLowestAlly()` and `GetLowestAlly(True)` at lines 1509, 1537, 1551 -- match.
- `GetNumberOfEnemies($aAggroRange)` at line 1469 -- 1-arg call, matches.
- `GetPartyHealth()` -- not directly called from combat loop (used in GUI), signature matches.

### Function body comparison

The function bodies in BotCore-Combat.au3 are character-for-character identical to the
Froggy originals, with two intentional improvements already applied:

1. **`$SkillBarCache[9][23]`** (BotCore) vs `$SkillBarCache[9][20]` (Froggy line 1302).
   BotCore correctly sizes the array to match the 23-member enum. This is a bug fix, not
   a mismatch.

2. **`$SkillbarSlot[3500]`** (BotCore) vs `$SkillbarSlot[10000]` (Froggy line 1303).
   BotCore uses the tighter bound. No functional difference (skill IDs max ~3400).

3. **`$PressureSpiritSkills = 0`** reset in Froggy's CacheSkillBar (line 1631) is absent
   in BotCore. This variable was identified as dead code in KF-004 and deleted.

4. **`$Skillbar[9]`** declared in Froggy (line 1301) is absent in BotCore.
   Also identified as dead code in KF-004 and deleted.

### Conclusion for KF-024

**No compatibility wrappers are needed.** All function signatures match exactly between
Froggy call sites and BotCore-Combat.au3 definitions. The integration step is:

1. Remove the duplicate function definitions from Froggy (lines 1275-1659 plus
   the globals at lines 1301-1303).
2. Ensure `Froggy_Includes.au3` includes `BotCore-Combat.au3`.
3. The bare global `$BestTargetPtr` used throughout Froggy will continue to work
   because BotCore-Combat.au3 still writes it as a bare global for compatibility
   (migration to `SetBestTarget()` accessor is deferred).

---

## KF-036: GUI Stat Update Bridge

### Findings

`UpdateStats()` was already extracted to `lib/custom/BotCore-RunStats.au3` (lines 66-72).

**Froggy version** (line 1027-1033):
```autoit
Func UpdateStats()
    GUI_SetVanguard(GetVanguardTitle() - $iVanguardTitle)
    GUI_SetNorn(GetNornTitle() - $iNornTitle)
    GUI_SetAsura(GetAsuraTitle() - $iAsuraTitle)
    GUI_SetDeldrimor(GetDeldrimorTitle() - $iDeldrimorTitle)
    GUI_SetLockpicks(GetPicksCount())
EndFunc
```

**BotCore-RunStats.au3 version** (lines 66-72):
```autoit
Func UpdateStats()
    GUI_SetVanguard(GetVanguardTitle() - $iVanguardTitle)
    GUI_SetNorn(GetNornTitle() - $iNornTitle)
    GUI_SetAsura(GetAsuraTitle() - $iAsuraTitle)
    GUI_SetDeldrimor(GetDeldrimorTitle() - $iDeldrimorTitle)
    GUI_SetLockpicks(GetPicksCount())
EndFunc
```

**Exact match.** The extracted version is identical.

### GUI dependency analysis

`UpdateStats()` calls five `GUI_Set*` functions:
- `GUI_SetVanguard()`
- `GUI_SetNorn()`
- `GUI_SetAsura()`
- `GUI_SetDeldrimor()`
- `GUI_SetLockpicks()`

These are Froggy-specific GUI update functions (defined in `GUI_Functions.au3`). They
write to GUICtrl handles that are created by Froggy's main GUI. This is correct --
`UpdateStats()` is a bridge function that connects generic game-state queries
(`GetVanguardTitle()` etc.) to Froggy's specific GUI controls.

The title baseline globals (`$iVanguardTitle`, `$iNornTitle`, `$iAsuraTitle`,
`$iDeldrimorTitle`) are set during `onStart()` in Froggy and read by `UpdateStats()`.
They are declared in Froggy's main script (line 42) and also referenced in
BotCore-RunStats.au3 via the comment on line 27.

### Also extracted (confirmed matching)

- `AvgRunTime()` -- extracted to BotCore-RunStats.au3, matches Froggy.
- `BestRunTime()` -- extracted to BotCore-RunStats.au3, matches Froggy.
- `CurrentRunTime()` -- extracted to BotCore-RunStats.au3, matches Froggy.
- `TotalRunTime()` -- extracted to BotCore-RunStats.au3, matches Froggy.

### Conclusion for KF-036

**No bridge code is needed.** `UpdateStats()` is already correctly extracted to
BotCore-RunStats.au3 with an identical implementation. The `GUI_Set*` functions it
calls are Froggy-specific and correctly remain linked to Froggy's GUI through
`GUI_Functions.au3` (already in the include chain).

The integration step is:
1. Remove the duplicate `UpdateStats()`, `CurrentRunTime()`, `TotalRunTime()`,
   `AvgRunTime()`, and `BestRunTime()` from Froggy.
2. Remove the duplicate state globals (`$nBestRunTime`, `$nCurrentRunTime`,
   `$nTotalRunTime`, `$CumulatedTime`, `$AvgRunTime`, `$bRunFailed`) from Froggy
   (lines 33, 44-46), since BotCore-RunStats.au3 declares them.
3. Ensure `Froggy_Includes.au3` includes `BotCore-RunStats.au3`.
