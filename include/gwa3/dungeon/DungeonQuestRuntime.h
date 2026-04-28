#pragma once

#include <gwa3/dungeon/DungeonQuest.h>
#include <gwa3/dungeon/DungeonBundle.h>

#include <cstdint>

namespace GWA3::DungeonInteractions {
class OpenedChestTracker;
}

namespace GWA3::DungeonQuestRuntime {

using BoolFn = bool(*)();

struct DialogExecutionOptions {
    uint32_t change_target_delay_ms = 250u;
    uint32_t interact_delay_ms = 1000u;
    uint32_t post_interact_delay_ms = 1000u;
    uint32_t dialog_delay_ms = 500u;
    uint32_t repeat_delay_ms = 1000u;
    uint32_t pre_interact_settle_ms = 0u;
    uint32_t move_to_npc_timeout_ms = 15000u;
    uint32_t dialog_wait_timeout_ms = 0u;
    int interact_count = 1;
    int max_retries_per_dialog = 1;
    float move_to_npc_tolerance = 0.0f;
    bool use_direct_npc_interact = false;
    bool move_to_actual_npc = false;
    bool cancel_action_before_interact = false;
    bool clear_dialog_state_before_interact = false;
    bool require_dialog_before_send = false;
};

struct BootstrapExecutionOptions {
    float npc_tolerance = 250.0f;
    float path_tolerance = 250.0f;
    uint32_t move_timeout_ms = 20000u;
    uint32_t move_reissue_ms = 1000u;
    uint32_t zone_timeout_ms = 60000u;
    uint32_t zone_poll_ms = 250u;
};

struct QuestVerificationOptions {
    uint32_t refresh_delay_ms = 150u;
    uint32_t refresh_interval_ms = 1000u;
    uint32_t poll_ms = 100u;
    uint32_t timeout_ms = 2000u;
    uint32_t post_set_active_delay_ms = 150u;
    bool set_active_when_present = true;
    bool require_not_completed_when_present = false;
};

struct QuestDialogOptions {
    uint32_t post_dialog_wait_ms = 500u;
    uint32_t refresh_delay_ms = 150u;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct QuestDialogResult {
    bool sent = false;
    bool quest_present = false;
    uint32_t active_quest_id = 0u;
    uint32_t last_dialog_id = 0u;
};

struct DungeonEntryReadyOptions {
    uint32_t quest_id = 0u;
    uint32_t entry_dialog_id = 0u;
    uint32_t npc_id = 0u;
    uint32_t timeout_ms = 2000u;
    uint32_t refresh_interval_ms = 500u;
    uint32_t poll_ms = 100u;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct DungeonEntryReadyResult {
    bool ready = false;
    bool quest_present = false;
    bool active_quest = false;
    bool entry_dialog_latched = false;
    bool entry_button_visible = false;
    uint32_t sender_agent_id = 0u;
};

struct RewardClaimOptions {
    bool allow_dialog_without_npc = true;
    DialogExecutionOptions execution = {};
};

struct RewardClaimResult {
    bool npc_found = false;
    bool dialog_sent = false;
    uint32_t npc_id = 0u;
};

struct RewardNpcStageOptions {
    float move_threshold = 250.0f;
    uint32_t settle_timeout_ms = 1000u;
    float settle_distance = 15.0f;
    const char* log_prefix = nullptr;
    BoolFn is_dead = nullptr;
};

struct RewardNpcStageResult {
    bool reached = false;
    float player_x = 0.0f;
    float player_y = 0.0f;
    float distance_to_anchor = -1.0f;
    uint32_t map_id = 0u;
    bool map_loaded = false;
};

struct RewardNpcResolveOptions {
    float local_search_radius = 3500.0f;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct RewardNpcResolveResult {
    uint32_t npc_id = 0u;
    bool found_at_anchor = false;
    bool found_near_player = false;
};

struct BossRewardOptions {
    uint32_t current_map_id = 0u;
    float chest_x = 0.0f;
    float chest_y = 0.0f;
    DungeonBundle::ChestOpenOptions chest = {};
    RewardClaimOptions reward = {};
};

struct BossRewardResult {
    bool chest_opened = false;
    bool reward_dialog_sent = false;
    bool reward_npc_found = false;
};

bool SendDialogPlan(
    const DungeonQuest::DialogPlan& plan,
    const DialogExecutionOptions& options = {});
QuestDialogResult SendDialogAndRefreshQuest(
    uint32_t dialogId,
    uint32_t questId,
    const QuestDialogOptions& options = {});
DungeonEntryReadyResult WaitForDungeonEntryReady(
    const DungeonEntryReadyOptions& options);
bool InteractNearestNpcAndSendDialogPlan(
    const DungeonQuest::QuestNpcAnchor& npc,
    const DungeonQuest::DialogPlan& plan,
    const DialogExecutionOptions& options = {});
RewardClaimResult TryClaimReward(
    const DungeonQuest::QuestNpcAnchor& npc,
    const DungeonQuest::DialogPlan& plan,
    const RewardClaimOptions& options = {});
RewardNpcStageResult StageRewardNpcInteraction(
    const DungeonQuest::QuestNpcAnchor& rewardNpc,
    const RewardNpcStageOptions& options = {});
RewardNpcResolveResult ResolveRewardNpc(
    const DungeonQuest::QuestNpcAnchor& rewardNpc,
    const RewardNpcResolveOptions& options = {});
BossRewardResult ExecuteBossRewardSequence(
    DungeonInteractions::OpenedChestTracker& tracker,
    const DungeonQuest::QuestNpcAnchor& rewardNpc,
    const DungeonQuest::DialogPlan& rewardDialog,
    const BossRewardOptions& options = {});
bool WaitForQuestState(
    uint32_t questId,
    bool expectPresent,
    const QuestVerificationOptions& options = {});
uint32_t MakeQuestRewardDialogId(uint32_t questId);
bool AcceptQuestRewardWithRetry(uint32_t questId, uint32_t npcId = 0u, uint32_t timeoutMs = 1000u);
bool FollowTravelPath(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t expectedMapId,
    float tolerance = 250.0f,
    uint32_t moveTimeoutMs = 20000u,
    uint32_t moveReissueMs = 1000u);
bool ZoneThroughPoint(
    float x,
    float y,
    uint32_t targetMapId,
    uint32_t timeoutMs = 60000u,
    uint32_t pollMs = 250u);
bool ExecuteBootstrapPlan(
    const DungeonQuest::BootstrapPlan& plan,
    const DialogExecutionOptions& dialogOptions = {},
    const BootstrapExecutionOptions& bootstrapOptions = {});

} // namespace GWA3::DungeonQuestRuntime
