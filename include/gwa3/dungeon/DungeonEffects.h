#pragma once

#include <cstdint>

namespace GWA3::DungeonEffects {

using WaitFn = void(*)(uint32_t ms);
using MoveToPointResultFn = bool(*)(float x, float y, float threshold);
using PositionSettleFn = bool(*)(uint32_t timeoutMs, float maxDeltaPerSample);
using SignpostScanLogFn = void(*)(float x, float y, float maxDist, const char* label, bool chestOnly);
using AgentLogFn = void(*)(const char* label, uint32_t agentId);

struct ActiveTitleEnsureResult {
    bool already_active = false;
    bool applied = false;
    uint32_t previous_title_id = 0u;
    uint32_t final_title_id = 0u;
};

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

struct DungeonBlessingAcquireOptions {
    float primary_search_radius = 900.0f;
    float fallback_search_radius = 1500.0f;
    float signpost_move_threshold = 120.0f;
    float npc_move_threshold = 90.0f;
    uint32_t required_title_id = 0x27u;
    uint32_t title_settle_delay_ms = 1000u;
    uint32_t accept_dialog_id = 0x84u;
    uint32_t settle_timeout_ms = 1200u;
    float settle_distance = 15.0f;
    const char* log_prefix = "Dungeon blessing";
    MoveToPointResultFn move_to_point = nullptr;
    PositionSettleFn wait_for_position_settle = nullptr;
    WaitFn wait_ms = nullptr;
    SignpostScanLogFn signpost_scan_log = nullptr;
    AgentLogFn agent_log = nullptr;
};

uint32_t GetPlayerEffectCount();
bool HasAnyDungeonBlessing(uint32_t agentId = 0u);
bool HasBlessing();
bool HasFullConset(uint32_t agentId = 0u);
ActiveTitleEnsureResult EnsureActiveTitle(
    uint32_t titleId,
    uint32_t settleDelayMs = 1000u,
    WaitFn wait_ms = nullptr);
BlessingAcquireResult TryAcquireBlessingAt(
    float shrineX,
    float shrineY,
    const BlessingAcquireOptions& options = {});
BlessingAcquireResult AcquireDungeonBlessingAt(
    float shrineX,
    float shrineY,
    const DungeonBlessingAcquireOptions& options = {});

} // namespace GWA3::DungeonEffects
