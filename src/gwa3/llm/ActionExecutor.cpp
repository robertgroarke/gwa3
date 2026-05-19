#include <gwa3/llm/ActionExecutor.h>
#include "ActionExecutorInternal.h"
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/LlmBridge.h>
#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/llm/Protocol.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/MerchantMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/advanced/Effects.h>
#include <gwa3/advanced/ItemActions.h>
#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonRuntime.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/Agent.h>
#include <bots/common/BotFramework.h>
#include <bots/froggy/FroggyHM.h>

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    // Rate limiter: max 50 actions per second (raised from 10 to support
    // bulk operations like material trader buys, which fire 100+ actions
    // in rapid succession during the conset cycle).
    static constexpr int MAX_ACTIONS_PER_SECOND = 50;
    static std::chrono::steady_clock::time_point g_rateWindow;
    static int g_rateCount = 0;

    static bool CheckRateLimit() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_rateWindow).count();
        if (elapsed >= 1000) {
            g_rateWindow = now;
            g_rateCount = 0;
        }
        if (g_rateCount >= MAX_ACTIONS_PER_SECOND) {
            return false;
        }
        g_rateCount++;
        return true;
    }

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

    static bool ShouldPauseSnapshotsDuringAction(const char* actionName) {
        if (!actionName) return false;
        return strcmp(actionName, "froggy_run_dungeon_loop") == 0 ||
               strcmp(actionName, "froggy_run_sparkfly_route_to_tekks") == 0 ||
               strcmp(actionName, "froggy_prepare_tekks_dungeon_entry") == 0 ||
               strcmp(actionName, "froggy_travel_to_gadds") == 0 ||
               strcmp(actionName, "froggy_travel_to_sparkfly") == 0 ||
               strcmp(actionName, "froggy_run_town_setup") == 0 ||
               strcmp(actionName, "froggy_run_full_maintenance") == 0;
    }

    static unsigned long SnapshotPauseBudgetForAction(const char* actionName) {
        if (!actionName) return 0u;
        if (strcmp(actionName, "froggy_run_dungeon_loop") == 0) return 7200000u;
        return 600000u;
    }

    ActionResult MakeOk() {
        ActionResult r;
        r.success = true;
        r.error[0] = '\0';
        return r;
    }

    ActionResult MakeError(const char* msg) {
        ActionResult r;
        r.success = false;
        strncpy_s(r.error, msg, sizeof(r.error) - 1);
        return r;
    }

    static ActionResult MakeBadParamsError(const nlohmann::json::exception& e) {
        char msg[128] = {};
        const char* field = "json";
        if (e.id >= 300 && e.id < 400) {
            field = "type_error";
        } else if (e.id >= 400 && e.id < 500) {
            field = "out_of_range";
        }
        sprintf_s(msg, "bad_params:%s", field);
        return MakeError(msg);
    }

    // Send action_result back to bridge
    static void SendResult(const char* requestId, bool success, const char* error) {
        json j;
        j["type"] = "action_result";
        GWA3::LLM::StampProtocol(j);
        j["request_id"] = requestId ? requestId : "";
        j["success"] = success;
        j["error"] = (error && error[0]) ? json(error) : json(nullptr);
        std::string s = j.dump();
        GWA3::Log::Info("[LLM-Action] SendResult begin: request_id=%s success=%d bytes=%u",
                        requestId ? requestId : "",
                        success ? 1 : 0,
                        static_cast<uint32_t>(s.size()));
        IpcServer::Send(s.c_str(),
                        static_cast<uint32_t>(s.size()),
                        IpcServer::OutboundPriority::ActionResult);
        GWA3::Log::Info("[LLM-Action] SendResult end: request_id=%s", requestId ? requestId : "");
    }

    // --- Action handlers ---

    static ActionDispatchTable g_dispatch;

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

    static void RegisterSkillbarAndFroggyActions() {
        g_dispatch["froggy_refresh_combat_skillbar"] = HandleFroggyRefreshCombatSkillbar;
        g_dispatch["froggy_run_town_setup"] = HandleFroggyRunTownSetup;
        g_dispatch["froggy_travel_to_gadds"] = HandleFroggyTravelToGadds;
        g_dispatch["froggy_travel_to_sparkfly"] = HandleFroggyTravelToSparkfly;
        g_dispatch["froggy_run_sparkfly_route_to_tekks"] = HandleFroggyRunSparkflyRouteToTekks;
        g_dispatch["froggy_prepare_tekks_dungeon_entry"] = HandleFroggyPrepareTekksDungeonEntry;
        g_dispatch["froggy_run_dungeon_loop"] = HandleFroggyRunDungeonLoop;
        g_dispatch["froggy_run_maintenance_cycle"] = HandleFroggyRunMaintenanceCycle;
        g_dispatch["froggy_run_full_maintenance"] = HandleFroggyRunFullMaintenance;
    }

    bool Initialize() {
        g_dispatch.clear();
        g_rateWindow = std::chrono::steady_clock::now();
        g_rateCount = 0;

        RegisterMovementActions(g_dispatch);
        RegisterCombatActions(g_dispatch);
        RegisterInteractionActions(g_dispatch);
        RegisterQuestActions(g_dispatch);
        RegisterPartyActions(g_dispatch);
        RegisterTravelActions(g_dispatch);
        RegisterItemActions(g_dispatch);
        RegisterTradeAndCraftingActions(g_dispatch);
        RegisterSkillbarActions(g_dispatch);
        RegisterSkillbarAndFroggyActions();
        RegisterBotControlActions(g_dispatch);
        RegisterUtilityActions(g_dispatch);

        GWA3::Log::Info("[LLM-Action] Initialized with %u actions", static_cast<uint32_t>(g_dispatch.size()));
        return true;
    }

    void Shutdown() {
        g_dispatch.clear();
        GWA3::Log::Info("[LLM-Action] Shutdown");
    }

    ActionResult Execute(const char* actionName, const char* paramsJson, const char* requestId) {
        if (!actionName || !actionName[0]) {
            auto r = MakeError("empty_action_name");
            SendResult(requestId, false, r.error);
            return r;
        }

        if (!CheckRateLimit()) {
            auto r = MakeError("rate_limited");
            SendResult(requestId, false, r.error);
            return r;
        }

        auto it = g_dispatch.find(actionName);
        if (it == g_dispatch.end()) {
            auto r = MakeError("unknown_action");
            SendResult(requestId, false, r.error);
            return r;
        }

        json params;
        if (paramsJson && paramsJson[0]) {
            try {
                params = json::parse(paramsJson);
            } catch (...) {
                auto r = MakeError("bad_params:json_parse");
                SendResult(requestId, false, r.error);
                return r;
            }
        }

        const bool pauseSnapshots = ShouldPauseSnapshotsDuringAction(actionName);
        if (pauseSnapshots) {
            GWA3::LLM::PauseSnapshotsFor(SnapshotPauseBudgetForAction(actionName));
        }

        GWA3::Log::Info("[LLM-Action] Executing: %s", actionName);
        ActionResult result;
        try {
            result = it->second(params);
        } catch (const nlohmann::json::exception& e) {
            result = MakeBadParamsError(e);
            GWA3::Log::Warn("[LLM-Action] Handler bad params: %s id=%d what=%s",
                            actionName,
                            e.id,
                            e.what());
        } catch (const std::exception& e) {
            result = MakeError("handler_exception");
            GWA3::Log::Warn("[LLM-Action] Handler exception: %s what=%s",
                            actionName,
                            e.what());
        } catch (...) {
            result = MakeError("handler_exception");
            GWA3::Log::Warn("[LLM-Action] Handler unknown exception: %s", actionName);
        }
        if (pauseSnapshots) {
            GWA3::LLM::PauseSnapshotsFor(0);
        }
        GWA3::Log::Info("[LLM-Action] Handler returned: %s success=%d error=%s",
                        actionName, result.success ? 1 : 0, result.error[0] ? result.error : "(none)");
        SendResult(requestId, result.success, result.error);
        GWA3::Log::Info("[LLM-Action] SendResult done: %s", actionName);
        return result;
    }

} // namespace GWA3::LLM::ActionExecutor
