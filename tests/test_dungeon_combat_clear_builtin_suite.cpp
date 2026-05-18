// Consolidated test module generated from small test files.
#include "DungeonCombatTestSupport.h"
#include "DungeonInteractionsTestSupport.h"
#include <gwa3/testing/TestFramework.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/game/Agent.h>
#include <cstring>

// --- tests/test_dungeon_combat_aggro_move_policy.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_aggro_move_policy {

namespace DungeonCombat = GWA3::DungeonCombat;

GWA3_TEST(dungeon_combat_aggro_move_policy, {
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::AGGRO_DEFAULT_FIGHT_RANGE), 1350);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::AGGRO_ARRIVAL_THRESHOLD), 250);
    GWA3_ASSERT_EQ(DungeonCombat::AGGRO_MOVE_BUDGET_MS, 240000u);
    GWA3_ASSERT_EQ(DungeonCombat::AGGRO_FORCE_MOVE_AFTER_MS, 60000u);
    GWA3_ASSERT_EQ(DungeonCombat::AGGRO_BLOCKED_LIMIT, 30);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::AGGRO_MOVE_RANDOM_RADIUS), 100);
    GWA3_ASSERT_EQ(DungeonCombat::AGGRO_STANDARD_MOVING_REISSUE_MS, 1800u);
    GWA3_ASSERT_EQ(DungeonCombat::AGGRO_STANDARD_REISSUE_MS, 1200u);
    GWA3_ASSERT_EQ(DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_COOLDOWN_MS, 2500u);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_COOLDOWN_DISTANCE), 450);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::AGGRO_STANDARD_BLOCKED_PROGRESS_DISTANCE), 10);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_aggro_move_policy

// --- tests/test_dungeon_combat_local_clear_policy.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_local_clear_policy {

namespace DungeonCombat = GWA3::DungeonCombat;

GWA3_TEST(dungeon_combat_local_clear_policy, {
    GWA3_ASSERT_EQ(DungeonCombat::LOCAL_CLEAR_PRE_FIGHT_CANCEL_DWELL_MS, 50u);
    GWA3_ASSERT_EQ(DungeonCombat::LOCAL_CLEAR_POST_FIGHT_CANCEL_DWELL_MS, 150u);
    GWA3_ASSERT_EQ(DungeonCombat::LOCAL_CLEAR_DWELL_POLL_MS, 200u);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::LOCAL_CLEAR_NEAREST_ENEMY_SCAN_PADDING), 250);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::LOCAL_CLEAR_EXIT_DISTANCE_PADDING), 75);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::LOCAL_CLEAR_RANGE_PADDING), 250);
    GWA3_ASSERT_EQ(static_cast<int>(DungeonCombat::LOCAL_CLEAR_MIN_RANGE), 1600);

    const auto shortPolicy = DungeonCombat::BuildLocalClearPolicy(
        DungeonCombat::LocalClearProfile::ShortTraversal,
        "Dungeon",
        1200.0f,
        false);
    GWA3_ASSERT(shortPolicy.single_pass);
    GWA3_ASSERT_EQ(static_cast<int>(shortPolicy.clear_range), 1600);
    GWA3_ASSERT_EQ(shortPolicy.local_clear_budget_ms, 20000u);
    GWA3_ASSERT_EQ(shortPolicy.quiet_dwell_ms, 700u);
    GWA3_ASSERT_EQ(shortPolicy.fight_budget_ms, 8000u);
    GWA3_ASSERT_EQ(shortPolicy.max_clear_passes, 1);
    GWA3_ASSERT(std::strcmp(shortPolicy.clear_label, "Dungeon") == 0);
    GWA3_ASSERT(std::strcmp(shortPolicy.loot_reason, "local-clear") == 0);

    const auto standardPolicy = DungeonCombat::BuildLocalClearPolicy(
        DungeonCombat::LocalClearProfile::StandardTraversal,
        nullptr,
        2000.0f,
        true);
    GWA3_ASSERT(!standardPolicy.single_pass);
    GWA3_ASSERT_EQ(static_cast<int>(standardPolicy.clear_range), 2250);
    GWA3_ASSERT_EQ(standardPolicy.local_clear_budget_ms, 120000u);
    GWA3_ASSERT_EQ(standardPolicy.quiet_dwell_ms, 1250u);
    GWA3_ASSERT_EQ(standardPolicy.initial_dwell_timeout_ms, 2500u);
    GWA3_ASSERT_EQ(standardPolicy.settle_dwell_timeout_ms, 4000u);
    GWA3_ASSERT_EQ(standardPolicy.fight_budget_ms, 240000u);
    GWA3_ASSERT(standardPolicy.max_clear_passes > 1000000);
    GWA3_ASSERT(std::strcmp(standardPolicy.clear_label, "Route") == 0);
    GWA3_ASSERT(std::strcmp(standardPolicy.loot_reason, "route-entry-local-clear") == 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_local_clear_policy

// --- tests/test_dungeon_combat_aggro_fight_post_loot.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_aggro_fight_post_loot {

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

namespace {

int UseSkillsWithoutKilling(uint32_t, float, bool) {
    return 0;
}

int UseSkillsAndKill(uint32_t targetId, float, bool) {
    KillTargetOnFight(targetId);
    return 1;
}

void RecordAggroPostLoot(void*, float range, const char*) {
    (void)RecordPickup(range);
}

} // namespace

GWA3_TEST(dungeon_combat_aggro_fight_skips_post_loot_when_budget_hits_with_enemy_nearby, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(616u);
    PartyStubs::ResetFlags();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(42u, 100.0f, 0.0f, 3u, 1.0f);
    ResetCombatRecorder();

    AggroFightCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.wait_ms = &WaitNoop;
    callbacks.use_skills = &UseSkillsWithoutKilling;
    callbacks.post_loot = &RecordAggroPostLoot;

    AggroFightOptions options;
    options.max_fight_ms = 1u;
    options.log_prefix = "Test";

    (void)FightEnemiesInAggro(1000.0f, callbacks, options);

    GWA3_ASSERT_EQ(PickupCount(), 0u);
})

GWA3_TEST(dungeon_combat_aggro_fight_runs_post_loot_after_enemy_clears, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(616u);
    PartyStubs::ResetFlags();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(42u, 100.0f, 0.0f, 3u, 1.0f);
    ResetCombatRecorder();

    AggroFightCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.wait_ms = &WaitNoop;
    callbacks.use_skills = &UseSkillsAndKill;
    callbacks.post_loot = &RecordAggroPostLoot;

    AggroFightOptions options;
    options.max_fight_ms = 1000u;
    options.log_prefix = "Test";

    (void)FightEnemiesInAggro(1000.0f, callbacks, options);

    GWA3_ASSERT_EQ(PickupCount(), 1u);
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_aggro_fight_post_loot

// --- tests/test_dungeon_combat_builtin_cadence.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_builtin_cadence {

using namespace GWA3::DungeonCombat;

GWA3_TEST(dungeon_builtin_combat_configures_froggy_style_clear_cadence, {
    AggroAdvanceOptions options;
    GWA3::DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(options, 30000u, true);

    GWA3_ASSERT_EQ(options.timeout_ms, 30000u);
    GWA3_ASSERT(options.clear_options.pickup_after_clear);
    GWA3_ASSERT(!options.clear_options.flag_heroes);
    GWA3_ASSERT(!options.clear_options.change_target);
    GWA3_ASSERT(!options.clear_options.call_target);
    GWA3_ASSERT(!options.clear_options.chase_during_clear);
    GWA3_ASSERT(options.clear_options.hold_movement_for_local_clear);
    GWA3_ASSERT_EQ(options.clear_options.quiet_confirmation_ms, 1250u);
    GWA3_ASSERT_EQ(options.clear_options.chase_wait_ms, 350u);
    GWA3_ASSERT_EQ(options.clear_options.pre_clear_cancel_wait_ms, 50u);
    GWA3_ASSERT_EQ(options.clear_options.post_clear_cancel_wait_ms, 150u);
    GWA3_ASSERT_EQ(options.clear_options.idle_wait_ms, 150u);
    GWA3_ASSERT_EQ(options.clear_options.loop_wait_ms, 250u);
    GWA3_ASSERT_EQ(options.clear_options.fight_reissue_ms, 750u);
    GWA3_ASSERT_EQ(options.clear_options.attack_reissue_ms, 750u);
    GWA3_ASSERT_EQ(options.clear_options.timeout_ms, 30000u);
    GWA3_ASSERT_EQ(options.clear_options.target_timeout_ms, 30000u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_builtin_cadence



// --- tests/test_dungeon_combat_builtin_callbacks.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_builtin_callbacks {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_builtin_callbacks_do_not_double_attack_inside_clear_loop, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForAggroAdvance;
    callbacks.fight_target = &FightTargetWithBuiltinCombatAndKill;

    ClearEnemiesOptions options;
    options.quiet_confirmation_ms = 0u;
    options.loop_wait_ms = 0u;
    options.idle_wait_ms = 0u;
    options.chase_wait_ms = 0u;
    options.flag_heroes = false;
    options.change_target = false;
    options.call_target = false;
    options.pickup_after_clear = false;

    GWA3_ASSERT(ClearEnemiesInArea(1300.0f, callbacks, options));
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::CallTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 8u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_builtin_callbacks



// --- tests/test_dungeon_combat_clear_enemies_casting.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_casting {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_clear_enemies_pauses_target_churn_while_casting, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->skill = 1239u;
    me->model_state = 0x245u;

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForAggroAdvance;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    ClearEnemiesOptions options;
    options.timeout_ms = 10u;
    options.loop_wait_ms = 0u;
    options.idle_wait_ms = 0u;
    options.chase_wait_ms = 0u;

    GWA3_ASSERT(!ClearEnemiesInArea(1300.0f, callbacks, options));
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::CallTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 0u);
    GWA3_ASSERT_EQ(LastFightTarget(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_casting



// --- tests/test_dungeon_combat_clear_enemies_flags.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_flags {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_clear_enemies_flags_targets_and_picks_up, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForAggroAdvance;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    ClearEnemiesOptions options;
    options.quiet_confirmation_ms = 0u;
    options.loop_wait_ms = 0u;
    options.idle_wait_ms = 0u;
    options.chase_wait_ms = 0u;

    GWA3_ASSERT(ClearEnemiesInArea(1300.0f, callbacks, options));
    GWA3_ASSERT_EQ(LastFightTarget(), 8u);
    GWA3_ASSERT_EQ(PartyStubs::FlagAllCount(), 1u);
    GWA3_ASSERT_EQ(PartyStubs::UnflagAllCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(PartyStubs::LastFlagAllX()), 250);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::CallTargetCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastChangedTargetId(), 8u);
    GWA3_ASSERT_EQ(AgentStubs::LastCalledTargetId(), 8u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 8u);
    GWA3_ASSERT_EQ(PickupCount(), 1u);
})

GWA3_TEST(dungeon_combat_clear_enemies_succeeds_when_timeout_recount_is_clear, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForAggroAdvance;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    ClearEnemiesOptions options;
    options.timeout_ms = 25u;
    options.quiet_confirmation_ms = 60000u;
    options.loop_wait_ms = 0u;
    options.idle_wait_ms = 0u;
    options.chase_wait_ms = 0u;
    options.flag_heroes = false;
    options.change_target = false;
    options.call_target = false;

    GWA3_ASSERT(ClearEnemiesInArea(1300.0f, callbacks, options));
    GWA3_ASSERT(EnemyCleared());
    GWA3_ASSERT_EQ(PickupCount(), 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_flags



// --- tests/test_dungeon_combat_clear_enemies_hold_movement.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_hold_movement {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_clear_enemies_can_hold_movement_during_local_clear, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 1500.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    ClearEnemiesOptions options;
    options.quiet_confirmation_ms = 0u;
    options.loop_wait_ms = 0u;
    options.idle_wait_ms = 0u;
    options.chase_wait_ms = 0u;
    options.pickup_after_clear = false;
    options.flag_heroes = false;
    options.change_target = false;
    options.call_target = false;
    options.chase_during_clear = false;
    options.hold_movement_for_local_clear = true;

    GWA3_ASSERT(ClearEnemiesInArea(900.0f, callbacks, options));
    GWA3_ASSERT_EQ(LastFightTarget(), 8u);
    GWA3_ASSERT(EnemyCleared());
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_hold_movement



// --- tests/test_dungeon_combat_clear_enemies_skip_packets.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_skip_packets {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_clear_enemies_can_skip_target_packets_and_still_attack, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForAggroAdvance;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    ClearEnemiesOptions options;
    options.quiet_confirmation_ms = 0u;
    options.loop_wait_ms = 0u;
    options.idle_wait_ms = 0u;
    options.chase_wait_ms = 0u;
    options.flag_heroes = false;
    options.change_target = false;
    options.call_target = false;

    GWA3_ASSERT(ClearEnemiesInArea(1300.0f, callbacks, options));
    GWA3_ASSERT_EQ(LastFightTarget(), 8u);
    GWA3_ASSERT_EQ(PartyStubs::FlagAllCount(), 0u);
    GWA3_ASSERT_EQ(PartyStubs::UnflagAllCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::CallTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 8u);
    GWA3_ASSERT_EQ(PickupCount(), 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_skip_packets



// --- tests/test_dungeon_combat_clear_enemies_bundle_carry.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_bundle_carry {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace ItemStubs = GWA3::TestStubs::ItemMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_clear_enemies_with_equipped_bundle_calls_target_and_moves_instead_of_attacking, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->weapon_type = 0u;
    me->weapon_item_type = 0u;
    me->weapon_item_id = 77u;
    AgentStubs::AddNpc(8u, 1500.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    ClearEnemiesOptions options;
    options.quiet_confirmation_ms = 0u;
    options.loop_wait_ms = 0u;
    options.idle_wait_ms = 0u;
    options.chase_wait_ms = 0u;
    options.flag_heroes = false;
    options.change_target = false;
    options.call_target = false;
    options.chase_during_clear = false;
    options.pickup_after_clear = false;

    GWA3_ASSERT(ClearEnemiesInArea(1300.0f, callbacks, options));
    GWA3_ASSERT_EQ(LastFightTarget(), 8u);
    GWA3_ASSERT_EQ(AgentStubs::CallTargetCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastCalledTargetId(), 8u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 0u);
    GWA3_ASSERT(AgentStubs::MoveCount() >= 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 1500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_clear_enemies_bundle_carry



// --- tests/test_dungeon_combat_enemy_queries.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_enemy_queries {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
using namespace GWA3::DungeonCombat;

GWA3_TEST(dungeon_combat_enemy_queries_find_and_count_foes, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(3u, 700.0f, 0.0f, 3u, 1.0f);
    AgentStubs::AddNpc(5u, 150.0f, 0.0f, 5u, 1.0f);
    AgentStubs::AddNpc(6u, 100.0f, 0.0f, 3u, 0.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);

    float distance = 0.0f;
    GWA3_ASSERT_EQ(FindNearestLivingEnemy(1000.0f, &distance), 8u);
    GWA3_ASSERT(distance > 0.0f && distance < 300.0f);
    GWA3_ASSERT_EQ(CountLivingEnemiesInRange(1000.0f), 2u);
})

GWA3_TEST(dungeon_combat_enemy_range_gate_blocks_close_short_range_movement, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(3u, 250.0f, 0.0f, 3u, 1.0f);

    GWA3_ASSERT(!CanMoveWithEnemyRangeGate(500.0f, 1320.0f));
    GWA3_ASSERT(CanMoveWithEnemyRangeGate(1400.0f, 1320.0f));
})

GWA3_TEST(dungeon_combat_enemy_clear_dwell_reports_clear_or_blocked_range, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);

    CombatCallbacks callbacks = {};
    callbacks.wait_ms = &GWA3::Tests::DungeonCombatSupport::WaitNoop;
    GWA3_ASSERT(WaitForEnemyClearDwell(500.0f, 0u, 0u, callbacks, 0u));

    AgentStubs::AddNpc(3u, 250.0f, 0.0f, 3u, 1.0f);
    GWA3_ASSERT(!WaitForEnemyClearDwell(500.0f, 0u, 0u, callbacks, 0u));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_enemy_queries



// --- tests/test_dungeon_combat_priority_builtin.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_priority_builtin {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;

GWA3_TEST(dungeon_combat_priority_builtin_combat_relies_on_attack_without_change_target, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);

    GWA3::DungeonBuiltinCombat::FightTargetWithPriorityBuiltinCombat(8u);

    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 8u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_priority_builtin
