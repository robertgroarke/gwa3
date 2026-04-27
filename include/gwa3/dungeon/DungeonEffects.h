#pragma once

#include <cstdint>

namespace GWA3::DungeonEffects {

using WaitFn = void(*)(uint32_t ms);

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

} // namespace GWA3::DungeonEffects
