#include <gwa3/bot/DungeonBuiltinCombat.h>
#include <gwa3/bot/DungeonCombat.h>
#include <gwa3/testing/TestFramework.h>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
void SetPlayerAgent(float x, float y, float hp);
uint32_t MoveCount();
uint32_t ChangeTargetCount();
uint32_t CallTargetCount();
uint32_t AttackCount();
uint32_t LastChangedTargetId();
uint32_t LastCalledTargetId();
uint32_t LastAttackTargetId();
float LastMoveX();
float LastMoveY();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::MapMgr {

void Reset();
void SetMapId(uint32_t mapId);

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetFlags();
uint32_t FlagAllCount();
uint32_t UnflagAllCount();
float LastFlagAllX();
float LastFlagAllY();

} // namespace GWA3::TestStubs::PartyMgr

using namespace GWA3::Bot::DungeonCombat;

namespace {

uint32_t g_pickup_count = 0u;
uint32_t g_last_fight_target = 0u;
bool g_enemy_cleared = false;
bool g_combat_route_retry_blocked_second_waypoint = false;
bool g_combat_route_retry_unlocked_second_waypoint = false;
uint32_t g_delayed_move_calls = 0u;
uint32_t g_delayed_move_release_after = 0u;
bool g_combat_progress_floor_failure_armed = false;
bool g_combat_progress_floor_third_waypoint_unlocked = false;
bool g_combat_progress_floor_wrong_backtrack = false;
uint32_t g_waypoint_hook_call_count = 0u;
uint32_t g_waypoint_hook_before_count = 0u;
uint32_t g_waypoint_hook_after_count = 0u;
int g_waypoint_hook_last_index = -1;
int g_waypoint_hook_last_label_kind = -1;
AggroWaypointPhase g_waypoint_hook_last_phase = AggroWaypointPhase::BeforeAdvance;
bool g_waypoint_hook_failed_second_before = false;

bool AliveAndLoaded() {
    return true;
}

bool AliveFalse() {
    return false;
}

void WaitNoop(uint32_t) {
}

void QueueMoveForCombat(float x, float y) {
    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveForAggroAdvance(float x, float y) {
    if (!g_enemy_cleared && static_cast<int>(x) == 500 && static_cast<int>(y) == 0) {
        GWA3::AgentMgr::Move(40.0f, 0.0f);
        return;
    }
    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveWithAggroBacktrack(float x, float y) {
    const int targetX = static_cast<int>(x);
    const int targetY = static_cast<int>(y);
    if (targetX == 100 && targetY == 0) {
        if (g_combat_route_retry_blocked_second_waypoint) {
            g_combat_route_retry_unlocked_second_waypoint = true;
        }
        GWA3::AgentMgr::Move(100.0f, 0.0f);
        return;
    }

    if (targetX == 200 && targetY == 0 && !g_combat_route_retry_unlocked_second_waypoint) {
        g_combat_route_retry_blocked_second_waypoint = true;
        return;
    }

    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveWithDelayedRelease(float x, float y) {
    ++g_delayed_move_calls;
    if (g_delayed_move_calls <= g_delayed_move_release_after) {
        return;
    }
    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveWithAggroProgressFloorRetry(float x, float y) {
    const int targetX = static_cast<int>(x);
    const int targetY = static_cast<int>(y);
    if (targetY != 0) {
        GWA3::AgentMgr::Move(x, y);
        return;
    }

    if (targetX == 100) {
        if (g_combat_progress_floor_failure_armed && !g_combat_progress_floor_third_waypoint_unlocked) {
            g_combat_progress_floor_wrong_backtrack = true;
        }
        GWA3::AgentMgr::Move(100.0f, 0.0f);
        return;
    }

    if (targetX == 200) {
        if (g_combat_progress_floor_failure_armed) {
            g_combat_progress_floor_third_waypoint_unlocked = true;
        }
        GWA3::AgentMgr::Move(200.0f, 0.0f);
        return;
    }

    if (targetX == 300 && !g_combat_progress_floor_third_waypoint_unlocked) {
        g_combat_progress_floor_failure_armed = true;
        GWA3::AgentMgr::Move(110.0f, 0.0f);
        return;
    }

    GWA3::AgentMgr::Move(x, y);
}

int RecordPickup(float) {
    ++g_pickup_count;
    return 1;
}

bool RecordWaypointHook(
    const GWA3::Bot::DungeonRoute::Waypoint&,
    int waypointIndex,
    GWA3::Bot::DungeonRoute::WaypointLabelKind labelKind,
    AggroWaypointPhase phase,
    void*) {
    ++g_waypoint_hook_call_count;
    g_waypoint_hook_last_index = waypointIndex;
    g_waypoint_hook_last_label_kind = static_cast<int>(labelKind);
    g_waypoint_hook_last_phase = phase;
    if (phase == AggroWaypointPhase::BeforeAdvance) {
        ++g_waypoint_hook_before_count;
    } else {
        ++g_waypoint_hook_after_count;
    }
    return true;
}

bool FailSecondWaypointBeforeOnce(
    const GWA3::Bot::DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    GWA3::Bot::DungeonRoute::WaypointLabelKind labelKind,
    AggroWaypointPhase phase,
    void* userData) {
    if (!RecordWaypointHook(waypoint, waypointIndex, labelKind, phase, userData)) {
        return false;
    }

    if (waypointIndex == 1 &&
        phase == AggroWaypointPhase::BeforeAdvance &&
        !g_waypoint_hook_failed_second_before) {
        g_waypoint_hook_failed_second_before = true;
        return false;
    }

    return true;
}

void KillTargetOnFight(uint32_t targetId) {
    g_last_fight_target = targetId;
    g_enemy_cleared = true;
    auto* agent = GWA3::AgentMgr::GetAgentByID(targetId);
    if (agent && agent->type == 0xDBu) {
        static_cast<GWA3::AgentLiving*>(agent)->hp = 0.0f;
    }
}

void FightTargetWithBuiltinCombatAndKill(uint32_t targetId) {
    GWA3::Bot::DungeonBuiltinCombat::FightTargetWithBuiltinCombat(targetId);
    KillTargetOnFight(targetId);
}

void ResetCombatRecorder() {
    g_pickup_count = 0u;
    g_last_fight_target = 0u;
    g_enemy_cleared = false;
    g_combat_route_retry_blocked_second_waypoint = false;
    g_combat_route_retry_unlocked_second_waypoint = false;
    g_delayed_move_calls = 0u;
    g_delayed_move_release_after = 0u;
    g_combat_progress_floor_failure_armed = false;
    g_combat_progress_floor_third_waypoint_unlocked = false;
    g_combat_progress_floor_wrong_backtrack = false;
    g_waypoint_hook_call_count = 0u;
    g_waypoint_hook_before_count = 0u;
    g_waypoint_hook_after_count = 0u;
    g_waypoint_hook_last_index = -1;
    g_waypoint_hook_last_label_kind = -1;
    g_waypoint_hook_last_phase = AggroWaypointPhase::BeforeAdvance;
    g_waypoint_hook_failed_second_before = false;
}

} // namespace

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
    GWA3_ASSERT_EQ(g_last_fight_target, 8u);
    GWA3_ASSERT_EQ(PartyStubs::FlagAllCount(), 1u);
    GWA3_ASSERT_EQ(PartyStubs::UnflagAllCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(PartyStubs::LastFlagAllX()), 250);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::CallTargetCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastChangedTargetId(), 8u);
    GWA3_ASSERT_EQ(AgentStubs::LastCalledTargetId(), 8u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 8u);
    GWA3_ASSERT_EQ(g_pickup_count, 1u);
})

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
    GWA3_ASSERT_EQ(g_last_fight_target, 8u);
    GWA3_ASSERT_EQ(PartyStubs::FlagAllCount(), 0u);
    GWA3_ASSERT_EQ(PartyStubs::UnflagAllCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::CallTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 8u);
    GWA3_ASSERT_EQ(g_pickup_count, 1u);
})

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

GWA3_TEST(dungeon_combat_priority_builtin_combat_relies_on_attack_without_change_target, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 250.0f, 0.0f, 3u, 1.0f);

    GWA3::Bot::DungeonBuiltinCombat::FightTargetWithPriorityBuiltinCombat(8u);

    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastAttackTargetId(), 8u);
})

GWA3_TEST(dungeon_builtin_combat_configures_froggy_style_clear_cadence, {
    AggroAdvanceOptions options;
    GWA3::Bot::DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(options, 30000u, true);

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
    GWA3_ASSERT_EQ(g_last_fight_target, 8u);
    GWA3_ASSERT(g_enemy_cleared);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::AttackCount(), 1u);
})

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
    GWA3_ASSERT_EQ(g_last_fight_target, 0u);
})

GWA3_TEST(dungeon_combat_advance_with_aggro_reaches_target_without_local_foes, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    AggroAdvanceOptions options;
    options.move_wait_ms = 0u;
    options.clear_options.quiet_confirmation_ms = 0u;
    options.clear_options.loop_wait_ms = 0u;
    options.clear_options.idle_wait_ms = 0u;
    options.clear_options.chase_wait_ms = 0u;

    GWA3_ASSERT(AdvanceWithAggro(500.0f, 0.0f, 1300.0f, callbacks, options));
    GWA3_ASSERT_EQ(g_last_fight_target, 0u);
    GWA3_ASSERT(AgentStubs::MoveCount() >= 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_combat_advance_with_aggro_skips_initial_move_when_already_within_threshold, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(100.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    AggroAdvanceOptions options;
    options.arrival_threshold = 60.0f;
    options.move_wait_ms = 0u;
    options.clear_options.quiet_confirmation_ms = 0u;
    options.clear_options.loop_wait_ms = 0u;
    options.clear_options.idle_wait_ms = 0u;
    options.clear_options.chase_wait_ms = 0u;

    GWA3_ASSERT(AdvanceWithAggro(150.0f, 0.0f, 1300.0f, callbacks, options));
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 0u);
    GWA3_ASSERT_EQ(g_last_fight_target, 0u);
})

GWA3_TEST(dungeon_combat_advance_with_aggro_holds_for_froggy_local_clear_radius, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddNpc(8u, 1500.0f, 0.0f, 3u, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForAggroAdvance;
    callbacks.fight_target = &KillTargetOnFight;
    callbacks.pickup_loot = &RecordPickup;

    AggroAdvanceOptions options;
    options.move_wait_ms = 0u;
    options.clear_options.quiet_confirmation_ms = 0u;
    options.clear_options.loop_wait_ms = 0u;
    options.clear_options.idle_wait_ms = 0u;
    options.clear_options.chase_wait_ms = 0u;
    options.clear_options.pickup_after_clear = false;
    options.clear_options.chase_during_clear = false;
    options.clear_options.hold_movement_for_local_clear = true;

    GWA3_ASSERT(AdvanceWithAggro(500.0f, 0.0f, 900.0f, callbacks, options));
    GWA3_ASSERT_EQ(g_last_fight_target, 8u);
    GWA3_ASSERT(g_enemy_cleared);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 2u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_combat_advance_with_aggro_can_disable_local_clear_floor, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
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

    AggroAdvanceOptions options;
    options.move_wait_ms = 0u;
    options.clear_options.extra_clear_range = 0.0f;
    options.clear_options.minimum_local_clear_range = 0.0f;
    options.clear_options.chase_distance = 900.0f;
    options.clear_options.quiet_confirmation_ms = 0u;
    options.clear_options.loop_wait_ms = 0u;
    options.clear_options.idle_wait_ms = 0u;
    options.clear_options.chase_wait_ms = 0u;
    options.clear_options.pickup_after_clear = false;

    GWA3_ASSERT(AdvanceWithAggro(500.0f, 0.0f, 900.0f, callbacks, options));
    GWA3_ASSERT_EQ(g_last_fight_target, 0u);
    GWA3_ASSERT(!g_enemy_cleared);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_combat_advance_with_aggro_honors_custom_stuck_abort_threshold, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();
    g_delayed_move_release_after = 40u;

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveWithDelayedRelease;
    callbacks.fight_target = &KillTargetOnFight;

    AggroAdvanceOptions options;
    options.timeout_ms = 250u;
    options.move_wait_ms = 0u;
    options.stuck_recovery_threshold = 1000;
    options.stuck_abort_threshold = 60;
    options.clear_options.quiet_confirmation_ms = 0u;
    options.clear_options.loop_wait_ms = 0u;
    options.clear_options.idle_wait_ms = 0u;
    options.clear_options.chase_wait_ms = 0u;

    GWA3_ASSERT(AdvanceWithAggro(500.0f, 0.0f, 1300.0f, callbacks, options));
    GWA3_ASSERT(g_delayed_move_calls > g_delayed_move_release_after);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_backtracks_after_timeout, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveWithAggroBacktrack;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3::Bot::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.stuck_recovery_threshold = 1000;
    aggroOptions.stuck_abort_threshold = 1000;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_preserves_progress_floor_when_player_drifts_backward, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveWithAggroProgressFloorRetry;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3::Bot::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.stuck_recovery_threshold = 1000;
    aggroOptions.stuck_abort_threshold = 1000;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT(!g_combat_progress_floor_wrong_backtrack);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_keeps_arrival_tolerance_separate_from_fight_range, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {500.0f, 0.0f, 1000.0f, "1"},
    };

    GWA3::Bot::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 250.0f;
    options.use_waypoint_fight_range_as_tolerance = false;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_invokes_waypoint_hooks_before_and_after_arrival, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {500.0f, 0.0f, 0.0f, "Asura Flame Staff"},
    };

    GWA3::Bot::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    AggroWaypointCallbacks waypointCallbacks;
    waypointCallbacks.on_waypoint = &RecordWaypointHook;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions,
        waypointCallbacks);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(g_waypoint_hook_call_count, 2u);
    GWA3_ASSERT_EQ(g_waypoint_hook_before_count, 1u);
    GWA3_ASSERT_EQ(g_waypoint_hook_after_count, 1u);
    GWA3_ASSERT_EQ(g_waypoint_hook_last_index, 0);
    GWA3_ASSERT_EQ(
        g_waypoint_hook_last_label_kind,
        static_cast<int>(GWA3::Bot::DungeonRoute::WaypointLabelKind::AsuraFlameStaff));
    GWA3_ASSERT_EQ(static_cast<int>(g_waypoint_hook_last_phase), static_cast<int>(AggroWaypointPhase::AfterAdvance));
})

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_retries_after_waypoint_hook_failure, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "Staff Check"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3::Bot::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    AggroWaypointCallbacks waypointCallbacks;
    waypointCallbacks.on_waypoint = &FailSecondWaypointBeforeOnce;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions,
        waypointCallbacks);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT(g_waypoint_hook_failed_second_before);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
