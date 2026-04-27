// Froggy combat session state. Included by FroggyHM.cpp.

using CachedSkill = DungeonSkill::CachedSkill;
using DungeonSkill::CanUseSkill;
using DungeonSkill::ExplainCanUseSkillFailure;
using DungeonSkill::ResolveSkillTarget;

static DungeonCombatRoutine::CombatSessionState s_combatSession = {};
static SparkflyTraversalCombatStats s_sparkflyTraversalCombatStats = {};
