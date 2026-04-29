#include <gwa3/dungeon/DungeonEntryRecovery.h>

#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

namespace GWA3::DungeonEntryRecovery {

namespace {

const char* PrefixOrDefault(const char* prefix) {
    return prefix != nullptr ? prefix : "DungeonEntryRecovery";
}

const char* LabelOrDefault(const char* label, const char* fallback) {
    return label != nullptr ? label : fallback;
}

} // namespace

void ResetEntryFailureTracker(EntryFailureTracker& tracker, const char* reason, const char* logPrefix) {
    const char* prefix = PrefixOrDefault(logPrefix);
    if (tracker.count > 0) {
        Log::Info("%s: resetting quest-entry failure count from %d reason=%s",
                  prefix,
                  tracker.count,
                  reason ? reason : "unspecified");
    }
    tracker.count = 0;
}

bool ExecuteTransitionPlan(const TransitionPlan& plan) {
    const char* prefix = PrefixOrDefault(plan.log_prefix);
    const char* label = LabelOrDefault(plan.label, "map transition");
    if (plan.target_map_id == 0u) {
        return false;
    }
    if (plan.required_start_map_id != 0u && MapMgr::GetMapId() != plan.required_start_map_id) {
        Log::Info("%s: %s skipped because map=%u required=%u",
                  prefix,
                  label,
                  MapMgr::GetMapId(),
                  plan.required_start_map_id);
        return false;
    }

    return DungeonRuntime::StageAndPushUntilMapReady(
        plan.target_map_id,
        plan.stage.x,
        plan.stage.y,
        plan.stage_threshold,
        plan.push.x,
        plan.push.y,
        plan.move_to_point,
        plan.pre_push_delay_ms,
        plan.ready_timeout_ms,
        plan.push_timeout_ms,
        plan.push_interval_ms,
        label);
}

bool EnterDungeonFromQuestMap(const DungeonEntryPlan& plan) {
    const char* prefix = PrefixOrDefault(plan.log_prefix);
    const char* label = LabelOrDefault(plan.label, "dungeon entry");
    const auto& bootstrap = plan.bootstrap;
    if (!DungeonQuest::IsValidBootstrapPlan(bootstrap)) {
        Log::Info("%s: %s invalid bootstrap plan", prefix, label);
        return false;
    }
    if (MapMgr::GetMapId() != bootstrap.entry_map_id) {
        Log::Info("%s: %s skipped because map=%u entryMap=%u",
                  prefix,
                  label,
                  MapMgr::GetMapId(),
                  bootstrap.entry_map_id);
        return false;
    }

    for (int i = 0; i < bootstrap.entry_path_count; ++i) {
        const auto& point = bootstrap.entry_path[i];
        if (i + 1 < bootstrap.entry_path_count) {
            (void)DungeonNavigation::MoveToAndWait(
                point.x,
                point.y,
                plan.path_threshold,
                DungeonNavigation::MOVE_TO_TIMEOUT_MS,
                DungeonNavigation::MOVE_TO_POLL_MS,
                bootstrap.entry_map_id);
        } else {
            AgentMgr::Move(point.x, point.y);
        }
    }
    if (plan.route_dwell_ms > 0u) {
        DungeonRuntime::WaitMs(plan.route_dwell_ms);
    }

    return DungeonRuntime::PushUntilMapReady(
        bootstrap.target_map_id,
        bootstrap.zone_point.x,
        bootstrap.zone_point.y,
        plan.ready_timeout_ms,
        plan.push_timeout_ms,
        plan.push_interval_ms,
        label);
}

bool BounceThroughDungeonToResetDialog(const DialogResetBouncePlan& plan) {
    const char* prefix = PrefixOrDefault(plan.log_prefix);
    const char* label = LabelOrDefault(plan.label, "quest dialog reset bounce");
    if (plan.required_start_map_id != 0u && MapMgr::GetMapId() != plan.required_start_map_id) {
        Log::Info("%s: %s skipped because map=%u required=%u",
                  prefix,
                  label,
                  MapMgr::GetMapId(),
                  plan.required_start_map_id);
        return false;
    }

    Log::Info("%s: %s zoning into dungeon and back to reset dialog", prefix, label);
    const bool entered = EnterDungeonFromQuestMap(plan.enter);
    if (!entered) {
        Log::Info("%s: %s failed to enter dungeon finalMap=%u",
                  prefix,
                  label,
                  MapMgr::GetMapId());
        if (plan.settle_ms > 0u) {
            DungeonRuntime::WaitMs(plan.settle_ms);
        }
        return false;
    }

    if (plan.settle_ms > 0u) {
        DungeonRuntime::WaitMs(plan.settle_ms);
    }
    const bool returned = ExecuteTransitionPlan(plan.return_to_quest_map);
    if (plan.settle_ms > 0u) {
        DungeonRuntime::WaitMs(plan.settle_ms);
    }
    Log::Info("%s: %s entered=%d returned=%d finalMap=%u",
              prefix,
              label,
              entered ? 1 : 0,
              returned ? 1 : 0,
              MapMgr::GetMapId());
    return returned &&
           (plan.required_start_map_id == 0u || MapMgr::GetMapId() == plan.required_start_map_id);
}

bool RecordEntryFailureAndMaybeResetDialog(
    EntryFailureTracker& tracker,
    const DialogResetBouncePlan& plan,
    const char* context) {
    const char* prefix = PrefixOrDefault(plan.log_prefix);
    ++tracker.count;
    Log::Info("%s: quest-entry failure count=%d threshold=%d context=%s map=%u",
              prefix,
              tracker.count,
              tracker.reset_threshold,
              context ? context : "unspecified",
              MapMgr::GetMapId());

    if (tracker.count <= tracker.reset_threshold) {
        return false;
    }

    const bool bounced = BounceThroughDungeonToResetDialog(plan);
    ResetEntryFailureTracker(
        tracker,
        bounced ? "dialog-reset-bounce" : "dialog-reset-bounce-failed",
        prefix);
    return bounced;
}

BacktrackReturnResult ReplayBacktrackAndReturnToQuestMap(const BacktrackReturnOptions& options) {
    BacktrackReturnResult result;
    result.final_map_id = MapMgr::GetMapId();

    DungeonCheckpoint::CheckpointBacktrackReplayOptions replayOptions;
    replayOptions.waypoints = options.waypoints;
    replayOptions.waypoint_count = options.waypoint_count;
    replayOptions.current_index = options.current_index;
    replayOptions.backtrack_start = options.current_index - options.backtrack_steps;
    replayOptions.move_waypoint = options.move_waypoint;
    replayOptions.after_move_waypoint = options.after_move_waypoint;
    (void)DungeonCheckpoint::ReplayCheckpointBacktrack(replayOptions);

    result.returned_to_quest_map = ExecuteTransitionPlan(options.return_to_quest_map);
    result.final_map_id = MapMgr::GetMapId();
    Log::Info("%s: %s returned=%d finalMap=%u",
              PrefixOrDefault(options.log_prefix),
              LabelOrDefault(options.label, "checkpoint return"),
              result.returned_to_quest_map ? 1 : 0,
              result.final_map_id);
    return result;
}

QuestDoorRecoveryResult HandleQuestDoorRecovery(const QuestDoorRecoveryOptions& options) {
    QuestDoorRecoveryResult result;
    result.final_map_id = MapMgr::GetMapId();
    if (options.quest_id == 0u) {
        return result;
    }

    DungeonQuestRuntime::QuestReadyOptions readyOptions = options.quest_ready;
    if (readyOptions.log_prefix == nullptr) {
        readyOptions.log_prefix = options.log_prefix;
    }
    if (readyOptions.label == nullptr) {
        readyOptions.label = "quest-door checkpoint precheck";
    }
    result.quest_ready = DungeonQuestRuntime::RefreshQuestReadyForDungeonEntry(
        options.quest_id,
        readyOptions).ready;
    if (result.quest_ready) {
        return result;
    }

    const char* prefix = PrefixOrDefault(options.log_prefix);
    const char* label = LabelOrDefault(options.label, "Quest Door Checkpoint");
    Log::Info("%s: %s reached without quest; returning for quest refresh", prefix, label);
    result.recovery_triggered = true;

    BacktrackReturnOptions returnOptions;
    returnOptions.waypoints = options.waypoints;
    returnOptions.waypoint_count = options.waypoint_count;
    returnOptions.current_index = options.current_index;
    returnOptions.backtrack_steps = options.backtrack_steps;
    returnOptions.move_waypoint = options.move_waypoint;
    returnOptions.after_move_waypoint = options.after_move_waypoint;
    returnOptions.return_to_quest_map = options.return_to_quest_map;
    returnOptions.log_prefix = options.log_prefix;
    returnOptions.label = label;
    const auto returned = ReplayBacktrackAndReturnToQuestMap(returnOptions);
    result.returned_to_quest_map = returned.returned_to_quest_map;
    result.final_map_id = returned.final_map_id;
    return result;
}

} // namespace GWA3::DungeonEntryRecovery
