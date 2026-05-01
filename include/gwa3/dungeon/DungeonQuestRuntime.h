#pragma once

#include <gwa3/dungeon/DungeonQuest.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/dungeon/DungeonRuntime.h>

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

enum class TravelToEntryMapStatus : uint8_t {
    AtEntryMap,
    ReturnedToSourceMap,
    Failed,
};

struct TravelToEntryMapOptions {
    uint32_t source_map_id = 0u;
    uint32_t entry_map_id = 0u;
    const DungeonQuest::TravelPoint* travel_path = nullptr;
    int travel_path_count = 0;
    DungeonQuest::TravelPoint zone_point = {};
    float path_tolerance = 250.0f;
    uint32_t move_timeout_ms = 20000u;
    uint32_t move_reissue_ms = 1000u;
    uint32_t zone_timeout_ms = 10000u;
    uint32_t zone_poll_ms = 250u;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
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

struct QuestReadyOptions {
    uint32_t refresh_delay_ms = 150u;
    uint32_t post_set_active_delay_ms = 150u;
    bool set_active_when_present = true;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct QuestReadyResult {
    bool ready = false;
    bool quest_present = false;
    uint32_t active_quest_id = 0u;
    uint32_t quest_log_size = 0u;
};

struct QuestGiverDialogIds {
    uint32_t talk = 0u;
    uint32_t accept = 0u;
    uint32_t reward = 0u;
    uint32_t dungeon_entry = 0u;
};

struct QuestGiverDialogSnapshot {
    uint32_t ping = 0u;
    bool quest_present = false;
    DungeonDialog::DialogSnapshot dialog = {};
    bool has_dungeon_entry = false;
    bool has_talk = false;
    bool has_accept = false;
    bool has_reward = false;
};

struct QuestGiverEntryPlan {
    DungeonQuest::QuestNpcAnchor npc = {};
    uint32_t quest_id = 0u;
    QuestGiverDialogIds dialogs = {};
};

struct QuestGiverEntryOptions {
    float anchor_move_threshold = 500.0f;
    uint32_t pre_interact_dwell_ms = 500u;
    uint32_t cancel_dwell_ms = 500u;
    float npc_move_threshold = 100.0f;
    uint32_t npc_settle_timeout_ms = 1000u;
    float npc_settle_distance = 15.0f;
    uint32_t initial_interact_target_wait_ms = 500u;
    uint32_t initial_interact_pass_wait_ms = 2000u;
    int initial_interact_passes = 3;
    uint32_t post_interact_dwell_ms = 2000u;
    uint32_t direct_entry_wait_base_ms = 1000u;
    uint32_t reward_first_wait_base_ms = 500u;
    uint32_t dialog_refresh_delay_ms = 150u;
    uint32_t post_reward_wait_base_ms = 500u;
    uint32_t post_reward_max_buttons_per_pass = 4u;
    int post_reward_max_passes = 4;
    uint32_t reopen_accept_target_wait_base_ms = 150u;
    uint32_t reopen_accept_pass_wait_ms = 1500u;
    int reopen_accept_interact_passes = 1;
    int reopen_accept_attempts = 3;
    uint32_t accept_wait_base_ms = 500u;
    QuestVerificationOptions accept_verify = {};
    uint32_t talk_wait_base_ms = 500u;
    uint32_t entry_dialog_wait_base_ms = 1000u;
    uint32_t entry_verify_wait_base_ms = 2000u;
    uint32_t entry_verify_refresh_interval_ms = 500u;
    uint32_t entry_verify_poll_ms = 100u;
    uint32_t post_set_active_delay_ms = 150u;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct QuestGiverEntryResult {
    bool confirmed = false;
    bool npc_found = false;
    bool quest_present = false;
    bool entry_ready = false;
    uint32_t npc_id = 0u;
    uint32_t active_quest_id = 0u;
    uint32_t last_dialog_id = 0u;
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

struct BossRewardClaimOptions {
    uint32_t quest_id = 0u;
    uint32_t reward_dialog_id = 0u;
    float npc_move_threshold = 120.0f;
    uint32_t interact_target_wait_ms = 500u;
    uint32_t interact_pass_wait_ms = 1500u;
    int interact_passes = 3;
    uint32_t dialog_dwell_ms = 1000u;
    uint32_t target_settle_ms = 150u;
    int advance_max_passes = 4;
    uint32_t accept_retry_timeout_ms = 5000u;
    uint32_t retry_post_dialog_wait_ms = 1000u;
    uint32_t retry_refresh_delay_ms = 500u;
    int fallback_send_attempts = 1;
    uint32_t fallback_send_delay_ms = 1000u;
    uint32_t fallback_refresh_delay_ms = 500u;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct BossRewardClaimResult {
    bool npc_found = false;
    bool used_fallback = false;
    bool reward_button_ready = false;
    bool reward_cleared = false;
    bool retry_sent = false;
    bool final_quest_present = false;
    uint32_t npc_id = 0u;
    uint32_t last_dialog_id = 0u;
};

struct BossCompletionOptions {
    DungeonLoot::CombatMoveToFn aggro_move_to = nullptr;
    DungeonLoot::PickupNearbyLootFn pickup_nearby_loot = nullptr;
    DungeonLoot::MoveToPointResultFn move_to_point = nullptr;
    DungeonLoot::OpenChestAtFn open_chest_at = nullptr;
    DungeonLoot::WaitFn wait_ms = nullptr;
    DungeonRuntime::SalvageRewardItemsFn salvage_reward_items = nullptr;
    float post_fight_loot_radius = 1500.0f;
    uint32_t post_fight_loot_delay_ms = 3000u;
    float chest_x = 0.0f;
    float chest_y = 0.0f;
    float chest_open_radius = 5000.0f;
    float chest_loot_radius = 5000.0f;
    DungeonLoot::BossChestLootOptions chest = {};
    DungeonQuest::QuestNpcAnchor reward_npc = {};
    RewardNpcStageOptions reward_stage = {};
    RewardNpcResolveOptions reward_resolve = {};
    BossRewardClaimOptions reward_claim = {};
    DungeonRuntime::PostRewardReturnOptions post_reward = {};
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct BossCompletionResult {
    DungeonLoot::BossChestLootResult chest = {};
    RewardNpcStageResult reward_stage = {};
    RewardNpcResolveResult reward_resolve = {};
    BossRewardClaimResult reward_claim = {};
    DungeonRuntime::PostRewardReturnResult post_reward = {};
    bool reward_attempted = false;
    bool reward_claimed = false;
    bool reward_dialog_latched = false;
    bool boss_completed = false;
    uint32_t last_dialog_id = 0u;
    uint32_t final_map_id = 0u;
};

using BossWaypointStartedFn = void(*)(void* userData);
using BossWaypointResultFn = void(*)(void* userData, const BossCompletionResult& result);

struct BossWaypointOptions {
    BossCompletionOptions completion = {};
    BossWaypointStartedFn on_started = nullptr;
    BossWaypointResultFn on_result = nullptr;
    void* user_data = nullptr;
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
QuestReadyResult RefreshQuestReadyForDungeonEntry(
    uint32_t questId,
    const QuestReadyOptions& options = {});
QuestGiverEntryResult PrepareDungeonEntryFromQuestGiver(
    const QuestGiverEntryPlan& plan,
    const QuestGiverEntryOptions& options = {});
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
BossRewardClaimResult ClaimBossReward(uint32_t npcId, const BossRewardClaimOptions& options);
BossCompletionResult ExecuteBossCompletion(
    float bossX,
    float bossY,
    float fightRange,
    const BossCompletionOptions& options = {});
BossCompletionResult ExecuteBossWaypoint(
    float bossX,
    float bossY,
    float fightRange,
    const BossWaypointOptions& options = {});
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
TravelToEntryMapStatus TravelToEntryMap(const TravelToEntryMapOptions& options);
bool ExecuteBootstrapPlan(
    const DungeonQuest::BootstrapPlan& plan,
    const DialogExecutionOptions& dialogOptions = {},
    const BootstrapExecutionOptions& bootstrapOptions = {});

} // namespace GWA3::DungeonQuestRuntime
