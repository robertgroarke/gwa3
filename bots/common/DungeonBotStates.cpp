#include <bots/common/DungeonBotStates.h>

#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/dungeon/DungeonRuntime.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/UIMgr.h>

namespace GWA3::Bot::DungeonStates {

namespace {

const char* PrefixOrDefault(const char* prefix) {
    return prefix != nullptr ? prefix : "Dungeon";
}

uint32_t ResolveOutpostMapId(const BotConfig& cfg, const TownSetupOptions& options) {
    if (cfg.outpost_map_id != 0u) {
        return cfg.outpost_map_id;
    }
    return options.default_outpost_map_id;
}

uint32_t ResolveOutpostMapId(const BotConfig& cfg, uint32_t defaultOutpostMapId) {
    if (cfg.outpost_map_id != 0u) {
        return cfg.outpost_map_id;
    }
    return defaultOutpostMapId;
}

bool IsMapInList(uint32_t mapId, const uint32_t* mapIds, int mapCount) {
    if (mapIds == nullptr || mapCount <= 0) {
        return false;
    }
    for (int i = 0; i < mapCount; ++i) {
        if (mapIds[i] == mapId) {
            return true;
        }
    }
    return false;
}

} // namespace

BotState HandleCharSelect(BotConfig& cfg, const CharSelectOptions& options) {
    (void)cfg;
    LogBot("State: CharSelect");

    if (UIMgr::IsFrameVisible(UIMgr::Hashes::PlayButton)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::PlayButton);
        DungeonRuntime::WaitMs(options.play_wait_ms);
    }

    if (UIMgr::IsFrameVisible(UIMgr::Hashes::ReconnectYes)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::ReconnectYes);
        DungeonRuntime::WaitMs(options.reconnect_wait_ms);
    }

    DungeonRuntime::WaitMs(options.map_load_wait_ms);
    if (MapMgr::GetMapId() > 0u) {
        return BotState::InTown;
    }

    return BotState::CharSelect;
}

BotState HandleTownSetup(BotConfig& cfg, const TownSetupOptions& options) {
    const char* prefix = PrefixOrDefault(options.log_prefix);
    if (options.run_number != 0u) {
        LogBot("State: TownSetup (run #%u)", options.run_number);
    } else {
        LogBot("State: TownSetup");
    }

    const uint32_t outpostMapId = ResolveOutpostMapId(cfg, options);
    if (outpostMapId == 0u) {
        LogBot("%s: town setup missing outpost map id", prefix);
        return BotState::Error;
    }

    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId != outpostMapId) {
        LogBot("%s: TownSetup traveling to outpost map %u from map %u", prefix, outpostMapId, mapId);
        if (!DungeonRuntime::EnsureOutpostReady(
                outpostMapId,
                options.outpost_ready_timeout_ms,
                options.outpost_context != nullptr ? options.outpost_context : "TownSetup")) {
            LogBot("%s: TownSetup outpost travel failed; stopping bot to avoid queue saturation", prefix);
            return BotState::Stopping;
        }
        return BotState::InTown;
    }

    if (!DungeonRuntime::WaitForTownRuntimeReady(outpostMapId, options.town_runtime_timeout_ms)) {
        LogBot("%s: TownSetup outpost runtime not ready after map load; waiting for next tick", prefix);
        DungeonRuntime::WaitMs(options.town_wait_ms);
        return BotState::InTown;
    }

    if (MaintenanceMgr::NeedsMaintenance(options.maintenance)) {
        LogBot("%s: Maintenance needed - running before dungeon entry", prefix);

        if (options.move_to_point) {
            (void)options.move_to_point(
                options.merchant_x,
                options.merchant_y,
                options.merchant_move_threshold);
        }

        if (options.open_merchant &&
            options.open_merchant(options.merchant_x, options.merchant_y, options.merchant_search_radius)) {
            MaintenanceMgr::PerformMaintenance(options.maintenance);
            DungeonRuntime::WaitMs(options.post_maintenance_wait_ms);
        } else {
            LogBot("%s: Maintenance merchant window failed to open, skipping sell/buy", prefix);
            MaintenanceMgr::DepositGold(10000u);
        }

        const uint32_t postMaintenanceFreeSlots = MaintenanceMgr::CountFreeSlots();
        const bool stillNeedsMaintenance = MaintenanceMgr::NeedsMaintenance(options.maintenance);
        if (postMaintenanceFreeSlots < options.critical_free_slots || stillNeedsMaintenance) {
            LogBot("%s: TownSetup maintenance did not clear inventory enough (freeSlots=%u preferred=%u critical=%u needsMaintenance=%s); stopping before explorable entry",
                   prefix,
                   postMaintenanceFreeSlots,
                   options.maintenance.minFreeSlots,
                   options.critical_free_slots,
                   stillNeedsMaintenance ? "yes" : "no");
            return BotState::Stopping;
        }
        if (postMaintenanceFreeSlots < options.maintenance.minFreeSlots) {
            LogBot("%s: TownSetup continuing after exhausted maintenance with %u free slots (preferred=%u)",
                   prefix,
                   postMaintenanceFreeSlots,
                   options.maintenance.minFreeSlots);
        }
    }

    if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg, options.outpost)) {
        LogBot("%s: outpost setup failed", prefix);
        return BotState::Error;
    }

    if (options.refresh_skill_cache) {
        options.refresh_skill_cache();
    }

    if (options.use_consumables) {
        options.use_consumables(cfg);
    }

    return BotState::Traveling;
}

BotState HandleTravelToEntryMap(BotConfig& cfg, const TravelStateOptions& options) {
    const uint32_t sourceMapId = ResolveOutpostMapId(cfg, options.default_source_map_id);
    if (sourceMapId == 0u || options.entry_map_id == 0u) {
        LogBot("%s: travel state missing source or entry map id",
               PrefixOrDefault(options.log_prefix));
        return BotState::Error;
    }

    DungeonQuestRuntime::TravelToEntryMapOptions travelOptions = {};
    travelOptions.source_map_id = sourceMapId;
    travelOptions.entry_map_id = options.entry_map_id;
    travelOptions.travel_path = options.travel_path;
    travelOptions.travel_path_count = options.travel_path_count;
    travelOptions.zone_point = options.zone_point;
    travelOptions.zone_timeout_ms = options.zone_timeout_ms;
    travelOptions.log_prefix = PrefixOrDefault(options.log_prefix);
    travelOptions.label = options.label != nullptr ? options.label : "Travel to entry map";

    const auto result = DungeonQuestRuntime::TravelToEntryMap(travelOptions);
    if (result == DungeonQuestRuntime::TravelToEntryMapStatus::AtEntryMap) {
        return BotState::InDungeon;
    }
    return BotState::InTown;
}

BotState HandleDungeonProgression(BotConfig& cfg, const DungeonProgressionOptions& options) {
    const char* prefix = PrefixOrDefault(options.log_prefix);
    uint32_t mapId = MapMgr::GetMapId();

    if (options.refresh_skill_cache) {
        options.refresh_skill_cache();
    }

    if (mapId == options.entry_map_id) {
        LogBot("State: %s - preparing %s entry",
               options.entry_map_name != nullptr ? options.entry_map_name : "entry map",
               options.dungeon_name != nullptr ? options.dungeon_name : "dungeon");

        if (options.use_consumables) {
            options.use_consumables(cfg);
        }

        if (options.move_to_entry_npc && !options.move_to_entry_npc()) {
            LogBot("%s entry NPC approach failed; staying in entry map for retry",
                   prefix);
            Log::Warn("%s: skipping ACTION_CANCEL after entry approach failure; retry will issue fresh movement",
                      prefix);
            DungeonRuntime::WaitMs(options.retry_wait_ms);
            return BotState::InDungeon;
        }

        if (options.prepare_entry && !options.prepare_entry()) {
            LogBot("%s entry preparation failed; staying in entry map for retry",
                   prefix);
            if (options.record_entry_failure) {
                options.record_entry_failure("entry-state");
            }
            Log::Warn("%s: skipping ACTION_CANCEL after entry prep failure; retry will refresh dialog/movement",
                      prefix);
            DungeonRuntime::WaitMs(options.retry_wait_ms);
            return BotState::InDungeon;
        }

        if (options.reset_entry_failures) {
            options.reset_entry_failures("entry-prepared");
        }

        if (options.enter_dungeon && !options.enter_dungeon()) {
            LogBot("%s dungeon portal transition failed; staying in entry map for retry",
                   prefix);
            Log::Warn("%s: skipping ACTION_CANCEL after portal transition failure; retry will issue fresh movement",
                      prefix);
            DungeonRuntime::WaitMs(options.retry_wait_ms);
            return BotState::InDungeon;
        }
    }

    mapId = MapMgr::GetMapId();

    if (IsMapInList(mapId, options.dungeon_map_ids, options.dungeon_map_count)) {
        uint32_t runNumber = 0u;
        if (options.mark_run_started) {
            runNumber = options.mark_run_started();
        }

        DungeonLoopStateResult loopResult = {};
        if (options.run_dungeon_loop) {
            loopResult = options.run_dungeon_loop();
        }
        if (loopResult.final_map_id == 0u) {
            loopResult.final_map_id = MapMgr::GetMapId();
        }

        if (loopResult.completed) {
            if (options.mark_run_completed) {
                options.mark_run_completed(runNumber, loopResult.final_map_id);
            }

            if (loopResult.final_map_id == options.entry_map_id) {
                const bool maintenanceNeeded = options.needs_maintenance && options.needs_maintenance();
                PostEntryMapRunDecision decision = {};
                if (options.resolve_post_entry_map_run_decision) {
                    decision = options.resolve_post_entry_map_run_decision(maintenanceNeeded);
                }
                if (decision.maintenance_deferred) {
                    LogBot("Maintenance needed after run; deferring town maintenance and preserving entry-map loop");
                }
                LogBot("Run returned to entry map; re-entering dungeon without town reset");
                return decision.next_state;
            }

            if (loopResult.final_map_id == ResolveOutpostMapId(cfg, options.default_outpost_map_id)) {
                return BotState::InTown;
            }
        }

        if (loopResult.final_map_id == options.entry_map_id) {
            LogBot("%s loop returned to entry map before completion; retrying entry",
                   options.dungeon_name != nullptr ? options.dungeon_name : "Dungeon");
            return BotState::InDungeon;
        }

        LogBot("%s loop did not complete cleanly (finalMap=%u); retrying dungeon state",
               options.dungeon_name != nullptr ? options.dungeon_name : "Dungeon",
               loopResult.final_map_id);
        DungeonRuntime::WaitMs(options.retry_wait_ms);
        return BotState::InDungeon;
    }

    const uint32_t outpostMapId = ResolveOutpostMapId(cfg, options.default_outpost_map_id);
    if (outpostMapId != 0u && mapId == outpostMapId) {
        if (options.mark_run_failed) {
            options.mark_run_failed(mapId);
        }
        LogBot("Run failed (returned to outpost), restarting");
        return BotState::InTown;
    }

    return BotState::InDungeon;
}

} // namespace GWA3::Bot::DungeonStates
