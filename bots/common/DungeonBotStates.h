#pragma once

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/managers/MaintenanceMgr.h>

#include <cstdint>
#include <functional>

namespace GWA3::Bot::DungeonStates {

struct CharSelectOptions {
    uint32_t play_wait_ms = 5000u;
    uint32_t reconnect_wait_ms = 3000u;
    uint32_t map_load_wait_ms = 5000u;
};

using MoveToPointFn = std::function<bool(float x, float y, float threshold)>;
using OpenMerchantFn = std::function<bool(float x, float y, float searchRadius)>;
using RefreshSkillCacheFn = std::function<void()>;
using UseConsumablesFn = std::function<void(const BotConfig& cfg)>;

struct TownSetupOptions {
    uint32_t default_outpost_map_id = 0u;
    uint32_t run_number = 0u;
    const char* log_prefix = "Dungeon";
    const char* outpost_context = "TownSetup";
    uint32_t outpost_ready_timeout_ms = 60000u;
    uint32_t town_runtime_timeout_ms = 10000u;
    uint32_t town_wait_ms = 500u;
    uint32_t post_maintenance_wait_ms = 500u;
    uint32_t critical_free_slots = 3u;

    MaintenanceMgr::Config maintenance = {};
    float merchant_x = 0.0f;
    float merchant_y = 0.0f;
    float merchant_move_threshold = 350.0f;
    float merchant_search_radius = 2500.0f;

    DungeonOutpostSetup::Options outpost = {};
    MoveToPointFn move_to_point = {};
    OpenMerchantFn open_merchant = {};
    RefreshSkillCacheFn refresh_skill_cache = {};
    UseConsumablesFn use_consumables = {};
};

BotState HandleCharSelect(BotConfig& cfg, const CharSelectOptions& options = {});
BotState HandleTownSetup(BotConfig& cfg, const TownSetupOptions& options);

} // namespace GWA3::Bot::DungeonStates
