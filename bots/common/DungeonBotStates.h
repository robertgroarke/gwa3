#pragma once

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonQuest.h>
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
using SimpleActionFn = std::function<bool()>;
using ContextActionFn = std::function<void(const char* context)>;
using NeedsMaintenanceFn = std::function<bool()>;

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

struct TravelStateOptions {
    uint32_t default_source_map_id = 0u;
    uint32_t entry_map_id = 0u;
    const DungeonQuest::TravelPoint* travel_path = nullptr;
    int travel_path_count = 0;
    DungeonQuest::TravelPoint zone_point = {};
    uint32_t zone_timeout_ms = 10000u;
    const char* log_prefix = "Dungeon";
    const char* label = "Travel to entry map";
};

struct DungeonLoopStateResult {
    bool completed = false;
    uint32_t final_map_id = 0u;
};

struct PostEntryMapRunDecision {
    BotState next_state = BotState::InDungeon;
    bool maintenance_deferred = false;
};

using RunDungeonLoopFn = std::function<DungeonLoopStateResult()>;
using MarkRunStartedFn = std::function<uint32_t()>;
using MarkRunCompletedFn = std::function<void(uint32_t runNumber, uint32_t finalMapId)>;
using MarkRunFailedFn = std::function<void(uint32_t mapId)>;
using ResolvePostEntryMapRunDecisionFn = std::function<PostEntryMapRunDecision(bool maintenanceNeeded)>;

struct DungeonProgressionOptions {
    uint32_t default_outpost_map_id = 0u;
    uint32_t entry_map_id = 0u;
    const uint32_t* dungeon_map_ids = nullptr;
    int dungeon_map_count = 0;

    const char* log_prefix = "Dungeon";
    const char* entry_map_name = "entry map";
    const char* dungeon_name = "dungeon";

    uint32_t retry_wait_ms = 1000u;

    RefreshSkillCacheFn refresh_skill_cache = {};
    UseConsumablesFn use_consumables = {};
    SimpleActionFn move_to_entry_npc = {};
    SimpleActionFn prepare_entry = {};
    SimpleActionFn enter_dungeon = {};
    ContextActionFn record_entry_failure = {};
    ContextActionFn reset_entry_failures = {};
    RunDungeonLoopFn run_dungeon_loop = {};
    NeedsMaintenanceFn needs_maintenance = {};
    ResolvePostEntryMapRunDecisionFn resolve_post_entry_map_run_decision = {};
    MarkRunStartedFn mark_run_started = {};
    MarkRunCompletedFn mark_run_completed = {};
    MarkRunFailedFn mark_run_failed = {};
};

BotState HandleCharSelect(BotConfig& cfg, const CharSelectOptions& options = {});
BotState HandleTownSetup(BotConfig& cfg, const TownSetupOptions& options);
BotState HandleTravelToEntryMap(BotConfig& cfg, const TravelStateOptions& options);
BotState HandleDungeonProgression(BotConfig& cfg, const DungeonProgressionOptions& options);

} // namespace GWA3::Bot::DungeonStates
