// Consolidated test module generated from small test files.
#include "DungeonCombatTestSupport.h"
#include <gwa3/testing/TestFramework.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/game/Agent.h>

// --- tests/test_dungeon_combat_advance_arrival_threshold.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_arrival_threshold {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

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
    GWA3_ASSERT_EQ(LastFightTarget(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_arrival_threshold



// --- tests/test_dungeon_combat_advance_disable_local_clear_floor.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_disable_local_clear_floor {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

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
    GWA3_ASSERT_EQ(LastFightTarget(), 0u);
    GWA3_ASSERT(!EnemyCleared());
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_disable_local_clear_floor



// --- tests/test_dungeon_combat_advance_local_clear_radius.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_local_clear_radius {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

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
    GWA3_ASSERT_EQ(LastFightTarget(), 8u);
    GWA3_ASSERT(EnemyCleared());
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 2u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_local_clear_radius



// --- tests/test_dungeon_combat_advance_no_local_foes.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_no_local_foes {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

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
    GWA3_ASSERT_EQ(LastFightTarget(), 0u);
    GWA3_ASSERT(AgentStubs::MoveCount() >= 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_no_local_foes



// --- tests/test_dungeon_combat_advance_stuck_abort.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_stuck_abort {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_advance_with_aggro_honors_custom_stuck_abort_threshold, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();
    SetDelayedMoveReleaseAfter(40u);

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
    GWA3_ASSERT(DelayedMoveCalls() > DelayedMoveReleaseAfter());
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_advance_stuck_abort
