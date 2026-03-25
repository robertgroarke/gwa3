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
| KF-001 | Create custom library skeleton | Infra | Ready | unassigned | none | yes | Create `GWA Censured/lib/custom/` and initial placeholder modules plus shared include aggregator. |
| KF-002 | Add shared include aggregator to Froggy | Infra | Backlog | unassigned | KF-001 | no | Add `lib/Froggy_Shared.au3` and wire Froggy to include it without behavior changes. |
| KF-003 | Define shared naming and state conventions | Infra | Ready | unassigned | none | yes | Write conventions for module prefixes, state containers, callbacks, and compatibility wrappers before parallel extraction begins. |
| KF-004 | Inventory all Froggy globals consumed by extractable functions | Analysis | Ready | unassigned | none | yes | Produce a definitive list of global state used by low-level, combat, loot, travel, and stats helpers. |
| KF-005 | Define compatibility policy for extracted functions | Infra | Backlog | unassigned | KF-003,KF-004 | yes | Decide which legacy function names remain as wrappers and which are renamed immediately. |
| KF-006 | Create Au3Check validation script/process | Tooling | Ready | unassigned | none | yes | Add a repeatable validation command or helper script for Froggy and new custom libraries. |
| KF-007 | Create smoke-test checklist for Froggy runtime | Tooling | Ready | unassigned | none | yes | Document the manual smoke checks required after each merged extraction phase. |
| KF-008 | Extract identity and coordinate helpers | Effects | Backlog | unassigned | KF-001,KF-005 | yes | Move `ID`, `GetX`, `GetY`, `X`, `Y` into `BotCore-Effects.au3` with compatibility wrappers. |
| KF-009 | Extract health/effect read helpers | Effects | Backlog | unassigned | KF-001,KF-004,KF-005 | yes | Move `GetHP`, `GetHasEnchantment`, `HasEffect`, `GetSkillEffectPtr`, `GetEffectsPtr` into `BotCore-Effects.au3`. |
| KF-010 | Extract skillbar pointer helpers | Effects | Backlog | unassigned | KF-001,KF-004,KF-005 | yes | Move `GetSkillbarPtr`, `GetAdrenaline`, and any required wrapper around `GetSkillPtr` into `BotCore-Effects.au3`. |
| KF-011 | Replace DP helper family with data-driven utility | Travel | Backlog | unassigned | KF-001,KF-005 | yes | Replace `usedp`, `usedp9`, `Usedp1`-`Usedp5` with one reusable helper in `BotCore-Travel.au3`. |
| KF-012 | Extract travel wrappers | Travel | Backlog | unassigned | KF-001,KF-005 | yes | Move `TravelTo` and `ResignAndReturn` into `BotCore-Travel.au3`, preserving Froggy behavior. |
| KF-013 | Extract inventory counters | Loot | Backlog | unassigned | KF-001,KF-005 | yes | Move `CountFreeSlots` and `GetPicksCount` into shared library code. |
| KF-014 | Extract skill classification core set | SkillRules | Backlog | unassigned | KF-001,KF-005 | yes | Move generic classifiers like `IsSkillType`, `IsWeaponRange`, `IsHexSpell`, `IsConditionSpell`, `IsDisguiseskill`. |
| KF-015 | Extract skill classification support set | SkillRules | Backlog | unassigned | KF-014 | yes | Move heal/bond/condition/hex/enchant removal classification helpers. |
| KF-016 | Extract skill classification advanced set | SkillRules | Backlog | unassigned | KF-014 | yes | Move pressure/binding/speed/survival/rupt/precast classification helpers. |
| KF-017 | Define combat state container | Combat | Backlog | unassigned | KF-003,KF-004 | no | Define ownership for `SkillBarCache`, `SkillbarSlot`, `BestTargetPtr`, `PressureSpiritSkills`, enum indices, and initialization flow. |
| KF-018 | Extract target selection helpers | Combat | Backlog | unassigned | KF-008,KF-009,KF-010,KF-014,KF-017 | yes | Move `GetBestTargetPtr`, `GetBestMeleeTarget`, `GetLowestAlly`, `GetNoHexEnemy`, `GetBalledEnchantedEnemy`, `GetMostBalledCastingEnemy`. |
| KF-019 | Extract ally/summon target helpers | Combat | Backlog | unassigned | KF-008,KF-009,KF-017 | yes | Move `GetNearestSpiritPtrToAgent`, `GetNearestMinionPtrToAgent`, `GetNearestDeadAllyPtrToAgent`, `NeedEchoAlly`, `MostCondsAllyPtr`, `MostHexedAllyPtr`. |
| KF-020 | Extract combat state readers | Combat | Backlog | unassigned | KF-008,KF-009,KF-010,KF-017 | yes | Move `Wipe`, `GetPartyHealth`, `GetNumberOfEnemies`, `IsKnocked`, `AgentHasEffect`. |
| KF-021 | Extract skillbar caching | Combat | Backlog | unassigned | KF-010,KF-014,KF-015,KF-016,KF-017 | no | Move `CacheSkillBar` into `BotCore-Combat.au3` using the new combat state container. |
| KF-022 | Extract cast/use decision engine | Combat | Backlog | unassigned | KF-018,KF-019,KF-020,KF-021 | no | Move `CanCast`, `CanAttack`, `CanUse`, and `GetBestTargetBySkillSlot`. |
| KF-023 | Extract active combat execution | Combat | Backlog | unassigned | KF-018,KF-019,KF-020,KF-021,KF-022 | no | Move `UseSkillSmart`, `UseSkills`, and `Fight`. |
| KF-024 | Add Froggy compatibility wrappers for extracted combat subsystem | Combat | Backlog | unassigned | KF-021,KF-022,KF-023 | no | Keep Froggy call sites stable while moving the implementation into shared modules. |
| KF-025 | Define loot policy callback interface | Loot | Backlog | unassigned | KF-003,KF-004 | no | Define interfaces for `ShouldPickItem`, stat update hooks, chest-open behavior, and post-pickup hooks. |
| KF-026 | Refactor pickup predicate into engine plus policy | Loot | Backlog | unassigned | KF-013,KF-025 | no | Split `CanPickUpEx` into generic reusable rules and Froggy-specific policy/config. |
| KF-027 | Extract loot pickup engine | Loot | Backlog | unassigned | KF-013,KF-025,KF-026 | no | Move `PickupLootEx` into `BotCore-Loot.au3` and route script-specific behavior through callbacks. |
| KF-028 | Extract chest detection/open flow | Loot | Backlog | unassigned | KF-013,KF-025,KF-026 | no | Move `CheckForChest` into `BotCore-Loot.au3` with configurable behavior. |
| KF-029 | Define waypoint action callback interface | Waypoints | Backlog | unassigned | KF-003,KF-004 | no | Define generic waypoint labels/actions, wipe restart callback, and special node handling. |
| KF-030 | Extract nearest-waypoint helper | Waypoints | Backlog | unassigned | KF-001,KF-005 | yes | Move `GetNearestWaypointIndex` into `BotCore-Waypoints.au3`. |
| KF-031 | Refactor movement step engine from AggroMoveToEX | Waypoints | Backlog | unassigned | KF-023,KF-025,KF-029 | no | Split `AggroMoveToEX` into reusable movement/fight/loot step logic plus Froggy policy. |
| KF-032 | Refactor generic waypoint runner from MoveandAggroEx | Waypoints | Backlog | unassigned | KF-028,KF-029,KF-030,KF-031 | no | Extract generic waypoint traversal and action dispatch from `MoveandAggroEx`. |
| KF-033 | Convert WipeManagement into Froggy route policy callback | Waypoints | Backlog | unassigned | KF-029,KF-032 | no | Keep Bogroot-specific restart behavior outside the generic waypoint module. |
| KF-034 | Define run-stats state container and callback interface | Stats | Backlog | unassigned | KF-003,KF-004 | yes | Define state structure and GUI callback hooks for runtime tracking. |
| KF-035 | Extract runtime timers and calculations | Stats | Backlog | unassigned | KF-001,KF-034 | yes | Move `AvgRunTime`, `BestRunTime`, `CurrentRunTime`, `TotalRunTime` into `BotCore-RunStats.au3`. |
| KF-036 | Extract GUI stat update bridge | Stats | Backlog | unassigned | KF-034,KF-035 | no | Refactor `UpdateStats` to use run-stats state plus GUI callback hooks. |
| KF-037 | Evaluate `LoadHeroConfigFromFile` for shared extraction | HeroSetup | Backlog | unassigned | KF-001,KF-005 | yes | Decide whether to keep it in Froggy or move it into `BotCore-HeroSetup.au3`. |
| KF-038 | Create post-phase integration branch/checkpoint plan | Process | Ready | unassigned | none | yes | Define merge checkpoints so parallel agents integrate in safe batches instead of one huge merge. |
| KF-039 | Integrate low-risk extraction batch into Froggy | Integration | Backlog | unassigned | KF-008,KF-009,KF-010,KF-011,KF-012,KF-013 | no | Merge and validate Phase 1 extractions together. |
| KF-040 | Integrate skill classification batch into Froggy | Integration | Backlog | unassigned | KF-014,KF-015,KF-016,KF-039 | no | Merge and validate skill-rule extraction with Froggy call sites. |
| KF-041 | Integrate combat subsystem batch into Froggy | Integration | Backlog | unassigned | KF-017,KF-018,KF-019,KF-020,KF-021,KF-022,KF-023,KF-024,KF-040 | no | Merge and validate combat extraction. |
| KF-042 | Integrate loot subsystem batch into Froggy | Integration | Backlog | unassigned | KF-025,KF-026,KF-027,KF-028,KF-041 | no | Merge and validate loot/chest extraction. |
| KF-043 | Integrate waypoint subsystem batch into Froggy | Integration | Backlog | unassigned | KF-029,KF-030,KF-031,KF-032,KF-033,KF-042 | no | Merge and validate waypoint extraction. |
| KF-044 | Integrate stats subsystem batch into Froggy | Integration | Backlog | unassigned | KF-034,KF-035,KF-036,KF-043 | no | Merge and validate run-stats extraction. |
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
