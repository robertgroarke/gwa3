#include <bots/common/DungeonBotStates.h>

#include <gwa3/dungeon/DungeonRuntime.h>
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

} // namespace GWA3::Bot::DungeonStates
