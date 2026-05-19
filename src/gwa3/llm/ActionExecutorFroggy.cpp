#include "ActionExecutorInternal.h"

#include <gwa3/advanced/ItemActions.h>
#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonRuntime.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/MerchantMgr.h>
#include <bots/common/BotFramework.h>
#include <bots/froggy/FroggyHM.h>

#include <Windows.h>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {
    static bool EnsureFroggyConsetsReadyForRun(const char* context) {
        const Bot::BotConfig& cfg = Bot::GetConfig();
        if (!cfg.use_consets) {
            return true;
        }

        const auto result = AdvancedItemActions::UseConsetsForCurrentPlayerIfEnabled(
            true,
            &DungeonRuntime::WaitMs,
            {},
            "Froggy");
        if (result.attempted && result.consets.full_active) {
            return true;
        }

        Log::Warn("[LLM-Action] %s: full conset not active; inventory consets=%u/%u/%u stored=%u/%u/%u",
                  context ? context : "froggy",
                  MaintenanceMgr::CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT),
                  MaintenanceMgr::CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY),
                  MaintenanceMgr::CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION),
                  MaintenanceMgr::CountItemByModelInStorage(ItemModelIds::GRAIL_OF_MIGHT),
                  MaintenanceMgr::CountItemByModelInStorage(ItemModelIds::ESSENCE_OF_CELERITY),
                  MaintenanceMgr::CountItemByModelInStorage(ItemModelIds::ARMOR_OF_SALVATION));
        return false;
    }

    static uint32_t CountFroggySalvageKitFamily() {
        return MaintenanceMgr::CountItemByModel(ItemModelIds::SALVAGE_KIT) +
               MaintenanceMgr::CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT) +
               MaintenanceMgr::CountItemByModel(ItemModelIds::RARE_SALVAGE_KIT) +
               MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT) +
               MaintenanceMgr::CountItemByModel(ItemModelIds::ALT_SALVAGE_KIT);
    }

    static bool HasReadableBackpackInventory() {
        Inventory* inv = ItemMgr::GetInventory();
        if (!inv) return false;

        for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
            Bag* bag = inv->bags[bagIdx];
            if (!bag) continue;
            if (bag->items_count == 0 || bag->items_count > 64) continue;
            if (!bag->items.buffer) continue;
            return true;
        }
        return false;
    }

    static bool WaitForReadableBackpackInventory(uint32_t timeoutMs, const char* label) {
        const DWORD start = GetTickCount();
        while ((GetTickCount() - start) < timeoutMs) {
            if (MapMgr::GetMapId() != 0 && AgentMgr::GetMyId() != 0 && HasReadableBackpackInventory()) {
                return true;
            }
            Sleep(250);
        }
        Log::Warn("[LLM-Action] %s inventory not readable after %u ms",
                  label ? label : "maintenance",
                  timeoutMs);
        return false;
    }

    static bool WaitForFroggyMaintenanceRestock(const MaintenanceMgr::Config& cfg, uint32_t timeoutMs) {
        const DWORD start = GetTickCount();
        while ((GetTickCount() - start) < timeoutMs) {
            const uint32_t regularSalvageKits =
                MaintenanceMgr::CountItemByModel(ItemModelIds::SALVAGE_KIT);
            const uint32_t highGradeSalvageKits =
                MaintenanceMgr::CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT) +
                MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT);
            if (MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT) >= cfg.targetIdKits &&
                regularSalvageKits >= cfg.targetSalvageKits &&
                highGradeSalvageKits == cfg.targetExpertSalvageKits) {
                return true;
            }
            Sleep(250);
        }
        const uint32_t regularSalvageKits =
            MaintenanceMgr::CountItemByModel(ItemModelIds::SALVAGE_KIT);
        const uint32_t highGradeSalvageKits =
            MaintenanceMgr::CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT) +
            MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT);
        return MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT) >= cfg.targetIdKits &&
               regularSalvageKits >= cfg.targetSalvageKits &&
               highGradeSalvageKits == cfg.targetExpertSalvageKits;
    }

    static ActionResult HandleFroggyRefreshCombatSkillbar(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        const bool ok = DungeonCombatRoutine::RefreshCombatSkillbarForDebug(Bot::Froggy::g_combatSession, "Froggy");
        return ok ? MakeOk() : MakeError("froggy_refresh_combat_skillbar_failed");
    }

    static ActionResult HandleFroggyRunSparkflyRouteToTekks(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        if (!EnsureFroggyConsetsReadyForRun("froggy_run_sparkfly_route_to_tekks")) {
            return MakeError("froggy_consets_missing_run_full_maintenance");
        }
        const bool ok = Bot::Froggy::DebugRunSparkflyRouteToTekks();
        return ok ? MakeOk() : MakeError("froggy_sparkfly_route_to_tekks_failed");
    }

    static ActionResult HandleFroggyRunTownSetup(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        Bot::BotConfig& cfg = Bot::GetConfig();
        const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
        if (MapMgr::GetMapId() != outpostMapId) {
            return MakeError("froggy_town_setup_requires_gadds");
        }

        if (!DungeonRuntime::WaitForTownRuntimeReady(outpostMapId, 10000u)) {
            return MakeError("town_runtime_not_ready");
        }

        DungeonOutpostSetup::Options options = {};
        options.default_hero_config_file = "Standard.txt";
        if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg, options)) {
            return MakeError("froggy_outpost_setup_failed");
        }

        (void)DungeonCombatRoutine::RefreshSkillCacheWithDebugLog(Bot::Froggy::g_combatSession, "Froggy");
        if (!EnsureFroggyConsetsReadyForRun("froggy_run_town_setup")) {
            return MakeError("froggy_consets_missing_run_full_maintenance");
        }
        Bot::SetState(Bot::BotState::Traveling);
        return MakeOk();
    }

    static ActionResult HandleFroggyTravelToGadds(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        constexpr uint32_t targetMapId = MapIds::GADDS_ENCAMPMENT;
        const uint32_t currentMapId = MapMgr::GetMapId();
        if (currentMapId == targetMapId) {
            return DungeonRuntime::WaitForTownRuntimeReady(targetMapId, 10000u)
                ? MakeOk()
                : MakeError("town_runtime_not_ready");
        }

        if (currentMapId != MapIds::EMBARK_BEACH) {
            Log::Warn("[LLM-Action] froggy_travel_to_gadds rejected from unsupported map=%u", currentMapId);
            return MakeError("froggy_travel_to_gadds_unsupported_map");
        }

        if (!MapMgr::Travel(targetMapId)) {
            return MakeError("froggy_travel_to_gadds_failed");
        }

        const DWORD start = GetTickCount();
        while ((GetTickCount() - start) < 60000u) {
            if (MapMgr::GetMapId() == targetMapId &&
                DungeonRuntime::WaitForTownRuntimeReady(targetMapId, 10000u)) {
                return MakeOk();
            }
            Sleep(500);
        }
        return MakeError("froggy_travel_to_gadds_timeout");
    }

    static ActionResult HandleFroggyTravelToSparkfly(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        if (!EnsureFroggyConsetsReadyForRun("froggy_travel_to_sparkfly")) {
            return MakeError("froggy_consets_missing_run_full_maintenance");
        }
        Bot::BotConfig& cfg = Bot::GetConfig();
        const Bot::BotState next = Bot::Froggy::HandleTravel(cfg);
        Bot::SetState(next);
        if (next == Bot::BotState::Error || next == Bot::BotState::Stopping) {
            return MakeError("froggy_travel_to_sparkfly_failed");
        }
        if (!DungeonRuntime::WaitForMapReady(MapIds::SPARKFLY_SWAMP, 15000u)) {
            return MakeError("sparkfly_runtime_not_ready");
        }
        return MakeOk();
    }

    static ActionResult HandleFroggyPrepareTekksDungeonEntry(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        const bool ok = Bot::Froggy::DebugPrepareTekksDungeonEntry();
        return ok ? MakeOk() : MakeError("froggy_prepare_tekks_dungeon_entry_failed");
    }

    static ActionResult HandleFroggyRunDungeonLoop(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        if (!EnsureFroggyConsetsReadyForRun("froggy_run_dungeon_loop")) {
            return MakeError("froggy_consets_missing_run_full_maintenance");
        }
        if (MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP) {
            Log::Info("[LLM-Action] froggy_run_dungeon_loop: routing from Sparkfly spawn toward Tekks before entry");
            if (!Bot::Froggy::DebugRunSparkflyRouteToTekks()) {
                return MakeError("froggy_sparkfly_route_to_tekks_failed");
            }
        }
        Bot::Froggy::ResetDungeonLoopTelemetry();
        const bool ok = Bot::Froggy::RunDungeonLoopFromCurrentMap();
        const uint32_t finalMapId = MapMgr::GetMapId();
        if (ok &&
            Bot::Froggy::g_dungeonLoopTelemetry.boss_completed &&
            (finalMapId == MapIds::BOGROOT_GROWTHS_LVL1 ||
             finalMapId == MapIds::BOGROOT_GROWTHS_LVL2)) {
            Log::Warn("[LLM-Action] froggy_run_dungeon_loop completed reward but remained in Bogroot map=%u; forcing return to outpost",
                      finalMapId);
            MapMgr::ReturnToOutpost();
            if (DungeonRuntime::WaitForMapReady(MapIds::GADDS_ENCAMPMENT, 120000u)) {
                return MakeOk();
            }
            return MakeError("froggy_dungeon_loop_reward_complete_still_in_bogroot");
        }
        return ok ? MakeOk() : MakeError("froggy_dungeon_loop_failed");
    }

    static ActionResult HandleFroggyRunMaintenanceCycle(const json& p) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        const bool includeSalvage = p.value("include_salvage", true);
        if (!WaitForReadableBackpackInventory(15000u, "froggy_run_maintenance_cycle pre")) {
            return MakeError("inventory_not_ready");
        }
        if (MerchantMgr::GetMerchantItemCount() == 0) {
            return MakeError("merchant_not_open_call_open_merchant_first");
        }

        const uint32_t freeBefore = MaintenanceMgr::CountFreeSlots();
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const uint32_t superiorBefore = MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
        const uint32_t salvageBefore = CountFroggySalvageKitFamily();
        Log::Info("[LLM-Action] froggy_run_maintenance_cycle: before free=%u gold=%u superiorId=%u salvage=%u",
                  freeBefore, goldBefore, superiorBefore, salvageBefore);

        const uint32_t identified = MaintenanceMgr::IdentifyAllItems();
        uint32_t salvaged = 0;
        if (includeSalvage) {
            Log::Info("[LLM-Action] froggy_run_maintenance_cycle: skipping maintenance salvage; native Froggy disables this path because salvage invalidates inventory roots");
        }
        const uint32_t sold = MaintenanceMgr::SellJunkItems();

        MaintenanceMgr::Config cfg = {};
        cfg.targetIdKits = 3;
        cfg.targetSalvageKits = 9;
        cfg.targetExpertSalvageKits = 1;
        MaintenanceMgr::BuyKitsToTarget(cfg);
        const bool restocked = WaitForFroggyMaintenanceRestock(cfg, 6000u);
        AgentMgr::CancelAction();
        Sleep(500);
        if (!WaitForReadableBackpackInventory(10000u, "froggy_run_maintenance_cycle post")) {
            return MakeError("inventory_not_ready_after_maintenance");
        }

        const uint32_t superiorAfter = MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
        const uint32_t regularSalvageAfter = MaintenanceMgr::CountItemByModel(ItemModelIds::SALVAGE_KIT);
        const uint32_t highGradeSalvageAfter =
            MaintenanceMgr::CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT) +
            MaintenanceMgr::CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT);
        const uint32_t salvageAfter = CountFroggySalvageKitFamily();
        Log::Info("[LLM-Action] froggy_run_maintenance_cycle: after free=%u gold=%u superiorId=%u regularSalv=%u highGradeSalv=%u totalSalv=%u identified=%u salvaged=%u sold=%u restocked=%d",
                  MaintenanceMgr::CountFreeSlots(),
                  ItemMgr::GetGoldCharacter(),
                  superiorAfter,
                  regularSalvageAfter,
                  highGradeSalvageAfter,
                  salvageAfter,
                  identified,
                  salvaged,
                  sold,
                  restocked ? 1 : 0);

        if (!restocked || superiorAfter < cfg.targetIdKits ||
            regularSalvageAfter < cfg.targetSalvageKits ||
            highGradeSalvageAfter != cfg.targetExpertSalvageKits) {
            return MakeError("froggy_maintenance_restock_failed");
        }
        return MakeOk();
    }

    static ActionResult HandleFroggyRunFullMaintenance(const json&) {
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        Bot::BotConfig& cfg = Bot::GetConfig();
        const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
        if (MapMgr::GetMapId() != outpostMapId) {
            return MakeError("froggy_full_maintenance_requires_gadds");
        }
        if (!DungeonRuntime::WaitForTownRuntimeReady(outpostMapId, 10000u)) {
            return MakeError("town_runtime_not_ready");
        }

        const Bot::BotState next = Bot::Froggy::HandleMaintenance(cfg);
        Bot::SetState(next);
        if (next == Bot::BotState::Traveling || next == Bot::BotState::InTown) {
            (void)DungeonCombatRoutine::RefreshSkillCacheWithDebugLog(Bot::Froggy::g_combatSession, "Froggy");
            if (!EnsureFroggyConsetsReadyForRun("froggy_run_full_maintenance")) {
                return MakeError("froggy_consets_missing_after_maintenance");
            }
            return MakeOk();
        }
        if (next == Bot::BotState::Maintenance) {
            return MakeError("froggy_full_maintenance_retry");
        }
        return MakeError("froggy_full_maintenance_failed");
    }

    void RegisterFroggyActions(ActionDispatchTable& dispatch) {
        dispatch["froggy_refresh_combat_skillbar"] = HandleFroggyRefreshCombatSkillbar;
        dispatch["froggy_run_town_setup"] = HandleFroggyRunTownSetup;
        dispatch["froggy_travel_to_gadds"] = HandleFroggyTravelToGadds;
        dispatch["froggy_travel_to_sparkfly"] = HandleFroggyTravelToSparkfly;
        dispatch["froggy_run_sparkfly_route_to_tekks"] = HandleFroggyRunSparkflyRouteToTekks;
        dispatch["froggy_prepare_tekks_dungeon_entry"] = HandleFroggyPrepareTekksDungeonEntry;
        dispatch["froggy_run_dungeon_loop"] = HandleFroggyRunDungeonLoop;
        dispatch["froggy_run_maintenance_cycle"] = HandleFroggyRunMaintenanceCycle;
        dispatch["froggy_run_full_maintenance"] = HandleFroggyRunFullMaintenance;
    }

} // namespace GWA3::LLM::ActionExecutor
