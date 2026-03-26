# Integration Checkpoints

Merge in this order. Each checkpoint must pass Au3Check before proceeding.

## Checkpoint 1: Foundation (KF-001 through KF-007)
- Module skeletons created
- Conventions defined
- Globals inventoried
- Validation tooling ready
- **Gate:** `bash tests/run_au3check.sh` passes

## Checkpoint 2: Low-Risk Helpers (KF-008 through KF-013, KF-039)
- Effects helpers extracted (ID, GetX/Y, GetHP, etc.)
- Travel wrappers extracted
- Inventory counters extracted
- DP helpers consolidated
- **Gate:** Au3Check passes + Froggy functions still resolve

## Checkpoint 3: Skill Rules (KF-014 through KF-016, KF-040)
- Skill classification extracted to BotCore-SkillRules.au3
- All IsXxxSkill functions moved
- **Gate:** Au3Check passes

## Checkpoint 4: Combat Subsystem (KF-017 through KF-024, KF-041)
- Combat state container defined
- Target selection extracted
- Skillbar caching extracted
- Cast/use engine extracted
- Fight loop extracted
- Froggy compatibility wrappers in place
- **Gate:** Au3Check passes + combat function call chain intact

## Checkpoint 5: Loot Subsystem (KF-025 through KF-028, KF-042)
- Loot policy interface defined
- Pickup engine extracted
- Chest flow extracted
- **Gate:** Au3Check passes

## Checkpoint 6: Waypoint Subsystem (KF-029 through KF-033, KF-043)
- Waypoint engine extracted from MoveandAggroEx
- WipeManagement as callback
- **Gate:** Au3Check passes

## Checkpoint 7: Stats + Cleanup (KF-034 through KF-037, KF-044 through KF-047)
- Run stats extracted
- Dead code removed
- Docs updated
- **Gate:** Full regression pass
