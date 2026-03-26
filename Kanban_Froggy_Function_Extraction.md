# Kanban: Froggy Function Extraction

**Date:** 2026-03-25  
**Scope:** implement `Implementation_Plan_Froggy_Function_Extraction.md`  
**Goal:** support parallel execution by multiple agents with explicit dependencies and task ownership

---

## Usage

- Set `Assignee` when an agent claims a ticket.
- Move status between `Backlog`, `Ready`, `In Progress`, `Blocked`, `Review`, `Done`.
- Do not take a ticket unless all items in `Depends on` are `Done`, unless the work is explicitly marked parallel-safe.
- If a task changes shared interfaces, update downstream tickets before marking it `Done`.

Suggested assignee format:

- `agent-1`
- `agent-2`
- `agent-3`
- `unassigned`

Suggested status values:

- `Backlog`
- `Ready`
- `In Progress`
- `Blocked`
- `Review`
- `Done`

---

## Board

| ID | Title | Area | Status | Assignee | Depends on | Parallel-safe | Summary |
|---|---|---|---|---|---|---|---|
| KF-001 | Create custom library skeleton | Infra | Done | agent-1 | none | yes | 8 BotCore module skeletons created in `lib/custom/`. |
| KF-002 | Add shared include aggregator to Froggy | Infra | Done | agent-1 | KF-001 | no | Modules wired into `Froggy_Includes.au3` master include. |
| KF-003 | Define shared naming and state conventions | Infra | Done | agent-2 | none | yes | `CONVENTIONS.md` written with module, function, state, callback, and MemRead rules. |
| KF-004 | Inventory all Froggy globals consumed by extractable functions | Analysis | Done | agent-1 | none | yes | 46 globals audited. Found 3 critical bugs (SkillBarCache OOB, NearestWaypoint corruption, BestTargetPtr side-effect). |
| KF-005 | Define compatibility policy for extracted functions | Infra | Done | agent-1 | KF-003,KF-004 | yes | `COMPATIBILITY_POLICY.md` — names preserved, duplicates expected during migration, dead globals deleted. |
| KF-006 | Create Au3Check validation script/process | Tooling | Done | agent-1 | none | yes | `tests/run_au3check.sh` with baseline 191 and real-error filtering. |
| KF-007 | Create smoke-test checklist for Froggy runtime | Tooling | Done | agent-1 | none | yes | `tests/SMOKE_TEST_CHECKLIST.md` covering launch, travel, combat, loot, maintenance, stats. |
| KF-008 | Extract identity and coordinate helpers | Effects | Done | agent-3 | KF-001,KF-005 | yes | `ID`, `GetX`, `GetY`, `X`, `Y` → BotCore-Effects.au3 (148 lines). |
| KF-009 | Extract health/effect read helpers | Effects | Done | agent-3 | KF-001,KF-004,KF-005 | yes | `GetHP`, `GetHasEnchantment`, `HasEffect`, `GetSkillEffectPtr`, `GetEffectsPtr` → BotCore-Effects.au3. |
| KF-010 | Extract skillbar pointer helpers | Effects | Done | agent-3 | KF-001,KF-004,KF-005 | yes | `GetSkillbarPtr`, `GetAdrenaline` → BotCore-Effects.au3. Depends on `$mBasePointer` global. |
| KF-011 | Replace DP helper family with data-driven utility | Travel | Done | agent-4 | KF-001,KF-005 | yes | 7 DP functions consolidated with named constants → BotCore-Travel.au3 (202 lines). |
| KF-012 | Extract travel wrappers | Travel | Done | agent-4 | KF-001,KF-005 | yes | `TravelTo`, `ResignAndReturn` → BotCore-Travel.au3. |
| KF-013 | Extract inventory counters | Loot | Done | agent-5 | KF-001,KF-005 | yes | `CountFreeSlots`, `GetPicksCount` → BotCore-Loot.au3. |
| KF-014 | Extract skill classification core set | SkillRules | Done | agent-5 | KF-001,KF-005 | yes | Core classifiers → BotCore-SkillRules.au3. |
| KF-015 | Extract skill classification support set | SkillRules | Done | agent-6 | KF-014 | yes | Heal/bond/condition/hex/enchant removal classifiers → BotCore-SkillRules.au3. |
| KF-016 | Extract skill classification advanced set | SkillRules | Done | agent-6 | KF-014 | yes | Pressure/binding/speed/survival/rupt/precast classifiers → BotCore-SkillRules.au3 (453 lines total). |
| KF-017 | Define combat state container | Combat | Done | agent-7 | KF-003,KF-004 | no | SkillBarCache[9][23] (OOB bug FIXED), SkillbarSlot[10000], BestTargetPtr, enum. Dead globals removed. |
| KF-018 | Extract target selection helpers | Combat | Done | agent-9 | KF-008,KF-009,KF-010,KF-014,KF-017 | yes | 6 target selection functions → BotCore-Combat.au3. |
| KF-019 | Extract ally/summon target helpers | Combat | Done | agent-9 | KF-008,KF-009,KF-017 | yes | 6 ally/summon targeting functions → BotCore-Combat.au3. |
| KF-020 | Extract combat state readers | Combat | Done | agent-7 | KF-008,KF-009,KF-010,KF-017 | yes | Wipe, GetPartyHealth, GetNumberOfEnemies, IsKnocked, AgentHasEffect → BotCore-Combat.au3. |
| KF-021 | Extract skillbar caching | Combat | Done | agent-10 | KF-010,KF-014,KF-015,KF-016,KF-017 | no | CacheSkillBar → BotCore-Combat.au3. |
| KF-022 | Extract cast/use decision engine | Combat | Done | agent-10 | KF-018,KF-019,KF-020,KF-021 | no | CanCast, CanAttack, CanUse, GetBestTargetBySkillSlot → BotCore-Combat.au3. |
| KF-023 | Extract active combat execution | Combat | Done | agent-10 | KF-018,KF-019,KF-020,KF-021,KF-022 | no | UseSkillSmart, UseSkills, Fight → BotCore-Combat.au3 (900 lines total). |
| KF-024 | Add Froggy compatibility wrappers for combat subsystem | Combat | In Progress | agent-13 | KF-021,KF-022,KF-023 | no | Analysis in progress — checking if wrappers needed. |
| KF-025 | Define loot policy callback interface | Loot | Done | agent-11 | KF-003,KF-004 | no | Policy documented in BotCore-Loot.au3 with FROGGY-SPECIFIC markers. |
| KF-026 | Refactor pickup predicate into engine plus policy | Loot | Done | agent-11 | KF-013,KF-025 | no | CanPickUpEx extracted with policy sections marked → BotCore-Loot.au3. |
| KF-027 | Extract loot pickup engine | Loot | Done | agent-11 | KF-013,KF-025,KF-026 | no | PickupLootEx → BotCore-Loot.au3 (318 lines total). |
| KF-028 | Extract chest detection/open flow | Loot | Done | agent-11 | KF-013,KF-025,KF-026 | no | CheckForChest → BotCore-Loot.au3. |
| KF-029 | Define waypoint action callback interface | Waypoints | In Progress | agent-12 | KF-003,KF-004 | no | Extraction in progress. |
| KF-030 | Extract nearest-waypoint helper | Waypoints | Done | agent-8 | KF-001,KF-005 | yes | GetNearestWaypointIndex → BotCore-Waypoints.au3. |
| KF-031 | Refactor movement step engine from AggroMoveToEX | Waypoints | In Progress | agent-12 | KF-023,KF-025,KF-029 | no | Extraction in progress. |
| KF-032 | Refactor generic waypoint runner from MoveandAggroEx | Waypoints | In Progress | agent-12 | KF-028,KF-029,KF-030,KF-031 | no | Extraction in progress. |
| KF-033 | Convert WipeManagement into Froggy route policy callback | Waypoints | In Progress | agent-12 | KF-029,KF-032 | no | Extraction in progress. |
| KF-034 | Define run-stats state container and callback interface | Stats | Done | agent-8 | KF-003,KF-004 | yes | State globals defined in BotCore-RunStats.au3. |
| KF-035 | Extract runtime timers and calculations | Stats | Done | agent-8 | KF-001,KF-034 | yes | AvgRunTime, BestRunTime, CurrentRunTime, TotalRunTime → BotCore-RunStats.au3 (88 lines). |
| KF-036 | Extract GUI stat update bridge | Stats | In Progress | agent-13 | KF-034,KF-035 | no | Analysis in progress. |
| KF-037 | Evaluate `LoadHeroConfigFromFile` for shared extraction | HeroSetup | Done | agent-1 | KF-001,KF-005 | yes | Extracted to BotCore-HeroSetup.au3 (88 lines). Generic config loader, reusable. |
| KF-038 | Create post-phase integration branch/checkpoint plan | Process | Done | agent-1 | none | yes | `tests/INTEGRATION_CHECKPOINTS.md` with 7 merge gates. |
| KF-039 | Integrate low-risk extraction batch into Froggy | Integration | Ready | unassigned | KF-008,KF-009,KF-010,KF-011,KF-012,KF-013 | no | All deps done. Remove duplicates from Froggy. |
| KF-040 | Integrate skill classification batch into Froggy | Integration | Ready | unassigned | KF-014,KF-015,KF-016,KF-039 | no | All deps done. Remove duplicates from Froggy. |
| KF-041 | Integrate combat subsystem batch into Froggy | Integration | Ready | unassigned | KF-017,KF-018,KF-019,KF-020,KF-021,KF-022,KF-023,KF-024,KF-040 | no | Deps done except KF-024 (in progress). |
| KF-042 | Integrate loot subsystem batch into Froggy | Integration | Ready | unassigned | KF-025,KF-026,KF-027,KF-028,KF-041 | no | All extraction deps done. |
| KF-043 | Integrate waypoint subsystem batch into Froggy | Integration | Blocked | unassigned | KF-029,KF-030,KF-031,KF-032,KF-033,KF-042 | no | Waiting on waypoint extraction (in progress). |
| KF-044 | Integrate stats subsystem batch into Froggy | Integration | Ready | unassigned | KF-034,KF-035,KF-036,KF-043 | no | Deps done except KF-036 (in progress). |
| KF-045 | Full Froggy regression pass | QA | Backlog | unassigned | KF-044,KF-006,KF-007 | no | Run Au3Check plus manual smoke checks across launch, setup, route, combat, looting, wipe recovery. |
| KF-046 | Cleanup dead code and duplicate local wrappers | Cleanup | Backlog | unassigned | KF-045 | no | Remove old Froggy-embedded implementations after shared modules are verified. |
| KF-047 | Update docs with new module map | Docs | Backlog | unassigned | KF-046 | yes | Update implementation docs and dependency notes to reflect extracted library ownership. |

---

## Suggested Parallel Workstreams

### Workstream A: Foundations

- KF-001
- KF-003
- KF-004
- KF-005
- KF-006
- KF-007
- KF-038

### Workstream B: Low-risk helper extraction

- KF-008
- KF-009
- KF-010
- KF-011
- KF-012
- KF-013
- KF-039

### Workstream C: Skill rules

- KF-014
- KF-015
- KF-016
- KF-040

### Workstream D: Combat subsystem

- KF-017
- KF-018
- KF-019
- KF-020
- KF-021
- KF-022
- KF-023
- KF-024
- KF-041

### Workstream E: Loot subsystem

- KF-025
- KF-026
- KF-027
- KF-028
- KF-042

### Workstream F: Waypoint subsystem

- KF-029
- KF-030
- KF-031
- KF-032
- KF-033
- KF-043

### Workstream G: Stats and hero setup

- KF-034
- KF-035
- KF-036
- KF-037
- KF-044

### Workstream H: QA and cleanup

- KF-045
- KF-046
- KF-047

---

## Critical Dependency Notes

### Do first

- KF-001
- KF-003
- KF-004
- KF-006
- KF-007
- KF-038

### Blocks most downstream extraction

- KF-005 blocks clean compatibility decisions for early tickets.
- KF-017 blocks meaningful combat extraction.
- KF-025 blocks safe loot extraction.
- KF-029 blocks safe waypoint extraction.
- KF-034 blocks safe stats extraction.

### Integration order must remain sequential

Even if implementation happens in parallel, these integration tickets should be merged in this order:

1. KF-039
2. KF-040
3. KF-041
4. KF-042
5. KF-043
6. KF-044
7. KF-045
8. KF-046
9. KF-047

---

## Agent Claim Template

Copy/paste for each ticket claim:

```md
- ID: KF-000
- Assignee: agent-x
- Status: In Progress
- Start date: YYYY-MM-DD
- Notes: short scope statement
```

---

## Recommended Initial Assignments

If you want to start immediately with multiple agents, the safest first wave is:

- Agent 1: KF-001 + KF-002
- Agent 2: KF-003 + KF-005
- Agent 3: KF-004
- Agent 4: KF-006 + KF-007
- Agent 5: KF-008
- Agent 6: KF-009
- Agent 7: KF-010
- Agent 8: KF-011 + KF-012
- Agent 9: KF-013
- Agent 10: KF-014

That gives parallel forward motion without creating early merge conflicts in the same files.
