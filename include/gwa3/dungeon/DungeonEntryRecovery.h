#pragma once

#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonQuest.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/dungeon/DungeonRuntime.h>

#include <cstdint>

namespace GWA3::DungeonEntryRecovery {

struct EntryFailureTracker {
    int count = 0;
    int reset_threshold = 5;
};

struct TransitionPlan {
    uint32_t required_start_map_id = 0u;
    uint32_t target_map_id = 0u;
    DungeonRuntime::TransitionAnchor stage = {};
    DungeonRuntime::TransitionAnchor push = {};
    float stage_threshold = 250.0f;
    uint32_t pre_push_delay_ms = 1000u;
    uint32_t ready_timeout_ms = 60000u;
    uint32_t push_timeout_ms = 30000u;
    uint32_t push_interval_ms = 3000u;
    DungeonRuntime::MoveToPointFn move_to_point = nullptr;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct DungeonEntryPlan {
    DungeonQuest::BootstrapPlan bootstrap = {};
    float path_threshold = 250.0f;
    uint32_t route_dwell_ms = 1000u;
    uint32_t ready_timeout_ms = 60000u;
    uint32_t push_timeout_ms = 30000u;
    uint32_t push_interval_ms = 3000u;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct DialogResetBouncePlan {
    uint32_t required_start_map_id = 0u;
    DungeonEntryPlan enter = {};
    TransitionPlan return_to_quest_map = {};
    uint32_t settle_ms = 1000u;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct QuestDoorRecoveryOptions {
    uint32_t quest_id = 0u;
    DungeonQuestRuntime::QuestReadyOptions quest_ready = {};
    const DungeonRoute::Waypoint* waypoints = nullptr;
    int waypoint_count = 0;
    int current_index = 0;
    int backtrack_steps = 3;
    DungeonCheckpoint::WaypointMoveFn move_waypoint = nullptr;
    DungeonCheckpoint::WaypointReplayVisitFn after_move_waypoint = nullptr;
    TransitionPlan return_to_quest_map = {};
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct QuestDoorRecoveryResult {
    bool quest_ready = false;
    bool recovery_triggered = false;
    bool returned_to_quest_map = false;
    uint32_t final_map_id = 0u;
};

struct BacktrackReturnOptions {
    const DungeonRoute::Waypoint* waypoints = nullptr;
    int waypoint_count = 0;
    int current_index = 0;
    int backtrack_steps = 3;
    DungeonCheckpoint::WaypointMoveFn move_waypoint = nullptr;
    DungeonCheckpoint::WaypointReplayVisitFn after_move_waypoint = nullptr;
    TransitionPlan return_to_quest_map = {};
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct BacktrackReturnResult {
    bool returned_to_quest_map = false;
    uint32_t final_map_id = 0u;
};

void ResetEntryFailureTracker(EntryFailureTracker& tracker, const char* reason, const char* logPrefix = nullptr);
bool ExecuteTransitionPlan(const TransitionPlan& plan);
bool EnterDungeonFromQuestMap(const DungeonEntryPlan& plan);
bool BounceThroughDungeonToResetDialog(const DialogResetBouncePlan& plan);
bool RecordEntryFailureAndMaybeResetDialog(
    EntryFailureTracker& tracker,
    const DialogResetBouncePlan& plan,
    const char* context = nullptr);
BacktrackReturnResult ReplayBacktrackAndReturnToQuestMap(const BacktrackReturnOptions& options);
QuestDoorRecoveryResult HandleQuestDoorRecovery(const QuestDoorRecoveryOptions& options);

} // namespace GWA3::DungeonEntryRecovery
