// Consolidated test module generated from small test files.
#include "DungeonCombatRoutineTestSupport.h"
#include <gwa3/managers/AgentMgr.h>

// --- tests/test_dungeon_combat_routine_action_result_builders.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_action_result_builders {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;

GWA3_TEST(dungeon_combat_routine_action_result_builders_populate_common_fields, {
    CachedSkill skill = {};
    skill.skill_id = 500u;
    skill.roles = ROLE_OFFENSIVE | ROLE_ATTACK;
    skill.target_type = 5u;

    const auto skillAction = MakeSkillActionResult(2, skill, 10u, 1234u);
    GWA3_ASSERT(skillAction.valid);
    GWA3_ASSERT(skillAction.used_skill);
    GWA3_ASSERT(!skillAction.auto_attack);
    GWA3_ASSERT_EQ(skillAction.slot, 3);
    GWA3_ASSERT_EQ(skillAction.skill_id, 500u);
    GWA3_ASSERT_EQ(skillAction.target_id, 10u);
    GWA3_ASSERT_EQ(skillAction.role_mask, ROLE_OFFENSIVE | ROLE_ATTACK);
    GWA3_ASSERT_EQ(skillAction.target_type, 5u);
    GWA3_ASSERT_EQ(skillAction.started_at_ms, 1234u);

    const auto attackAction = MakeAutoAttackActionResult(11u, ROLE_ATTACK, 5678u);
    GWA3_ASSERT(attackAction.valid);
    GWA3_ASSERT(!attackAction.used_skill);
    GWA3_ASSERT(attackAction.auto_attack);
    GWA3_ASSERT_EQ(attackAction.target_id, 11u);
    GWA3_ASSERT_EQ(attackAction.role_mask, ROLE_ATTACK);
    GWA3_ASSERT_EQ(attackAction.started_at_ms, 5678u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_action_result_builders

// --- tests/test_dungeon_combat_routine_builtin_auto_attack.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_builtin_auto_attack {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_execute_builtin_priority_falls_back_to_auto_attack, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    SkillActionResult action = {};
    auto context = CombatRoutineSupport::MakeContext(cache, used);

    GWA3_ASSERT(ExecuteBuiltinPriorityStep(10u, context, &CombatRoutineSupport::AutoAttack, action));
    GWA3_ASSERT(action.valid);
    GWA3_ASSERT(action.auto_attack);
    GWA3_ASSERT_EQ(action.target_id, 10u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 10u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_builtin_auto_attack

// --- tests/test_dungeon_combat_routine_builtin_heal.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_builtin_heal {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_execute_builtin_priority_prefers_emergency_heal, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
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
    auto context = CombatRoutineSupport::MakeContext(cache, used);
    GWA3_ASSERT(ExecuteBuiltinPriorityStep(10u, context, &CombatRoutineSupport::AutoAttack, action));
    GWA3_ASSERT(action.used_skill);
    GWA3_ASSERT_EQ(action.skill_id, 68u);
    GWA3_ASSERT_EQ(action.target_id, 2u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_builtin_heal

// --- tests/test_dungeon_combat_routine_builtin_resurrection.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_builtin_resurrection {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_execute_builtin_priority_prefers_resurrection, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
    AgentStubs::AddNpc(3u, 120.0f, 0.0f, 1u, 0.0f);
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    SkillStubs::SetSkillData(200u, 2u, 6u, 5u, 1.0f, 0.50f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 200u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    SkillActionResult action = {};
    auto context = CombatRoutineSupport::MakeContext(cache, used);
    GWA3_ASSERT(ExecuteBuiltinPriorityStep(10u, context, &CombatRoutineSupport::AutoAttack, action));
    GWA3_ASSERT(action.used_skill);
    GWA3_ASSERT_EQ(action.skill_id, 200u);
    GWA3_ASSERT_EQ(action.target_id, 3u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillTargetId(), 3u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_builtin_resurrection

// --- tests/test_dungeon_combat_routine_cast_timing.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_cast_timing {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_build_skill_cast_timing_clamps_aftercast, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
    SkillStubs::SetSkillData(500u, 2u, 5u, 5u, 0.25f, 2.75f, 0u, 0u);

    CachedSkill skill = {};
    skill.skill_id = 500u;

    SkillCastTimingOptions options;
    options.max_aftercast = 1.5f;
    const SkillCastTiming timing = BuildSkillCastTiming(0, skill, options);

    GWA3_ASSERT_EQ(static_cast<uint32_t>(timing.activation * 1000.0f), 250u);
    GWA3_ASSERT_EQ(static_cast<uint32_t>(timing.aftercast * 1000.0f), 1500u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_cast_timing

// --- tests/test_dungeon_combat_routine_execute_invalid_target.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_execute_invalid_target {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_execute_combat_step_rejects_invalid_target, {
    CombatRoutineSupport::ResetCombatRoutineStubs();

    CachedSkill cache[8] = {};
    bool used[8] = {};
    SkillActionResult action = {};
    auto context = CombatRoutineSupport::MakeContext(cache, used);

    GWA3_ASSERT(!ExecuteCombatStep(0u, context, &CombatRoutineSupport::AutoAttack, action));
    GWA3_ASSERT(!action.valid);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_execute_invalid_target

// --- tests/test_dungeon_combat_routine_inspect_candidate.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_inspect_candidate {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_inspect_skill_candidate_reports_common_gates, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
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
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_inspect_candidate

// --- tests/test_dungeon_combat_routine_slot_order.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_slot_order {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_use_skills_in_slot_order_sweeps_player_bar, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
    AgentStubs::AddNpc(10u, 500.0f, 0.0f, 3u, 1.0f);

    SkillStubs::SetSkillData(500u, 1u, 5u, 5u, 0.0f, 0.0f, 0u, 0u);
    SkillStubs::SetSkillData(600u, 1u, 5u, 5u, 0.0f, 0.0f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 500u, 0u, 0u);
    SkillStubs::SetSkillbarSkill(2u, 600u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    SkillActionResult action = {};
    auto context = CombatRoutineSupport::MakeContext(cache, used);
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
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_slot_order

// --- tests/test_dungeon_combat_routine_try_role_heal.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_try_role_heal {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_try_use_skill_with_role_resolves_heal_target, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
    AgentStubs::AddNpc(2u, 100.0f, 0.0f, 1u, 0.35f);

    SkillStubs::SetSkillData(68u, 2u, 3u, 5u, 0.25f, 0.75f, 0u, 0u);
    SkillStubs::SetSkillbarSkill(1u, 68u, 0u, 0u);

    CachedSkill cache[8] = {};
    bool used[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));

    SkillActionResult action = {};
    auto context = CombatRoutineSupport::MakeContext(cache, used);
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
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_try_role_heal

// --- tests/test_dungeon_combat_routine_try_role_skips_casting.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_try_role_skips_casting {


using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_try_use_skill_with_role_skips_while_casting, {
    CombatRoutineSupport::ResetCombatRoutineStubs();
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
    auto context = CombatRoutineSupport::MakeContext(cache, used);
    GWA3_ASSERT(!TryUseSkillWithRole(10u, ROLE_OFFENSIVE, context, action));
    GWA3_ASSERT_EQ(SkillStubs::LastUsedSkillSlot(), 0u);
    GWA3_ASSERT(!action.valid);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_try_role_skips_casting

// --- tests/test_dungeon_combat_routine_wait_cast_completion.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_wait_cast_completion {

using namespace GWA3::DungeonCombatRoutine;
using namespace GWA3::DungeonSkill;
namespace CombatRoutineSupport = GWA3::Tests::DungeonCombatRoutineSupport;

GWA3_TEST(dungeon_combat_routine_wait_for_skill_cast_completion_records_aftercast, {
    CombatRoutineSupport::ResetCombatRoutineStubs();

    CachedSkill skill = {};
    skill.skill_id = 500u;

    SkillCastTiming timing;
    timing.activation = 0.0f;
    timing.aftercast = 0.25f;

    SkillCastTimingOptions options;
    options.latch_extra_ms = 0u;
    options.latch_min_timeout_ms = 0u;
    options.completion_timeout_ms = 0u;

    CachedSkill cache[8] = {};
    bool used[8] = {};
    auto context = CombatRoutineSupport::MakeContext(cache, used);
    SkillActionResult action = {};

    WaitForSkillCastCompletion(0, skill, timing, 0u, 0u, context, action, options);

    GWA3_ASSERT_EQ(action.expected_aftercast_ms, 250u);
    GWA3_ASSERT(action.finished_at_ms != 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_routine_wait_cast_completion
