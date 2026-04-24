#pragma once

#include <gwa3/bot/DungeonQuest.h>

#include <cstdint>

namespace GWA3::Bot::DungeonQuestRuntime {

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

bool SendDialogPlan(
    const DungeonQuest::DialogPlan& plan,
    const DialogExecutionOptions& options = {});
bool InteractNearestNpcAndSendDialogPlan(
    const DungeonQuest::QuestNpcAnchor& npc,
    const DungeonQuest::DialogPlan& plan,
    const DialogExecutionOptions& options = {});
bool WaitForQuestState(
    uint32_t questId,
    bool expectPresent,
    const QuestVerificationOptions& options = {});
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

} // namespace GWA3::Bot::DungeonQuestRuntime
