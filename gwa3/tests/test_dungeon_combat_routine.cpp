#include <gwa3/bot/DungeonCombatRoutine.h>
#include <gwa3/bot/DungeonSkill.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/testing/TestFramework.h>

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
}

namespace GWA3::TestStubs::EffectMgr {
void Reset();
}

namespace GWA3::TestStubs::PartyMgr {
void ResetFlags();
void SetPartyDefeated(bool defeated);
}

namespace GWA3::TestStubs::SkillMgr {
void Reset();
void SetSkillData(uint32_t skillId, uint32_t type, uint8_t target, uint8_t energyCost,
                  float activation, float aftercast,
                  uint32_t recharge, uint32_t adrenaline);
void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t recharge, uint32_t adrenaline);
uint32_t LastUsedSkillSlot();
uint32_t LastUsedSkillTargetId();
}

using namespace GWA3::Bot::DungeonCombatRoutine;
using namespace GWA3::Bot::DungeonSkill;

namespace {

void WaitNoopCombatRoutine(uint32_t) {
}

bool NotDead() {
    return false;
}

void AutoAttack(uint32_t targetId) {
    GWA3::AgentMgr::Attack(targetId);
}

SkillExecutionContext MakeContext(CachedSkill cache[8], bool used[8]) {
    SkillExecutionContext context;
    context.skill_cache = cache;
    context.skill_used_this_step = used;
    context.skill_count = 8u;
    context.wait_ms = &WaitNoopCombatRoutine;
    context.is_dead = &NotDead;
    return context;
}

void ResetCombatRoutineStubs() {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    EffectStubs::Reset();
    PartyStubs::ResetFlags();
    PartyStubs::SetPartyDefeated(false);
    SkillStubs::Reset();
}

} // namespace

GWA3_TEST(dungeon_combat_routine_try_use_skill_with_role_resolves_heal_target, {
    ResetCombatRoutineStubs();
    AgentStubs::AddNpc(2u, 100.0f, 0.0f, 1u, 0.35f);

    SkillStubs::SetSkillData(68u, 2u, 3u, 5u, 0.25f, 0.75f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 68u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    SkillActionResult action = {};
    auto context = MakeContext(cache, used);
    GWA3_ASSERT(TryUseSkillWithRole(99u, ROLE_ANY_HEAL, context, action));
    GWA3_ASSERT(action.valid);
    GWA3_ASSERT(action.used_skill);
    GWA3_ASSERT_EQ(action.slot, 1);
    GWA3_ASSERT_EQ(action.skill_id, 68u);
    GWA3_ASSERT_EQ(action.target_id, 2u);
    GWA3_ASSERT_EQ(action.expected_aftercast_ms, 750u);
    GWA3_ASSERT(used[0]);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillSlot(), 1u);
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillTargetId(), 2u);
})

GWA3_TEST(dungeon_combat_routine_try_use_skill_with_role_skips_while_casting, {
    ResetCombatRoutineStubs();
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    SkillStubs::SetSkillData(500u, 2u, 5u, 5u, 0.25f, 0.50f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 500u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->skill = 500u;
    me->model_state = 0x245u;

    SkillActionResult action = {};
    auto context = MakeContext(cache, used);
    GWA3_ASSERT(!TryUseSkillWithRole(10u, ROLE_OFFENSIVE, context, action));
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillSlot(), 0u);
    GWA3_ASSERT(!action.valid);
})

GWA3_TEST(dungeon_combat_routine_execute_builtin_priority_prefers_emergency_heal, {
    ResetCombatRoutineStubs();
    AgentStubs::AddNpc(2u, 100.0f, 0.0f, 1u, 0.20f);
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    SkillStubs::SetSkillData(68u, 2u, 3u, 5u, 0.25f, 0.50f, 0u, 0u);
    SkillStubs::SetSkillData(500u, 2u, 5u, 5u, 0.25f, 0.50f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 68u, 0u, 0u);
    SkillStubs::SetSkillbarSkill(2u, 500u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    SkillActionResult action = {};
    auto context = MakeContext(cache, used);
    GWA3_ASSERT(ExecuteBuiltinPriorityStep(10u, context, &AutoAttack, action));
    GWA3_ASSERT(action.used_skill);
    GWA3_ASSERT_EQ(action.skill_id, 68u);
    GWA3_ASSERT_EQ(action.target_id, 2u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
})

GWA3_TEST(dungeon_combat_routine_execute_builtin_priority_prefers_resurrection, {
    ResetCombatRoutineStubs();
    AgentStubs::AddNpc(3u, 120.0f, 0.0f, 1u, 0.0f);
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    SkillStubs::SetSkillData(200u, 2u, 6u, 5u, 1.0f, 0.50f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 200u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    SkillActionResult action = {};
    auto context = MakeContext(cache, used);
    GWA3_ASSERT(ExecuteBuiltinPriorityStep(10u, context, &AutoAttack, action));
    GWA3_ASSERT(action.used_skill);
    GWA3_ASSERT_EQ(action.skill_id, 200u);
    GWA3_ASSERT_EQ(action.target_id, 3u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillTargetId(), 3u);
})

GWA3_TEST(dungeon_combat_routine_execute_builtin_priority_falls_back_to_auto_attack, {
    ResetCombatRoutineStubs();
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    SkillActionResult action = {};
    auto context = MakeContext(cache, used);

    GWA3_ASSERT(ExecuteBuiltinPriorityStep(10u, context, &AutoAttack, action));
    GWA3_ASSERT(action.valid);
    GWA3_ASSERT(action.auto_attack);
    GWA3_ASSERT_EQ(action.target_id, 10u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 10u);
})

GWA3_TEST(dungeon_combat_routine_use_skills_in_slot_order_sweeps_player_bar, {
    ResetCombatRoutineStubs();
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    SkillStubs::SetSkillData(500u, 1u, 5u, 5u, 0.0f, 0.0f, 0u, 0u);
    SkillStubs::SetSkillData(600u, 1u, 5u, 5u, 0.0f, 0.0f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 500u, 0u, 0u);
    SkillStubs::SetSkillbarSkill(2u, 600u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    SkillActionResult action = {};
    auto context = MakeContext(cache, used);
    GWA3_ASSERT_EQ(UseSkillsInSlotOrder(10u, context, &action), 2);
    GWA3_ASSERT(action.valid);
    GWA3_ASSERT(action.used_skill);
    GWA3_ASSERT_EQ(action.slot, 2);
    GWA3_ASSERT_EQ(action.skill_id, 600u);
    GWA3_ASSERT(used[0]);
    GWA3_ASSERT(used[1]);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillSlot(), 2u);
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillTargetId(), 10u);
})

GWA3_TEST(dungeon_combat_routine_execute_combat_step_rejects_invalid_target, {
    ResetCombatRoutineStubs();

    CachedSkill cache[8] = {};
    bool used[8] = {};
    SkillActionResult action = {};
    auto context = MakeContext(cache, used);

    GWA3_ASSERT(!ExecuteCombatStep(0u, context, &AutoAttack, action));
    GWA3_ASSERT(!action.valid);
})

GWA3_TEST(dungeon_combat_routine_inspect_skill_candidate_reports_common_gates, {
    ResetCombatRoutineStubs();
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    SkillStubs::SetSkillData(500u, 2u, 5u, 5u, 0.25f, 0.50f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 500u, 0u, 0u);

    CachedSkill candidate = {};
    candidate.skill_id = 500u;
    candidate.roles = ROLE_OFFENSIVE;
    candidate.slot = 0u;
    candidate.target_type = 5u;
    candidate.energy_cost = 5u;
    candidate.skill_type = 2u;

    const auto inspection = InspectSkillCandidate(candidate, 0, 10u, ROLE_OFFENSIVE | ROLE_ATTACK);
    GWA3_ASSERT(inspection.available);
    GWA3_ASSERT(inspection.role_match);
    GWA3_ASSERT(inspection.recharge_ready);
    GWA3_ASSERT(inspection.energy_ready);
    GWA3_ASSERT(inspection.can_cast);
    GWA3_ASSERT(inspection.can_use);
    GWA3_ASSERT_EQ(inspection.resolved_target, 10u);
    GWA3_ASSERT_EQ(inspection.recharge, 0u);
})
