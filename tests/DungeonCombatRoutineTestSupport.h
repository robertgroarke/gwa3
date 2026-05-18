#pragma once

#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/testing/TestFramework.h>

#include <cstdint>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace EffectStubs = GWA3::TestStubs::EffectMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
namespace SkillStubs = GWA3::TestStubs::SkillMgr;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
void SetPlayerAgent(float x, float y, float hp);
uint32_t ChangeTargetCount();
uint32_t LastAttackTargetId();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::EffectMgr {

void Reset();

} // namespace GWA3::TestStubs::EffectMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetFlags();
void SetPartyDefeated(bool defeated);

} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::TestStubs::SkillMgr {

void Reset();
void SetSkillData(uint32_t skillId, uint32_t type, uint8_t target, uint8_t energyCost,
                  float activation, float aftercast,
                  uint32_t recharge, uint32_t adrenaline);
void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t recharge, uint32_t adrenaline);
uint32_t LastUsedSkillSlot();
uint32_t LastUsedSkillTargetId();

} // namespace GWA3::TestStubs::SkillMgr

namespace GWA3::Tests::DungeonCombatRoutineSupport {

void WaitNoopCombatRoutine(uint32_t ms);
bool NotDead();
void AutoAttack(uint32_t targetId);
GWA3::DungeonCombatRoutine::SkillExecutionContext MakeContext(
    GWA3::DungeonSkill::CachedSkill cache[8],
    bool used[8]);
void ResetCombatRoutineStubs();

} // namespace GWA3::Tests::DungeonCombatRoutineSupport
