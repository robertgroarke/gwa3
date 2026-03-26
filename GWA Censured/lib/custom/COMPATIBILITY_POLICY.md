# KF-005: Compatibility Policy for Extracted Functions

**Scope:** Rules for how extracted functions relate to their original Froggy call sites during and after migration.

---

## Core Rules

### 1. Function names do NOT change during extraction

When moving a function from Froggy to a BotCore module, keep the **exact same name and signature**. Froggy call sites must work without modification after the include chain brings in the module.

**Do:**
```autoit
; In BotCore-Effects.au3:
Func GetHP($agent)     ; same name, same params
```

**Don't:**
```autoit
; Don't rename during extraction:
Func BotCore_GetHP($agent)   ; breaks all callers
```

### 2. Duplicate definitions are expected during migration

During extraction, a function will temporarily exist in BOTH the BotCore module AND in Froggy. Au3Check will report "already defined" warnings. This is expected and resolves at the integration step (KF-039 through KF-044) when the duplicate is removed from Froggy.

**Migration sequence per function:**
1. Copy function to BotCore module (duplicate exists)
2. Au3Check: +1 "already defined" warning (expected)
3. Integration ticket: delete function from Froggy
4. Au3Check: warning disappears

### 3. Globals move WITH their primary consumer

When a global is consumed by multiple functions being extracted to the same module, move the global declaration to that module. Keep the same name — AutoIt globals are file-independent.

When a global is consumed by functions in DIFFERENT modules, keep it in Froggy until all consumers are extracted, then move it to the module that writes it most.

### 4. Dead globals get deleted, not moved

The KF-004 audit identified 8 dead globals. Delete them during integration, don't extract them:
- `$gReturnMap`, `$district_name`, `$unlit`, `$Summon_Spirits`
- `$Skillbar[9]`, `$PressureSpiritSkills`
- `$HERO_ID_MERCENARY_1/2/3`, `$DIALOG_ID_TEKKS_WAR_ACCEPT`

### 5. Bug fixes happen IN the extracted module

The KF-004 audit found 3 critical bugs. Fix them in the BotCore module, not in Froggy:
- **SkillBarCache OOB**: Fix size to [9][23] in BotCore-Combat.au3
- **$NearestWaypoint global mutation**: Convert to Local in BotCore-Waypoints.au3
- **$BestTargetPtr side-effect**: Document the coupling, fix in BotCore-Combat.au3

### 6. MemoryRead → MemRead conversion happens during extraction

Every `MemoryRead(` call in extracted code becomes `MemRead(` (from GWA2_Compat.au3). This is mandatory per CONVENTIONS.md.

### 7. Froggy-specific functions stay in Froggy

Functions that reference Bogroot-specific data (waypoint arrays, quest IDs, dialog constants, dungeon-specific flow) stay in Froggy. The BotCore modules should be dungeon-agnostic.

**Stay in Froggy:**
- `BogrootLvl1`, `BogrootLvl2`, `Boss`, `TakeQuest0`
- `WipeManagement` (Bogroot-specific restart logic)
- `onStart`, `onStop`, `onResume` (GUI callbacks)
- Waypoint array definitions

**Move to BotCore:**
- `MoveandAggroEx` → generic waypoint engine (with Bogroot policy as callback)
- `Fight`, `UseSkills`, `UseSkillSmart` → combat engine
- `PickupLootEx`, `CheckForChest` → loot engine
- All `Is*Skill` classifiers → skill rules

---

## Integration Ticket Checklist

For each integration ticket (KF-039 through KF-044):

- [ ] Remove extracted functions from Froggy
- [ ] Remove extracted globals from Froggy (if moved to module)
- [ ] Delete identified dead globals
- [ ] Run `bash tests/run_au3check.sh`
- [ ] Verify "already defined" warnings decreased by expected count
- [ ] Verify zero new real errors
- [ ] Git commit with ticket ID in message
