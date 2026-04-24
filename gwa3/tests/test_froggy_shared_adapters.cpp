#include <froggy_shared/FroggySharedAdapters.h>

#include <gwa3/testing/TestFramework.h>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace ItemStubs = GWA3::TestStubs::ItemMgr;
namespace QuestStubs = GWA3::TestStubs::QuestMgr;
namespace FroggyShared = GWA3::Bot::FroggySharedStaging;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
uint32_t MoveCount();
uint32_t LastInteractedNpcId();
uint32_t NpcInteractionCount();
uint32_t LastInteractedSignpostId();
uint32_t SignpostInteractionCount();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::ItemMgr {

void Reset();
uint32_t LastPickedItemAgentId();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs();
std::size_t DialogCount();
uint32_t DialogAt(std::size_t index);
void SetBlessingDialogEffect(uint32_t dialogId, uint32_t skillId);

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::TestStubs::EffectMgr {

void Reset();
void AddEffect(uint32_t skillId);

} // namespace GWA3::TestStubs::EffectMgr

namespace GWA3::TestStubs::PlayerMgr {

void Reset();

} // namespace GWA3::TestStubs::PlayerMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset();
uint32_t ShutdownCount();
uint32_t InitializeCount();

} // namespace GWA3::TestStubs::DialogMgr

namespace GWA3::TestStubs::MapMgr {

void Reset();
void SetMapId(uint32_t mapId);
void SetMoveSetsMapId(uint32_t mapId);
void SetMoveSetsMapIdOnPoint(float x, float y, uint32_t mapId);

} // namespace GWA3::TestStubs::MapMgr

namespace {

float g_lastMoveX = 0.0f;
float g_lastMoveY = 0.0f;
float g_lastMoveThreshold = 0.0f;
int g_moveCallCount = 0;
float g_moveHistoryX[16] = {};
float g_moveHistoryY[16] = {};
float g_moveHistoryValue[16] = {};
int g_moveHistoryCount = 0;

void ResetMoveRecorder() {
    g_lastMoveX = 0.0f;
    g_lastMoveY = 0.0f;
    g_lastMoveThreshold = 0.0f;
    g_moveCallCount = 0;
    g_moveHistoryCount = 0;
}

void RecordMoveToPoint(float x, float y, float threshold) {
    g_lastMoveX = x;
    g_lastMoveY = y;
    g_lastMoveThreshold = threshold;
    if (g_moveHistoryCount < 16) {
        g_moveHistoryX[g_moveHistoryCount] = x;
        g_moveHistoryY[g_moveHistoryCount] = y;
        g_moveHistoryValue[g_moveHistoryCount] = threshold;
    }
    ++g_moveHistoryCount;
    ++g_moveCallCount;
}

void RecordAggroMoveToPoint(float x, float y, float fightRange) {
    RecordMoveToPoint(x, y, fightRange);
}

void AgentMoveToPoint(float x, float y, float) {
    GWA3::AgentMgr::Move(x, y);
}

void AgentQueueMove(float x, float y) {
    GWA3::AgentMgr::Move(x, y);
}

bool MapLoadedTrue() {
    return true;
}

bool MapLoadedFalse() {
    return false;
}

void NoWait(uint32_t) {
}

} // namespace

GWA3_TEST(froggy_stage_waypoint_lookup, {
    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "Blessing"},
        {500.0f, 0.0f, 0.0f, "Boss"},
    };

    GWA3_ASSERT_EQ(FroggyShared::GetNearestWaypointIndex(waypoints, 3, 110.0f, 10.0f), 1);
    GWA3_ASSERT_EQ(static_cast<int>(FroggyShared::ResolveWaypointBehavior("Blessing")),
                   static_cast<int>(FroggyShared::WaypointBehavior::GrabBlessing));
    GWA3_ASSERT_EQ(static_cast<int>(FroggyShared::ResolveWaypointBehavior("Dungeon Door")),
                   static_cast<int>(FroggyShared::WaypointBehavior::OpenDungeonDoor));
    GWA3_ASSERT_EQ(static_cast<int>(FroggyShared::ResolveWaypointBehavior("Dungeon Door Checkpoint")),
                   static_cast<int>(FroggyShared::WaypointBehavior::ValidateDungeonDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(FroggyShared::ResolveWaypointBehavior("Quest Door Checkpoint")),
                   static_cast<int>(FroggyShared::WaypointBehavior::ValidateQuestDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(FroggyShared::ResolveWaypointBehavior("Boss")),
                   static_cast<int>(FroggyShared::WaypointBehavior::BossRewardSequence));
    const auto plan = FroggyShared::BuildWaypointExecutionPlan(waypoints, 3, 2);
    GWA3_ASSERT(plan.waypoint != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(plan.behavior),
                   static_cast<int>(FroggyShared::WaypointBehavior::BossRewardSequence));
    GWA3_ASSERT_EQ(FroggyShared::ComputeWipeRestartWaypoint(7), 5);
    GWA3_ASSERT_EQ(FroggyShared::ComputeWipeRestartWaypoint(1), 0);
    GWA3_ASSERT_EQ(FroggyShared::ComputeStuckBacktrackWaypoint(4), 3);
    GWA3_ASSERT(FroggyShared::ShouldBacktrackForStuck(5));
    GWA3_ASSERT(!FroggyShared::ShouldBacktrackForStuck(4));

    auto progressState = FroggyShared::MakeWaypointProgressState(2);
    GWA3_ASSERT(!FroggyShared::EvaluateWaypointProgress(2, progressState).backtrack);
    GWA3_ASSERT(!FroggyShared::EvaluateWaypointProgress(2, progressState).backtrack);
    GWA3_ASSERT(!FroggyShared::EvaluateWaypointProgress(2, progressState).backtrack);
    GWA3_ASSERT(!FroggyShared::EvaluateWaypointProgress(2, progressState).backtrack);
    const auto stuckBacktrack = FroggyShared::EvaluateWaypointProgress(2, progressState);
    GWA3_ASSERT(stuckBacktrack.backtrack);
    GWA3_ASSERT_EQ(stuckBacktrack.backtrack_index, 1);

    const auto progressReset = FroggyShared::EvaluateWaypointProgress(4, progressState);
    GWA3_ASSERT(!progressReset.backtrack);
    GWA3_ASSERT_EQ(progressState.last_nearest_index, 4);
    GWA3_ASSERT_EQ(progressState.unchanged_nearest_count, 0);

    FroggyShared::CheckpointDecision retryDecision;
    retryDecision.retry_from_nearest_after_backtrack = true;
    retryDecision.retry_index = 6;
    GWA3_ASSERT_EQ(FroggyShared::ResolveCheckpointRetryIndex(retryDecision, 7), 6);
    GWA3_ASSERT_EQ(FroggyShared::ResolveCheckpointRetryIndex(retryDecision, 0), 0);

    FroggyShared::CheckpointDecision fixedDecision;
    fixedDecision.retry_index = 4;
    GWA3_ASSERT_EQ(FroggyShared::ResolveCheckpointRetryIndex(fixedDecision, 7), 4);
})

GWA3_TEST(froggy_stage_dialog_retry, {
    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::EffectMgr::Reset();
    GWA3::TestStubs::PlayerMgr::Reset();
    GWA3::TestStubs::DialogMgr::Reset();

    GWA3_ASSERT(FroggyShared::SendDialogWithRetry(0x833901u, 3, 0u));
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(3));
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(0), 0x833901u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(2), 0x833901u);

    const auto entry = FroggyShared::GetEntryBootstrapPlan();
    const auto rewardNpc = FroggyShared::GetRewardNpcAnchor();
    const auto rewardDialog = FroggyShared::GetRewardDialogPlan();
    uint32_t expanded[8] = {};

    GWA3_ASSERT_EQ(FroggyShared::GetQuestId(), 0x339u);
    GWA3_ASSERT(GWA3::Bot::DungeonQuest::IsValidBootstrapPlan(entry));
    GWA3_ASSERT_EQ(entry.entry_map_id, 558u);
    GWA3_ASSERT_EQ(entry.target_map_id, 615u);
    GWA3_ASSERT_EQ(entry.entry_path_count, 3);
    GWA3_ASSERT_EQ(static_cast<int>(entry.zone_point.x), 13097);
    GWA3_ASSERT_EQ(static_cast<int>(entry.zone_point.y), 26393);
    GWA3_ASSERT_EQ(static_cast<int>(entry.npc.x), 12396);
    GWA3_ASSERT_EQ(static_cast<int>(rewardNpc.x), 14618);
    GWA3_ASSERT(GWA3::Bot::DungeonQuest::IsValidDialogPlan(rewardDialog));
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::ExpandDialogSequence(entry, expanded, 8), 4);
    GWA3_ASSERT_EQ(expanded[0], 0x2AE6u);
    GWA3_ASSERT_EQ(expanded[1], 0x833901u);
    GWA3_ASSERT_EQ(expanded[3], 0x833901u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(rewardDialog), 3);

    FroggyShared::RewardClaimOptions rewardOptions;
    rewardOptions.execution.interact_delay_ms = 0u;
    rewardOptions.execution.post_interact_delay_ms = 0u;
    rewardOptions.execution.dialog_delay_ms = 0u;
    rewardOptions.execution.repeat_delay_ms = 0u;
    rewardOptions.execution.interact_count = 3;
    rewardOptions.execution.max_retries_per_dialog = 1;

    AgentStubs::AddNpc(77u, 14610.0f, -17820.0f, 6u, 1.0f);
    QuestStubs::ResetDialogs();
    const auto rewardWithNpc = FroggyShared::TryClaimReward(rewardOptions);

    GWA3_ASSERT(rewardWithNpc.npc_found);
    GWA3_ASSERT(rewardWithNpc.dialog_sent);
    GWA3_ASSERT_EQ(rewardWithNpc.npc_id, 77u);
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedNpcId(), 77u);
    GWA3_ASSERT_EQ(AgentStubs::NpcInteractionCount(), 3u);
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(3));
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(0), 0x833907u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(2), 0x833907u);

    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    const auto rewardWithoutNpc = FroggyShared::TryClaimReward(rewardOptions);

    GWA3_ASSERT(!rewardWithoutNpc.npc_found);
    GWA3_ASSERT(rewardWithoutNpc.dialog_sent);
    GWA3_ASSERT_EQ(rewardWithoutNpc.npc_id, 0u);
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(3));
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(1), 0x833907u);

    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    QuestStubs::ResetDialogs();
    AgentStubs::AddAgent(30u, 14880.0f, -19020.0f, 0x200u);
    AgentStubs::AddAgent(31u, 14895.0f, -19010.0f, 0x400u);
    AgentStubs::AddNpc(91u, 14610.0f, -17820.0f, 6u, 1.0f);

    GWA3::Bot::DungeonInteractions::OpenedChestTracker tracker;
    FroggyShared::BossRewardOptions bossOptions;
    bossOptions.current_map_id = 615u;
    bossOptions.chest.signpost_search_radius = 1500.0f;
    bossOptions.chest.item_search_radius = 1500.0f;
    bossOptions.chest.interact_count = 1;
    bossOptions.chest.pickup_attempts = 1;
    bossOptions.chest.interact_delay_ms = 0u;
    bossOptions.chest.pickup_delay_ms = 0u;
    bossOptions.reward = rewardOptions;

    const auto bossResult = FroggyShared::ExecuteBossRewardSequence(tracker, bossOptions);
    GWA3_ASSERT(bossResult.chest_opened);
    GWA3_ASSERT(bossResult.reward_dialog_sent);
    GWA3_ASSERT(bossResult.reward_npc_found);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 31u);
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(3));
})

GWA3_TEST(froggy_stage_blessing, {
    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::EffectMgr::Reset();
    GWA3::TestStubs::PlayerMgr::Reset();
    GWA3::TestStubs::DialogMgr::Reset();

    AgentStubs::AddNpc(90u, 100.0f, 50.0f, 6u, 1.0f);
    QuestStubs::SetBlessingDialogEffect(0x84u, 2549u);

    FroggyShared::BlessingAcquireOptions options;
    options.interact_count = 3;
    options.dialog_retries = 1;
    options.title_settle_delay_ms = 0u;
    options.interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;

    GWA3_ASSERT(!FroggyShared::HasBlessing());
    const auto result = FroggyShared::TryAcquireBlessingAt(0.0f, 0.0f, options);

    GWA3_ASSERT(!result.already_active);
    GWA3_ASSERT(result.npc_found);
    GWA3_ASSERT(result.interacted);
    GWA3_ASSERT(result.dialog_sent);
    GWA3_ASSERT(result.confirmed);
    GWA3_ASSERT(result.title_applied);
    GWA3_ASSERT(result.dialog_hooks_toggled);
    GWA3_ASSERT_EQ(result.npc_id, 90u);
    GWA3_ASSERT_EQ(result.final_title_id, 0x27u);
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedNpcId(), 90u);
    GWA3_ASSERT_EQ(AgentStubs::NpcInteractionCount(), 3u);
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(1));
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(0), 0x84u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::DialogMgr::ShutdownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::DialogMgr::InitializeCount(), 1u);
    GWA3_ASSERT(FroggyShared::HasBlessing());
})

GWA3_TEST(froggy_stage_chest_ids, {
    GWA3_ASSERT(FroggyShared::IsChestGadgetId(8141u));
    GWA3_ASSERT(FroggyShared::IsChestGadgetId(4582u));
    GWA3_ASSERT(!FroggyShared::IsChestGadgetId(9999u));
})

GWA3_TEST(froggy_stage_find_signpost, {
    AgentStubs::ResetAgents();
    AgentStubs::AddAgent(10u, 300.0f, 0.0f, 0x200u);
    AgentStubs::AddAgent(11u, 80.0f, 0.0f, 0x200u);

    GWA3_ASSERT_EQ(FroggyShared::FindNearestSignpost(0.0f, 0.0f, 1000.0f), 11u);
})

GWA3_TEST(froggy_stage_open_chest, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::AddAgent(30u, 40.0f, 0.0f, 0x200u);
    AgentStubs::AddAgent(31u, 55.0f, 0.0f, 0x400u);

    GWA3::Bot::DungeonInteractions::OpenedChestTracker tracker;
    GWA3_ASSERT(FroggyShared::TryOpenChestAt(0.0f, 0.0f, 100u, tracker));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 30u);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 31u);

    GWA3_ASSERT(!FroggyShared::TryOpenChestAt(0.0f, 0.0f, 100u, tracker));
    GWA3_ASSERT(FroggyShared::TryOpenChestAt(0.0f, 0.0f, 101u, tracker));
})

GWA3_TEST(froggy_stage_open_door, {
    AgentStubs::ResetAgents();
    ResetMoveRecorder();
    AgentStubs::AddAgent(50u, 70.0f, 0.0f, 0x200u);

    FroggyShared::DoorOpenOptions options;
    options.interact_count = 6;
    options.interact_delay_ms = 0u;

    GWA3_ASSERT(FroggyShared::TryOpenDoorAt(0.0f, 0.0f, options));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 50u);
    GWA3_ASSERT_EQ(AgentStubs::SignpostInteractionCount(), 6u);
    GWA3_ASSERT(FroggyShared::ExecuteDoorOpenSequence(0.0f, 0.0f, &RecordMoveToPoint, options, 200.0f, 0u));
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveX), 0);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveY), 0);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 200);

    const auto doorPassed = FroggyShared::EvaluateCheckpointProgress("Dungeon Door Checkpoint", 6, 7, 20);
    const auto doorFailed = FroggyShared::EvaluateCheckpointProgress("Dungeon Door Checkpoint", 6, 6, 20);
    const auto questFailed = FroggyShared::EvaluateCheckpointProgress("Quest Door Checkpoint", 3, 3, 20);
    const auto normal = FroggyShared::EvaluateCheckpointProgress("1", 1, 1, 20);

    GWA3_ASSERT(doorPassed.passed);
    GWA3_ASSERT(!doorFailed.passed);
    GWA3_ASSERT_EQ(static_cast<int>(doorFailed.action),
                   static_cast<int>(GWA3::Bot::DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry));
    GWA3_ASSERT_EQ(doorFailed.backtrack_start, 3);
    GWA3_ASSERT_EQ(doorFailed.retry_index, 6);
    GWA3_ASSERT(doorFailed.retry_from_nearest_after_backtrack);
    GWA3_ASSERT(!questFailed.passed);
    GWA3_ASSERT_EQ(static_cast<int>(questFailed.action),
                   static_cast<int>(GWA3::Bot::DungeonCheckpoint::CheckpointFailureAction::AbortRun));
    GWA3_ASSERT(!normal.retry_from_nearest_after_backtrack);
    GWA3_ASSERT(normal.passed);
})

GWA3_TEST(froggy_stage_open_door_sequence_returns_false_without_signpost, {
    AgentStubs::ResetAgents();
    ResetMoveRecorder();

    FroggyShared::DoorOpenOptions options;
    options.interact_count = 6;
    options.interact_delay_ms = 0u;

    GWA3_ASSERT(!FroggyShared::ExecuteDoorOpenSequence(0.0f, 0.0f, &RecordMoveToPoint, options, 200.0f, 0u));
    GWA3_ASSERT_EQ(g_moveCallCount, 0);
})

GWA3_TEST(froggy_stage_waypoint_traversal_policy, {
    ResetMoveRecorder();

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {10.0f, 20.0f, 0.0f, "Move"},
        {30.0f, 40.0f, 1350.0f, "Aggro"},
        {50.0f, 60.0f, 900.0f, "Aggro 2"},
    };

    FroggyShared::ExecuteWaypointTravel(
        waypoints[0],
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedTrue,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveX), 10);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveY), 20);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 250);

    ResetMoveRecorder();
    FroggyShared::ExecuteWaypointTravel(
        waypoints[1],
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedTrue,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 1350);

    ResetMoveRecorder();
    FroggyShared::ExecuteWaypointTravel(
        waypoints[1],
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedFalse,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 250);

    ResetMoveRecorder();
    FroggyShared::ExecuteReverseWaypointTraversal(
        waypoints,
        2,
        0,
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedTrue,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 3);
    GWA3_ASSERT_EQ(g_moveHistoryCount, 3);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryX[0]), 50);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryValue[0]), 900);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryX[1]), 30);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryValue[1]), 1350);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryX[2]), 10);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryValue[2]), 250);
})

GWA3_TEST(froggy_stage_map_transition_move, {
    AgentStubs::ResetAgents();
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(615u);
    GWA3::TestStubs::MapMgr::SetMoveSetsMapIdOnPoint(14747.0f, 480.0f, 558u);

    GWA3_ASSERT(FroggyShared::ExecuteMapTransitionMove(
        14747.0f,
        480.0f,
        558u,
        &AgentMoveToPoint,
        &AgentQueueMove,
        &GWA3::MapMgr::GetMapId,
        &NoWait,
        100u,
        0u,
        250.0f));
    GWA3_ASSERT_EQ(GWA3::MapMgr::GetMapId(), 558u);
    GWA3_ASSERT(AgentStubs::MoveCount() >= 1u);

    AgentStubs::ResetAgents();
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(615u);
    GWA3_ASSERT(!FroggyShared::ExecuteMapTransitionMove(
        14747.0f,
        480.0f,
        558u,
        &AgentMoveToPoint,
        &AgentQueueMove,
        &GWA3::MapMgr::GetMapId,
        &NoWait,
        0u,
        0u,
        250.0f));
    GWA3_ASSERT_EQ(GWA3::MapMgr::GetMapId(), 615u);
})
