// Consolidated test module generated from small test files.
#include <gwa3/testing/TestFramework.h>
#include <bots/froggy/FroggyHM.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <cstring>
#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonRouteRunner.h>

// --- tests/test_froggy_aggro_move_policy.cpp ---
namespace GWA3::Tests::Consolidated::test_froggy_aggro_move_policy {

namespace Froggy = GWA3::Bot::Froggy;

GWA3_TEST(froggy_bogroot_aggro_route_tuning, {
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::AGGRO_BOGROOT_SIDESTEP_RANDOM_RADIUS), 500);
    GWA3_ASSERT_EQ(Froggy::AGGRO_BOGROOT_FIGHT_BUDGET_MS, 12000u);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::AGGRO_BOGROOT_LOOT_RADIUS), 3000);
})

GWA3_TEST(froggy_spirit_caster_aggro_gate_is_closer_than_standard, {
    const uint32_t spiritBar[8] = {2233u, 1239u, 1253u, 2110u, 1232u, 1237u, 1228u, 2100u};
    const uint32_t mesmerBar[8] = {2233u, 1346u, 1059u, 2102u, 0u, 0u, 0u, 0u};
    const float fightRange = 1350.0f;

    GWA3_ASSERT(Froggy::IsSpiritCasterAggroSkillbar(spiritBar, 8u));
    GWA3_ASSERT(!Froggy::IsSpiritCasterAggroSkillbar(mesmerBar, 8u));
    GWA3_ASSERT_EQ(
        static_cast<int>(Froggy::ResolveAggroMoveEnemyGateRange(spiritBar, 8u, fightRange)),
        static_cast<int>(Froggy::AGGRO_SPIRIT_CASTER_ENEMY_GATE_RANGE));
    GWA3_ASSERT_EQ(
        static_cast<int>(Froggy::ResolveAggroMoveEnemyGateRange(mesmerBar, 8u, fightRange)),
        static_cast<int>(GWA3::DungeonCombat::ComputeLocalClearRange(fightRange)));
})
} // namespace GWA3::Tests::Consolidated::test_froggy_aggro_move_policy

// --- tests/test_froggy_post_sparkfly_run_decision.cpp ---
namespace GWA3::Tests::Consolidated::test_froggy_post_sparkfly_run_decision {

namespace Froggy = GWA3::Bot::Froggy;

GWA3_TEST(froggy_post_sparkfly_return_only_enters_maintenance_lane_when_needed, {
    const auto normal = Froggy::ResolvePostSparkflyRunDecision(false);
    const auto maintenance = Froggy::ResolvePostSparkflyRunDecision(true);

    GWA3_ASSERT_EQ(static_cast<int>(normal.next_state),
                   static_cast<int>(GWA3::Bot::BotState::InDungeon));
    GWA3_ASSERT(!normal.maintenance_deferred);
    GWA3_ASSERT_EQ(static_cast<int>(maintenance.next_state),
                   static_cast<int>(GWA3::Bot::BotState::Merchant));
    GWA3_ASSERT(!maintenance.maintenance_deferred);
})

} // namespace GWA3::Tests::Consolidated::test_froggy_post_sparkfly_run_decision

// --- tests/test_froggy_config_quest_plans.cpp ---
namespace GWA3::Tests::Consolidated::test_froggy_config_quest_plans {

namespace Froggy = GWA3::Bot::Froggy;

GWA3_TEST(froggy_config_quest_plans, {
    const auto entry = Froggy::GetEntryBootstrapPlan();
    const auto rewardNpc = Froggy::GetRewardNpcAnchor();
    const auto rewardDialog = Froggy::GetRewardDialogPlan();
    uint32_t expanded[8] = {};

    GWA3_ASSERT_EQ(GWA3::QuestIds::TEKKS_WAR, 0x339u);
    GWA3_ASSERT(GWA3::DungeonQuest::IsValidBootstrapPlan(entry));
    GWA3_ASSERT_EQ(entry.entry_map_id, GWA3::MapIds::SPARKFLY_SWAMP);
    GWA3_ASSERT_EQ(entry.target_map_id, GWA3::MapIds::BOGROOT_GROWTHS_LVL1);
    GWA3_ASSERT_EQ(entry.entry_path_count, 3);
    GWA3_ASSERT_EQ(static_cast<int>(entry.zone_point.x), 13097);
    GWA3_ASSERT_EQ(static_cast<int>(entry.zone_point.y), 26393);
    GWA3_ASSERT_EQ(static_cast<int>(entry.npc.x), 12396);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::SPARKFLY_TEKKS_STAGE.x), 12061);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::SPARKFLY_DUNGEON_ENTRY_STAGE.y), 22677);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::BOSS_CHEST_X), 14876);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::BOGROOT_LVL1_TO_LVL2_PORTAL.x), 7665);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::BOGROOT_LVL1_TO_LVL2_PORTAL.y), -19050);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::BOGROOT_REVERSE_TO_SPARKFLY_STAGE.x), 14747);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::BOGROOT_RETURN_TO_SPARKFLY_STAGE.x), 14876);
    GWA3_ASSERT_EQ(static_cast<int>(Froggy::BOGROOT_RETURN_TO_SPARKFLY_PUSH.y), 450);
    GWA3_ASSERT_EQ(Froggy::BOGROOT_DUNGEON_MAP_COUNT, 2);
    GWA3_ASSERT(Froggy::IsBogrootMapId(GWA3::MapIds::BOGROOT_GROWTHS_LVL1));
    GWA3_ASSERT(Froggy::IsBogrootMapId(GWA3::MapIds::BOGROOT_GROWTHS_LVL2));
    GWA3_ASSERT(!Froggy::IsBogrootMapId(GWA3::MapIds::SPARKFLY_SWAMP));
    GWA3_ASSERT(Froggy::IsTekksQuestReadyForDungeonEntry(true, 0u));
    GWA3_ASSERT(Froggy::IsTekksQuestReadyForDungeonEntry(false, GWA3::QuestIds::TEKKS_WAR));
    GWA3_ASSERT(!Froggy::IsTekksQuestReadyForDungeonEntry(false, 0u));
    GWA3_ASSERT(Froggy::IsTekksDungeonEntryConfirmed(true, 0u, true));
    GWA3_ASSERT(Froggy::IsTekksDungeonEntryConfirmed(false, GWA3::QuestIds::TEKKS_WAR, true));
    GWA3_ASSERT(!Froggy::IsTekksDungeonEntryConfirmed(false, 0u, true));
    GWA3_ASSERT(!Froggy::IsTekksDungeonEntryConfirmed(true, 0u, false));
    GWA3_ASSERT_EQ(Froggy::TEKKS_DIALOG_RESET_FAILURE_THRESHOLD, 2);
    GWA3_ASSERT(!(2 > Froggy::TEKKS_DIALOG_RESET_FAILURE_THRESHOLD));
    GWA3_ASSERT(3 > Froggy::TEKKS_DIALOG_RESET_FAILURE_THRESHOLD);
    GWA3_ASSERT_EQ(static_cast<int>(rewardNpc.x), 14618);
    GWA3_ASSERT(GWA3::DungeonQuest::IsValidDialogPlan(rewardDialog));
    GWA3_ASSERT_EQ(GWA3::DungeonQuest::ExpandDialogSequence(entry, expanded, 8), 4);
    GWA3_ASSERT_EQ(expanded[0], 0x2AE6u);
    GWA3_ASSERT_EQ(expanded[1], 0x833901u);
    GWA3_ASSERT_EQ(expanded[3], 0x833901u);
    GWA3_ASSERT_EQ(GWA3::DungeonQuest::GetExpandedDialogCount(rewardDialog), 3);
})
} // namespace GWA3::Tests::Consolidated::test_froggy_config_quest_plans

// --- tests/test_froggy_route_definitions.cpp ---
namespace GWA3::Tests::Consolidated::test_froggy_route_definitions {


GWA3_TEST(froggy_route_definitions, {
    GWA3_ASSERT_EQ(GWA3::Bot::Froggy::SPARKFLY_TO_DUNGEON_COUNT, 9);
    GWA3_ASSERT_EQ(GWA3::Bot::Froggy::BOGROOT_LVL1_COUNT, 28);
    GWA3_ASSERT_EQ(GWA3::Bot::Froggy::BOGROOT_LVL2_COUNT, 36);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::Froggy::BOGROOT_LVL2[11].fight_range), 1600);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::Froggy::BOGROOT_LVL2[31].x), 13583);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::Froggy::BOGROOT_LVL2[32].x), 14120);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::Froggy::BOGROOT_LVL2[33].x), 14450);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::Froggy::BOGROOT_LVL2[34].x), 14850);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::Froggy::BOGROOT_LVL2[35].x), 15117);
})
} // namespace GWA3::Tests::Consolidated::test_froggy_route_definitions

// --- tests/test_froggy_route_policy_checkpoint_progress.cpp ---
namespace GWA3::Tests::Consolidated::test_froggy_route_policy_checkpoint_progress {

GWA3_TEST(froggy_route_policy_checkpoint_progress, {
    const GWA3::DungeonCheckpoint::CheckpointRetryPolicy policies[] = {
        {
            "Dungeon Door Checkpoint",
            GWA3::DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry,
            3,
            6,
        },
        {
            "Quest Door Checkpoint",
            GWA3::DungeonCheckpoint::CheckpointFailureAction::AbortRun,
            0,
            3,
        },
    };
    const auto doorPassed = GWA3::DungeonCheckpoint::EvaluateLabeledAdvanceCheckpointProgress(
        "Dungeon Door Checkpoint", 6, 7, 20, policies, 2);
    const auto doorFailed = GWA3::DungeonCheckpoint::EvaluateLabeledAdvanceCheckpointProgress(
        "Dungeon Door Checkpoint", 6, 6, 20, policies, 2);
    const auto questFailed = GWA3::DungeonCheckpoint::EvaluateLabeledAdvanceCheckpointProgress(
        "Quest Door Checkpoint", 3, 3, 20, policies, 2);
    const auto normal = GWA3::DungeonCheckpoint::EvaluateLabeledAdvanceCheckpointProgress(
        "1", 1, 1, 20, policies, 2);

    GWA3_ASSERT(doorPassed.passed);
    GWA3_ASSERT(!doorFailed.passed);
    GWA3_ASSERT_EQ(static_cast<int>(doorFailed.action),
                   static_cast<int>(GWA3::DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry));
    GWA3_ASSERT_EQ(doorFailed.backtrack_start, 3);
    GWA3_ASSERT_EQ(doorFailed.retry_index, 6);
    GWA3_ASSERT(doorFailed.retry_from_nearest_after_backtrack);
    GWA3_ASSERT(!questFailed.passed);
    GWA3_ASSERT_EQ(static_cast<int>(questFailed.action),
                   static_cast<int>(GWA3::DungeonCheckpoint::CheckpointFailureAction::AbortRun));
    GWA3_ASSERT(!normal.retry_from_nearest_after_backtrack);
    GWA3_ASSERT(normal.passed);
})
} // namespace GWA3::Tests::Consolidated::test_froggy_route_policy_checkpoint_progress

// --- tests/test_froggy_route_policy_waypoint_lookup.cpp ---
namespace GWA3::Tests::Consolidated::test_froggy_route_policy_waypoint_lookup {

namespace Froggy = GWA3::Bot::Froggy;

GWA3_TEST(froggy_route_policy_waypoint_lookup, {
    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "Blessing"},
        {500.0f, 0.0f, 0.0f, "Boss"},
    };

    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel("Blessing")),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::Blessing));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel("Lvl1 to Lvl2")),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::LevelTransition));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel("Dungeon Key")),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::DungeonKey));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel("Dungeon Door")),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::DungeonDoor));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel("Dungeon Door Checkpoint")),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::DungeonDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel("Quest Door Checkpoint")),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::QuestDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel("Boss")),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::Boss));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(waypoints[2].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::Boss));

    GWA3::DungeonCheckpoint::AdvanceCheckpointDecision retryDecision;
    retryDecision.retry_from_nearest_after_backtrack = true;
    retryDecision.retry_index = 6;
    GWA3_ASSERT_EQ(GWA3::DungeonCheckpoint::ResolveAdvanceCheckpointRetryIndex(retryDecision, 7), 6);
    GWA3_ASSERT_EQ(GWA3::DungeonCheckpoint::ResolveAdvanceCheckpointRetryIndex(retryDecision, 0), 0);

    GWA3::DungeonCheckpoint::AdvanceCheckpointDecision fixedDecision;
    fixedDecision.retry_index = 4;
    GWA3_ASSERT_EQ(GWA3::DungeonCheckpoint::ResolveAdvanceCheckpointRetryIndex(fixedDecision, 7), 4);

})

} // namespace GWA3::Tests::Consolidated::test_froggy_route_policy_waypoint_lookup
