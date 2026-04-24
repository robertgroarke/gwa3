#include <froggy_shared/FroggySharedAdapters.h>

#include <gwa3/bot/DungeonBundle.h>
#include <gwa3/bot/DungeonDialog.h>
#include <gwa3/bot/DungeonQuestRuntime.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/PlayerMgr.h>

#include <Windows.h>

namespace GWA3::Bot::FroggySharedStaging {

namespace {

constexpr uint32_t MAP_SPARKFLY_SWAMP = 558u;
constexpr uint32_t MAP_BOGROOT_LVL1 = 615u;
constexpr uint32_t QUEST_TEKKS_WAR = 0x339u;
constexpr uint32_t DIALOG_QUEST_REWARD = 0x833907u;
constexpr uint32_t DIALOG_QUEST_ACCEPT = 0x833901u;
constexpr uint32_t DIALOG_NPC_TALK = 0x2AE6u;
constexpr uint32_t SKILL_DWARVEN_BLESSING = 2049u;
constexpr uint32_t SKILL_ASURAN_BLESSING = 2050u;
constexpr uint32_t SKILL_NORN_BLESSING = 2051u;
constexpr uint32_t SKILL_VANGUARD_BLESSING = 2052u;
constexpr uint32_t SKILL_VET_ASURAN_BODYGUARD = 2548u;
constexpr uint32_t SKILL_VET_DWARVEN_RAIDER = 2549u;
constexpr uint32_t SKILL_VET_VANGUARD_PATROL = 2550u;
constexpr uint32_t SKILL_VET_NORN_HUNTING_PARTY = 2551u;

constexpr uint32_t kEntryDialogs[] = {
    DIALOG_NPC_TALK,
    DIALOG_QUEST_ACCEPT,
};

constexpr uint32_t kRewardDialogs[] = {
    DIALOG_QUEST_REWARD,
};

constexpr DungeonQuest::TravelPoint kEntryPath[] = {
    {12228.0f, 22677.0f},
    {12470.0f, 25036.0f},
    {12968.0f, 26219.0f},
};

bool HasAnyBlessingSkill(uint32_t agentId) {
    return EffectMgr::HasEffect(agentId, SKILL_DWARVEN_BLESSING) ||
           EffectMgr::HasEffect(agentId, SKILL_ASURAN_BLESSING) ||
           EffectMgr::HasEffect(agentId, SKILL_NORN_BLESSING) ||
           EffectMgr::HasEffect(agentId, SKILL_VANGUARD_BLESSING) ||
           EffectMgr::HasEffect(agentId, SKILL_VET_ASURAN_BODYGUARD) ||
           EffectMgr::HasEffect(agentId, SKILL_VET_DWARVEN_RAIDER) ||
           EffectMgr::HasEffect(agentId, SKILL_VET_VANGUARD_PATROL) ||
           EffectMgr::HasEffect(agentId, SKILL_VET_NORN_HUNTING_PARTY);
}

const DungeonCheckpoint::CheckpointRetryPolicy* GetFroggyCheckpointPolicy(
    DungeonRoute::WaypointLabelKind kind,
    DungeonCheckpoint::CheckpointRetryPolicy& storage,
    int currentIndex) {
    switch (kind) {
    case DungeonRoute::WaypointLabelKind::DungeonDoorCheckpoint:
        storage = {
            "Dungeon Door Checkpoint",
            DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry,
            3,
            currentIndex,
        };
        return &storage;
    case DungeonRoute::WaypointLabelKind::QuestDoorCheckpoint:
        storage = {
            "Quest Door Checkpoint",
            DungeonCheckpoint::CheckpointFailureAction::AbortRun,
            0,
            0,
        };
        return &storage;
    default:
        return nullptr;
    }
}

} // namespace

int GetNearestWaypointIndex(const DungeonRoute::Waypoint* waypoints, int count, float x, float y) {
    return DungeonRoute::FindNearestWaypointIndex(waypoints, count, x, y);
}

bool SendDialogWithRetry(uint32_t dialogId, int maxRetries, uint32_t delayMs) {
    return DungeonDialog::SendDialogWithRetry(dialogId, maxRetries, delayMs);
}

bool IsChestGadgetId(uint32_t gadgetId) {
    return DungeonInteractions::IsChestGadgetId(gadgetId);
}

uint32_t FindNearestSignpost(float x, float y, float maxDist) {
    return DungeonInteractions::FindNearestSignpost(x, y, maxDist);
}

uint32_t GetQuestId() {
    return QUEST_TEKKS_WAR;
}

uint32_t GetRewardDialogId() {
    return DIALOG_QUEST_REWARD;
}

uint32_t GetAcceptDialogId() {
    return DIALOG_QUEST_ACCEPT;
}

uint32_t GetNpcTalkDialogId() {
    return DIALOG_NPC_TALK;
}

uint32_t GetBlessingTitleId() {
    return 0x27u;
}

uint32_t GetBlessingAcceptDialogId() {
    return 0x84u;
}

DungeonQuest::BootstrapPlan GetEntryBootstrapPlan() {
    DungeonQuest::BootstrapPlan plan;
    plan.npc = {12396.0f, 22407.0f, 1500.0f};
    plan.dialog_ids = kEntryDialogs;
    plan.dialog_count = static_cast<int>(sizeof(kEntryDialogs) / sizeof(kEntryDialogs[0]));
    plan.dialog_repeats = 2;
    plan.entry_path = kEntryPath;
    plan.entry_path_count = static_cast<int>(sizeof(kEntryPath) / sizeof(kEntryPath[0]));
    plan.zone_point = {13097.0f, 26393.0f};
    plan.entry_map_id = MAP_SPARKFLY_SWAMP;
    plan.target_map_id = MAP_BOGROOT_LVL1;
    return plan;
}

DungeonQuest::QuestNpcAnchor GetRewardNpcAnchor() {
    return {14618.0f, -17828.0f, 1500.0f};
}

DungeonQuest::DialogPlan GetRewardDialogPlan() {
    return {
        kRewardDialogs,
        static_cast<int>(sizeof(kRewardDialogs) / sizeof(kRewardDialogs[0])),
        3,
    };
}

bool HasBlessing() {
    const uint32_t myId = AgentMgr::GetMyId();
    if (myId == 0u) {
        return false;
    }
    return HasAnyBlessingSkill(myId);
}

BlessingAcquireResult TryAcquireBlessingAt(
    float shrineX,
    float shrineY,
    const BlessingAcquireOptions& options) {
    BlessingAcquireResult result;
    result.final_title_id = PlayerMgr::GetActiveTitleId();

    if (HasBlessing()) {
        result.already_active = true;
        result.confirmed = true;
        return result;
    }

    if (result.final_title_id == 0u && options.required_title_id != 0u) {
        result.title_applied = PlayerMgr::SetActiveTitle(options.required_title_id);
        if (result.title_applied) {
            Sleep(options.title_settle_delay_ms);
        }
        result.final_title_id = PlayerMgr::GetActiveTitleId();
    }

    result.npc_id = DungeonInteractions::FindNearestNpc(shrineX, shrineY, options.npc_search_radius);
    result.npc_found = result.npc_id != 0u;
    if (!result.npc_found) {
        return result;
    }

    if (options.toggle_dialog_hooks) {
        DialogMgr::Shutdown();
        result.dialog_hooks_toggled = true;
    }

    for (int attempt = 0; attempt < options.interact_count; ++attempt) {
        AgentMgr::InteractNPC(result.npc_id);
        result.interacted = true;
        Sleep(options.interact_delay_ms);
    }

    result.dialog_sent = DungeonDialog::SendDialogWithRetry(
        options.accept_dialog_id,
        options.dialog_retries,
        options.dialog_delay_ms);

    if (options.toggle_dialog_hooks) {
        DialogMgr::Initialize();
    }

    result.confirmed = HasBlessing();
    result.final_title_id = PlayerMgr::GetActiveTitleId();
    return result;
}

RewardClaimResult TryClaimReward(const RewardClaimOptions& options) {
    RewardClaimResult result;
    const auto npc = GetRewardNpcAnchor();
    const auto plan = GetRewardDialogPlan();

    result.npc_id = DungeonInteractions::FindNearestNpc(npc.x, npc.y, npc.search_radius);
    result.npc_found = result.npc_id != 0u;
    if (result.npc_found) {
        result.dialog_sent = DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(
            npc,
            plan,
            options.execution);
        return result;
    }

    if (options.allow_dialog_without_npc) {
        result.dialog_sent = DungeonQuestRuntime::SendDialogPlan(plan, options.execution);
    }
    return result;
}

BossRewardResult ExecuteBossRewardSequence(
    DungeonInteractions::OpenedChestTracker& tracker,
    const BossRewardOptions& options) {
    BossRewardResult result;
    result.chest_opened = TryOpenChestAt(
        options.chest_x,
        options.chest_y,
        options.current_map_id,
        tracker,
        options.chest);

    const RewardClaimResult reward = TryClaimReward(options.reward);
    result.reward_dialog_sent = reward.dialog_sent;
    result.reward_npc_found = reward.npc_found;
    return result;
}

WaypointBehavior ResolveWaypointBehavior(const char* label) {
    switch (DungeonRoute::ClassifyWaypointLabel(label)) {
    case DungeonRoute::WaypointLabelKind::Blessing:
        return WaypointBehavior::GrabBlessing;
    case DungeonRoute::WaypointLabelKind::DungeonDoor:
        return WaypointBehavior::OpenDungeonDoor;
    case DungeonRoute::WaypointLabelKind::DungeonDoorCheckpoint:
        return WaypointBehavior::ValidateDungeonDoorCheckpoint;
    case DungeonRoute::WaypointLabelKind::QuestDoorCheckpoint:
        return WaypointBehavior::ValidateQuestDoorCheckpoint;
    case DungeonRoute::WaypointLabelKind::Boss:
        return WaypointBehavior::BossRewardSequence;
    default:
        return WaypointBehavior::StandardMove;
    }
}

WaypointExecutionPlan BuildWaypointExecutionPlan(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int waypointIndex) {
    WaypointExecutionPlan plan;
    if (waypoints == nullptr || waypointIndex < 0 || waypointIndex >= count) {
        return plan;
    }

    plan.waypoint = &waypoints[waypointIndex];
    plan.behavior = ResolveWaypointBehavior(plan.waypoint->label);
    return plan;
}

int ComputeWipeRestartWaypoint(int nearestIndex) {
    return DungeonRoute::ComputeGenericRestartIndex(nearestIndex, 2);
}

int ComputeStuckBacktrackWaypoint(int nearestIndex) {
    return DungeonRoute::ComputeStuckBacktrackIndex(nearestIndex, 1);
}

bool ShouldBacktrackForStuck(int unchangedNearestCount) {
    return DungeonRoute::ShouldTriggerStuckBacktrack(unchangedNearestCount, 5);
}

CheckpointDecision EvaluateCheckpointProgress(
    const char* label,
    int currentIndex,
    int nearestIndex,
    int waypointCount) {
    CheckpointDecision decision;
    const auto kind = DungeonRoute::ClassifyWaypointLabel(label);
    DungeonCheckpoint::CheckpointRetryPolicy policyStorage = {};
    const auto* policy = GetFroggyCheckpointPolicy(kind, policyStorage, currentIndex);
    if (policy == nullptr) {
        return decision;
    }

    const auto resolution = DungeonCheckpoint::EvaluateAdvanceCheckpointResolution(
        currentIndex,
        nearestIndex,
        waypointCount,
        policy);
    decision.passed = resolution.passed;
    decision.action = resolution.action;
    decision.backtrack_start = resolution.backtrack_start;
    decision.retry_index = resolution.retry_index;
    decision.retry_from_nearest_after_backtrack =
        !resolution.passed && resolution.action == DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry;
    return decision;
}

WaypointProgressState MakeWaypointProgressState(int start_index) {
    WaypointProgressState state;
    state.last_nearest_index = start_index;
    state.unchanged_nearest_count = 0;
    return state;
}

WaypointProgressDecision EvaluateWaypointProgress(
    int current_nearest_index,
    WaypointProgressState& state) {
    WaypointProgressDecision decision;
    if (current_nearest_index == state.last_nearest_index) {
        ++state.unchanged_nearest_count;
        if (ShouldBacktrackForStuck(state.unchanged_nearest_count)) {
            decision.backtrack = true;
            decision.backtrack_index = ComputeStuckBacktrackWaypoint(current_nearest_index);
            state.unchanged_nearest_count = 0;
        }
        return decision;
    }

    state.last_nearest_index = current_nearest_index;
    state.unchanged_nearest_count = 0;
    return decision;
}

int ResolveCheckpointRetryIndex(
    const CheckpointDecision& decision,
    int nearest_index) {
    if (decision.retry_from_nearest_after_backtrack) {
        return nearest_index > 0 ? nearest_index - 1 : 0;
    }
    return decision.retry_index > 0 ? decision.retry_index : 0;
}

bool TryOpenChestAt(
    float x,
    float y,
    uint32_t currentMapId,
    DungeonInteractions::OpenedChestTracker& tracker,
    const ChestOpenOptions& options) {
    tracker.ResetForMap(currentMapId);

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(
        x,
        y,
        options.signpost_search_radius);
    if (signpostId == 0u || tracker.IsOpened(signpostId)) {
        return false;
    }

    tracker.MarkOpened(signpostId);
    return DungeonBundle::OpenChestAndPickUpBundle(
        x,
        y,
        options.signpost_search_radius,
        options.item_search_radius,
        options.interact_count,
        options.pickup_attempts,
        options.interact_delay_ms,
        options.pickup_delay_ms);
}

bool TryOpenDoorAt(float x, float y, const DoorOpenOptions& options) {
    return DungeonBundle::InteractSignpostNearPoint(
        x,
        y,
        options.signpost_search_radius,
        options.interact_count,
        options.interact_delay_ms);
}

bool ExecuteDoorOpenSequence(
    float x,
    float y,
    MoveToPointFn move_to_point,
    const DoorOpenOptions& options,
    float settle_threshold,
    uint32_t settle_delay_ms) {
    if (!TryOpenDoorAt(x, y, options)) {
        return false;
    }

    if (move_to_point != nullptr) {
        move_to_point(x, y, settle_threshold);
    }
    Sleep(settle_delay_ms);
    return true;
}

void ExecuteWaypointTravel(
    const DungeonRoute::Waypoint& waypoint,
    MoveWaypointFn move_to_point,
    MoveWaypointFn aggro_move_to_point,
    IsMapLoadedFn is_map_loaded,
    float move_threshold) {
    if (move_to_point == nullptr || aggro_move_to_point == nullptr || is_map_loaded == nullptr) {
        return;
    }

    if (waypoint.fight_range > 0.0f && is_map_loaded()) {
        aggro_move_to_point(waypoint.x, waypoint.y, waypoint.fight_range);
        return;
    }

    move_to_point(waypoint.x, waypoint.y, move_threshold);
}

void ExecuteReverseWaypointTraversal(
    const DungeonRoute::Waypoint* waypoints,
    int start_index,
    int end_index,
    MoveWaypointFn move_to_point,
    MoveWaypointFn aggro_move_to_point,
    IsMapLoadedFn is_map_loaded,
    float move_threshold) {
    if (waypoints == nullptr || start_index < end_index) {
        return;
    }

    for (int index = start_index; index >= end_index; --index) {
        ExecuteWaypointTravel(
            waypoints[index],
            move_to_point,
            aggro_move_to_point,
            is_map_loaded,
            move_threshold);
    }
}

bool ExecuteMapTransitionMove(
    float x,
    float y,
    uint32_t target_map_id,
    MoveToPointFn move_to_point,
    QueueMoveFn queue_move,
    GetMapIdFn get_map_id,
    WaitMsFn wait_ms,
    uint32_t timeout_ms,
    uint32_t pulse_delay_ms,
    float settle_threshold) {
    if (move_to_point == nullptr || queue_move == nullptr || get_map_id == nullptr || wait_ms == nullptr) {
        return false;
    }

    move_to_point(x, y, settle_threshold);
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeout_ms) {
        queue_move(x, y);
        wait_ms(pulse_delay_ms);
        if (get_map_id() == target_map_id) {
            return true;
        }
    }

    return get_map_id() == target_map_id;
}

} // namespace GWA3::Bot::FroggySharedStaging
