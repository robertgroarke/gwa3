#pragma once

#include <gwa3/bot/DungeonCheckpoint.h>
#include <gwa3/bot/DungeonInteractions.h>
#include <gwa3/bot/DungeonQuest.h>
#include <gwa3/bot/DungeonQuestRuntime.h>
#include <gwa3/bot/DungeonRoute.h>

#include <cstdint>

namespace GWA3::Bot::FroggySharedStaging {

struct ChestOpenOptions {
    float signpost_search_radius = 1500.0f;
    float item_search_radius = 18000.0f;
    int interact_count = 2;
    int pickup_attempts = 3;
    uint32_t interact_delay_ms = 500u;
    uint32_t pickup_delay_ms = 500u;
};

struct DoorOpenOptions {
    float signpost_search_radius = 1500.0f;
    int interact_count = 6;
    uint32_t interact_delay_ms = 500u;
};

using MoveToPointFn = void(*)(float x, float y, float threshold);
using MoveWaypointFn = void(*)(float x, float y, float value);
using QueueMoveFn = void(*)(float x, float y);
using IsMapLoadedFn = bool(*)();
using GetMapIdFn = uint32_t(*)();
using WaitMsFn = void(*)(uint32_t ms);

struct BlessingAcquireOptions {
    float npc_search_radius = 2000.0f;
    uint32_t required_title_id = 0x27u;
    uint32_t accept_dialog_id = 0x84u;
    int interact_count = 3;
    int dialog_retries = 1;
    uint32_t title_settle_delay_ms = 1000u;
    uint32_t interact_delay_ms = 1000u;
    uint32_t dialog_delay_ms = 2000u;
    bool toggle_dialog_hooks = true;
};

struct BlessingAcquireResult {
    bool already_active = false;
    bool npc_found = false;
    bool interacted = false;
    bool dialog_sent = false;
    bool confirmed = false;
    bool title_applied = false;
    bool dialog_hooks_toggled = false;
    uint32_t npc_id = 0u;
    uint32_t final_title_id = 0u;
};

struct RewardClaimOptions {
    bool allow_dialog_without_npc = true;
    DungeonQuestRuntime::DialogExecutionOptions execution = {};
};

struct RewardClaimResult {
    bool npc_found = false;
    bool dialog_sent = false;
    uint32_t npc_id = 0u;
};

struct BossRewardOptions {
    uint32_t current_map_id = 0u;
    float chest_x = 14876.0f;
    float chest_y = -19033.0f;
    ChestOpenOptions chest = {};
    RewardClaimOptions reward = {};
};

struct BossRewardResult {
    bool chest_opened = false;
    bool reward_dialog_sent = false;
    bool reward_npc_found = false;
};

enum class WaypointBehavior : uint8_t {
    StandardMove,
    GrabBlessing,
    OpenDungeonDoor,
    ValidateDungeonDoorCheckpoint,
    ValidateQuestDoorCheckpoint,
    BossRewardSequence,
};

struct WaypointExecutionPlan {
    const DungeonRoute::Waypoint* waypoint = nullptr;
    WaypointBehavior behavior = WaypointBehavior::StandardMove;
};

struct CheckpointDecision {
    bool passed = true;
    DungeonCheckpoint::CheckpointFailureAction action = DungeonCheckpoint::CheckpointFailureAction::None;
    int backtrack_start = 0;
    int retry_index = 0;
    bool retry_from_nearest_after_backtrack = false;
};

struct WaypointProgressState {
    int last_nearest_index = 0;
    int unchanged_nearest_count = 0;
};

struct WaypointProgressDecision {
    bool backtrack = false;
    int backtrack_index = 0;
};

int GetNearestWaypointIndex(const DungeonRoute::Waypoint* waypoints, int count, float x, float y);
bool SendDialogWithRetry(uint32_t dialogId, int maxRetries = 3, uint32_t delayMs = 1000u);
bool IsChestGadgetId(uint32_t gadgetId);
uint32_t FindNearestSignpost(float x, float y, float maxDist);
uint32_t GetQuestId();
uint32_t GetRewardDialogId();
uint32_t GetAcceptDialogId();
uint32_t GetNpcTalkDialogId();
uint32_t GetBlessingTitleId();
uint32_t GetBlessingAcceptDialogId();
DungeonQuest::BootstrapPlan GetEntryBootstrapPlan();
DungeonQuest::QuestNpcAnchor GetRewardNpcAnchor();
DungeonQuest::DialogPlan GetRewardDialogPlan();
bool HasBlessing();
BlessingAcquireResult TryAcquireBlessingAt(
    float shrineX,
    float shrineY,
    const BlessingAcquireOptions& options = {});
RewardClaimResult TryClaimReward(const RewardClaimOptions& options = {});
BossRewardResult ExecuteBossRewardSequence(
    DungeonInteractions::OpenedChestTracker& tracker,
    const BossRewardOptions& options = {});
WaypointBehavior ResolveWaypointBehavior(const char* label);
WaypointExecutionPlan BuildWaypointExecutionPlan(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int waypointIndex);
int ComputeWipeRestartWaypoint(int nearestIndex);
int ComputeStuckBacktrackWaypoint(int nearestIndex);
bool ShouldBacktrackForStuck(int unchangedNearestCount);
CheckpointDecision EvaluateCheckpointProgress(
    const char* label,
    int currentIndex,
    int nearestIndex,
    int waypointCount);
WaypointProgressState MakeWaypointProgressState(int start_index);
WaypointProgressDecision EvaluateWaypointProgress(
    int current_nearest_index,
    WaypointProgressState& state);
int ResolveCheckpointRetryIndex(
    const CheckpointDecision& decision,
    int nearest_index);
bool TryOpenChestAt(
    float x,
    float y,
    uint32_t currentMapId,
    DungeonInteractions::OpenedChestTracker& tracker,
    const ChestOpenOptions& options = {});
bool TryOpenDoorAt(
    float x,
    float y,
    const DoorOpenOptions& options = {});
bool ExecuteDoorOpenSequence(
    float x,
    float y,
    MoveToPointFn move_to_point,
    const DoorOpenOptions& options = {},
    float settle_threshold = 200.0f,
    uint32_t settle_delay_ms = 500u);
void ExecuteWaypointTravel(
    const DungeonRoute::Waypoint& waypoint,
    MoveWaypointFn move_to_point,
    MoveWaypointFn aggro_move_to_point,
    IsMapLoadedFn is_map_loaded,
    float move_threshold = 250.0f);
void ExecuteReverseWaypointTraversal(
    const DungeonRoute::Waypoint* waypoints,
    int start_index,
    int end_index,
    MoveWaypointFn move_to_point,
    MoveWaypointFn aggro_move_to_point,
    IsMapLoadedFn is_map_loaded,
    float move_threshold = 250.0f);
bool ExecuteMapTransitionMove(
    float x,
    float y,
    uint32_t target_map_id,
    MoveToPointFn move_to_point,
    QueueMoveFn queue_move,
    GetMapIdFn get_map_id,
    WaitMsFn wait_ms,
    uint32_t timeout_ms = 60000u,
    uint32_t pulse_delay_ms = 250u,
    float settle_threshold = 250.0f);

} // namespace GWA3::Bot::FroggySharedStaging
