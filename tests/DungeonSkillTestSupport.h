#pragma once

#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/managers/AgentMgr.h>
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

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::EffectMgr {

void Reset();
void AddEffect(uint32_t skillId);
void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration = 10.0f);

} // namespace GWA3::TestStubs::EffectMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetFlags();
void SetPartyDefeated(bool defeated);

} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::TestStubs::SkillMgr {

void Reset();
void SetSkillData(uint32_t skillId, uint32_t type, uint8_t target, uint8_t energyCost,
                  float activation = 0.0f, float aftercast = 0.0f,
                  uint32_t recharge = 0u, uint32_t adrenaline = 0u);
void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t recharge = 0u, uint32_t adrenaline = 0u);

} // namespace GWA3::TestStubs::SkillMgr
