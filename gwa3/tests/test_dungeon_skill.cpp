#include <gwa3/bot/DungeonSkill.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/testing/TestFramework.h>

#include <cstring>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace EffectStubs = GWA3::TestStubs::EffectMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
namespace SkillStubs = GWA3::TestStubs::SkillMgr;

namespace GWA3::TestStubs::AgentMgr {
void ResetAgents();
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
void SetPlayerAgent(float x, float y, float hp);
}

namespace GWA3::TestStubs::EffectMgr {
void Reset();
void AddEffect(uint32_t skillId);
void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration = 10.0f);
}

namespace GWA3::TestStubs::PartyMgr {
void ResetFlags();
void SetPartyDefeated(bool defeated);
}

namespace GWA3::TestStubs::SkillMgr {
void Reset();
void SetSkillData(uint32_t skillId, uint32_t type, uint8_t target, uint8_t energyCost,
                  float activation = 0.0f, float aftercast = 0.0f,
                  uint32_t recharge = 0u, uint32_t adrenaline = 0u);
void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t recharge = 0u, uint32_t adrenaline = 0u);
}

using namespace GWA3::Bot::DungeonSkill;

GWA3_TEST(dungeon_skill_build_skill_cache_classifies_common_roles, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    SkillStubs::Reset();

    SkillStubs::SetSkillData(1239u, 13u, 0u, 10u, 2.0f, 0.75f, 20u, 0u);
    SkillStubs::SetSkillData(332u, 9u, 5u, 0u, 0.0f, 0.0f, 0u, 5u);
    SkillStubs::SetSkillbarSkill(1u, 1239u);
    SkillStubs::SetSkillbarSkill(2u, 332u);

    CachedSkill cache[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));
    GWA3_ASSERT_EQ(cache[0].skill_id, 1239u);
    GWA3_ASSERT(cache[0].hasRole(ROLE_BINDING));
    GWA3_ASSERT(cache[0].hasRole(ROLE_PRESSURE));
    GWA3_ASSERT(cache[0].hasRole(ROLE_PRECAST));
    GWA3_ASSERT_EQ(cache[1].skill_id, 332u);
    GWA3_ASSERT(cache[1].hasRole(ROLE_ATTACK));
    GWA3_ASSERT(cache[1].hasRole(ROLE_OFFENSIVE));
});

GWA3_TEST(dungeon_skill_target_resolution_prefers_contextual_targets, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    EffectStubs::Reset();
    SkillStubs::Reset();

    AgentStubs::AddNpc(2u, 100.0f, 0.0f, 1u, 0.40f);
    AgentStubs::AddNpc(3u, 150.0f, 0.0f, 1u, 0.0f);
    AgentStubs::AddNpc(10u, 600.0f, 0.0f, 3u, 1.0f);
    AgentStubs::AddNpc(11u, 500.0f, 0.0f, 3u, 1.0f);
    AgentStubs::AddNpc(12u, 200.0f, 0.0f, 3u, 1.0f);
    AgentStubs::AddNpc(13u, 700.0f, 0.0f, 3u, 1.0f);

    static_cast<GWA3::AgentLiving*>(GWA3::AgentMgr::GetAgentByID(11u))->skill = 777u;
    static_cast<GWA3::AgentLiving*>(GWA3::AgentMgr::GetAgentByID(13u))->hex = 1u;

    SkillStubs::SetSkillData(900u, 3u, 3u, 5u, 1.0f, 0.5f, 15u, 0u);
    EffectStubs::AddEffectForAgent(10u, 900u, 12.0f);

    CachedSkill heal = {};
    heal.roles = ROLE_HEAL_SINGLE;
    heal.target_type = 3u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(heal, 0u), 2u);

    CachedSkill selfTarget = {};
    selfTarget.roles = ROLE_PRECAST | ROLE_BINDING;
    selfTarget.target_type = 0u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(selfTarget, 99u), 1u);

    CachedSkill resurrect = {};
    resurrect.roles = ROLE_RESURRECT;
    resurrect.target_type = 6u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(resurrect, 0u), 3u);

    CachedSkill interrupt = {};
    interrupt.roles = ROLE_INTERRUPT_HARD;
    interrupt.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(interrupt, 99u), 11u);

    CachedSkill enchantRemove = {};
    enchantRemove.roles = ROLE_ENCHANT_REMOVE;
    enchantRemove.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(enchantRemove, 99u), 10u);

    CachedSkill attack = {};
    attack.roles = ROLE_ATTACK;
    attack.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(attack, 99u), 12u);

    CachedSkill hex = {};
    hex.roles = ROLE_HEX;
    hex.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(hex, 99u), 12u);
});

GWA3_TEST(dungeon_skill_can_cast_reports_party_and_effect_blocks, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    EffectStubs::Reset();
    PartyStubs::ResetFlags();
    SkillStubs::Reset();

    CachedSkill spell = {};
    spell.skill_id = 500u;
    spell.slot = 0u;
    spell.skill_type = 2u;
    spell.roles = ROLE_OFFENSIVE;

    GWA3_ASSERT(ExplainCanCastFailure(spell) == nullptr);
    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->skill = 777u;
    me->model_state = 0x645u;
    GWA3_ASSERT(ExplainCanCastFailure(spell) == nullptr);
    SkillStubs::SetSkillbarSkill(1u, spell.skill_id, 100u, 0u);
    GWA3_ASSERT(std::strcmp(ExplainCanCastFailure(spell), "recharging") == 0);
    SkillStubs::SetSkillbarSkill(1u, spell.skill_id, 0u, 0u);
    EffectStubs::AddEffect(11u);
    GWA3_ASSERT(std::strcmp(ExplainCanCastFailure(spell), "diversion") == 0);
    GWA3_ASSERT(!CanCast(spell));

    EffectStubs::Reset();
    PartyStubs::SetPartyDefeated(true);
    GWA3_ASSERT(std::strcmp(ExplainCanCastFailure(spell), "party_defeated") == 0);
});

GWA3_TEST(dungeon_skill_can_use_skill_applies_common_gates, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 0.95f);
    EffectStubs::Reset();
    PartyStubs::ResetFlags();
    SkillStubs::Reset();

    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->energy = 1.0f;
    me->max_energy = 10u;
    me->skill = 777u;

    CachedSkill heal = {};
    heal.roles = ROLE_HEAL_SINGLE;
    heal.skill_type = 2u;
    heal.target_type = 3u;
    GWA3_ASSERT(!CanUseSkill(heal, 0u));
    me->skill = 0u;

    CachedSkill survival = {};
    survival.skill_id = 2358u;
    survival.roles = ROLE_SURVIVAL;
    survival.skill_type = 3u;
    survival.target_type = 0u;
    GWA3_ASSERT(!CanUseSkill(survival, 0u));

    me->hp = 0.25f;
    GWA3_ASSERT(CanUseSkill(survival, 0u));
    EffectStubs::AddEffectForAgent(me->agent_id, 2358u, 8.0f);
    GWA3_ASSERT(!CanUseSkill(survival, 0u));

    EffectStubs::Reset();
    CachedSkill precast = {};
    precast.skill_id = 4000u;
    precast.roles = ROLE_PRECAST;
    precast.skill_type = 0u;
    precast.target_type = 0u;
    EffectStubs::AddEffectForAgent(me->agent_id, 4000u, 6.0f);
    GWA3_ASSERT(!CanUseSkill(precast, 0u));

    EffectStubs::Reset();
    CachedSkill energyGate = {};
    energyGate.skill_id = 5000u;
    energyGate.roles = ROLE_OFFENSIVE;
    energyGate.skill_type = 2u;
    energyGate.target_type = 5u;
    energyGate.energy_cost = 15u;
    GWA3_ASSERT(!CanUseSkill(energyGate, 10u));

    SkillStubs::SetSkillData(2249u, 2u, 5u, 5u, 0.5f, 0.75f, 0u, 0u);
    AgentStubs::AddNpc(20u, 100.0f, 0.0f, 3u, 0.80f);
    CachedSkill finishHim = {};
    finishHim.skill_id = 2249u;
    finishHim.roles = ROLE_PRESSURE;
    finishHim.skill_type = 2u;
    finishHim.target_type = 5u;
    finishHim.energy_cost = 5u;
    me->energy = 1.0f;
    me->max_energy = 30u;
    GWA3_ASSERT(!CanUseSkill(finishHim, 20u));
    static_cast<GWA3::AgentLiving*>(GWA3::AgentMgr::GetAgentByID(20u))->hp = 0.30f;
    GWA3_ASSERT(CanUseSkill(finishHim, 20u));
});
