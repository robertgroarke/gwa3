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

    static ActionResult HandleMoveTo(const json& p) {
        if (!p.contains("x") || !p.contains("y")) return MakeError("missing x or y");
        float x = p["x"].get<float>();
        float y = p["y"].get<float>();
        if (std::abs(x) > 100000 || std::abs(y) > 100000) return MakeError("coordinates_out_of_range");
        // Lenient gate: accept any state where GW has a non-zero map and a
        // player id. The stricter GetIsMapLoaded() adds an extra check
        // that GetAgentByID(myId) is non-null, which transiently fails
        // for a few ticks after a skill cast (the agent pointer gets
        // refreshed by the cast) and was blocking every subsequent
        // action with map_not_loaded for ~30+ seconds at a time.
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        std::thread([x, y]() {
            (void)GWA3::DungeonNavigation::MoveToAndWait(x, y, 250.0f, 30000u);
        }).detach();
        return MakeOk();
    }

    static ActionResult HandleAggroMoveTo(const json& p) {
        if (!p.contains("x") || !p.contains("y")) return MakeError("missing x or y");
        float x = p["x"].get<float>();
        float y = p["y"].get<float>();
        if (std::abs(x) > 100000 || std::abs(y) > 100000) return MakeError("coordinates_out_of_range");
        // Lenient gate: accept any state where GW has a non-zero map and a
        // player id. The stricter GetIsMapLoaded() adds an extra check
        // that GetAgentByID(myId) is non-null, which transiently fails
        // for a few ticks after a skill cast (the agent pointer gets
        // refreshed by the cast) and was blocking every subsequent
        // action with map_not_loaded for ~30+ seconds at a time.
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        // aggro_move_to wraps Froggy's DebugAggroMoveTo ??? it walks toward
        // (x, y), fights any enemy that enters fight_range, sidesteps on
        // stuck detection, and re-issues moves until it arrives or times
        // out (internal ~240s budget). Handler returns immediately; the
        // walk completes on the detached worker thread so the bridge
        // pipe is not blocked.
        const float fightRange = p.value("fight_range", 1350.0f);
        std::thread([x, y, fightRange]() {
            Bot::Froggy::DebugAggroMoveTo(x, y, fightRange);
        }).detach();
        return MakeOk();
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

    static ActionResult HandleChangeTarget(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        GWA3::GameThread::Enqueue([id]() { AgentMgr::ChangeTarget(id); });
        return MakeOk();
    }

    static ActionResult HandleCancelAction(const json&) {
        GWA3::GameThread::Enqueue([]() { AgentMgr::CancelAction(); });
        return MakeOk();
    }

    static ActionResult HandleAttack(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        // Lenient gate: accept any state where GW has a non-zero map and a
        // player id. The stricter GetIsMapLoaded() adds an extra check
        // that GetAgentByID(myId) is non-null, which transiently fails
        // for a few ticks after a skill cast (the agent pointer gets
        // refreshed by the cast) and was blocking every subsequent
        // action with map_not_loaded for ~30+ seconds at a time.
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");
        auto* agent = AgentMgr::GetAgentByID(id);
        if (agent && agent->type == 0xDB) {
            auto* living = reinterpret_cast<AgentLiving*>(agent);
            if (living->hp <= 0.0f) return MakeError("target_dead");
        }
        GWA3::GameThread::Enqueue([id]() { AgentMgr::Attack(id); });
        return MakeOk();
    }

    static ActionResult HandleCallTarget(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        GWA3::GameThread::Enqueue([id]() { AgentMgr::CallTarget(id); });
        return MakeOk();
    }

    static ActionResult HandleUseSkill(const json& p) {
        if (!p.contains("slot")) return MakeError("missing slot");
        uint32_t slot = p["slot"].get<uint32_t>();
        if (slot >= 8) return MakeError("invalid_slot");
        // Lenient gate: accept any state where GW has a non-zero map and a
        // player id. The stricter GetIsMapLoaded() adds an extra check
        // that GetAgentByID(myId) is non-null, which transiently fails
        // for a few ticks after a skill cast (the agent pointer gets
        // refreshed by the cast) and was blocking every subsequent
        // action with map_not_loaded for ~30+ seconds at a time.
        if (MapMgr::GetMapId() == 0 || AgentMgr::GetMyId() == 0) return MakeError("map_not_loaded");

        // Check recharge (GetSkillbarSkill uses 0-based indexing)
        auto* skill = SkillMgr::GetSkillbarSkill(slot);
        if (skill && skill->recharge > 0) return MakeError("skill_on_recharge");

        uint32_t target = p.value("target_agent_id", 0u);
        uint32_t callTarget = p.value("call_target", 0u);
        // SkillMgr::UseSkill uses 1-BASED slot indexing internally (it
        // skips slot==0 as a sentinel and reads bar->skills[slot-1]).
        // The bridge schema advertises 0..7, so translate here rather
        // than leak the 1-based convention out to every LLM prompt.
        uint32_t nativeSlot = slot + 1u;
        GWA3::GameThread::Enqueue([nativeSlot, target, callTarget]() {
            SkillMgr::UseSkill(nativeSlot, target, callTarget);
        });
        return MakeOk();
    }

    static ActionResult HandleUseHeroSkill(const json& p) {
        if (!p.contains("hero_index") || !p.contains("slot")) return MakeError("missing hero_index or slot");
        uint32_t heroIdx = p["hero_index"].get<uint32_t>();
        uint32_t slot = p["slot"].get<uint32_t>();
        if (slot >= 8) return MakeError("invalid_slot");
        uint32_t target = p.value("target_agent_id", 0u);
        // Match HandleUseSkill: the native SkillMgr helpers use 1-based
        // slot indexing, the bridge schema advertises 0..7.
        uint32_t nativeSlot = slot + 1u;
        GWA3::GameThread::Enqueue([heroIdx, nativeSlot, target]() {
            SkillMgr::UseHeroSkill(heroIdx, nativeSlot, target);
        });
        return MakeOk();
    }

    static ActionResult HandleInteractNpc(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        GWA3::GameThread::Enqueue([id]() { AgentMgr::InteractNPC(id); });
        return MakeOk();
    }

    static ActionResult HandleInteractPlayer(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        GWA3::GameThread::Enqueue([id]() { AgentMgr::InteractPlayer(id); });
        return MakeOk();
    }

    static ActionResult HandleInteractSignpost(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        GWA3::GameThread::Enqueue([id]() { AgentMgr::InteractSignpost(id); });
        return MakeOk();
    }

    static ActionResult HandleDialog(const json& p) {
        if (!p.contains("dialog_id")) return MakeError("missing dialog_id");
        uint32_t id = p["dialog_id"].get<uint32_t>();
        GWA3::GameThread::Enqueue([id]() { QuestMgr::Dialog(id); });
        return MakeOk();
    }

    // --- Quest log manipulation ---
    // The quest must be present in the local quest log for the server to
    // honour any of these. GetQuestById guards against absurd IDs ??? we still
    // let the call through if the quest is not in the log yet (the log may
    // update between snapshot and action).
    static ActionResult HandleSetActiveQuest(const json& p) {
        if (!p.contains("quest_id")) return MakeError("missing quest_id");
        uint32_t id = p["quest_id"].get<uint32_t>();
        if (id == 0) return MakeError("quest_id_zero");
        GWA3::GameThread::Enqueue([id]() { QuestMgr::SetActiveQuest(id); });
        return MakeOk();
    }

    static ActionResult HandleAbandonQuest(const json& p) {
        if (!p.contains("quest_id")) return MakeError("missing quest_id");
        uint32_t id = p["quest_id"].get<uint32_t>();
        if (id == 0) return MakeError("quest_id_zero");
        if (!QuestMgr::GetQuestById(id)) return MakeError("quest_not_in_log");
        GWA3::GameThread::Enqueue([id]() { QuestMgr::AbandonQuest(id); });
        return MakeOk();
    }

    static ActionResult HandleRequestQuestInfo(const json& p) {
        if (!p.contains("quest_id")) return MakeError("missing quest_id");
        uint32_t id = p["quest_id"].get<uint32_t>();
        if (id == 0) return MakeError("quest_id_zero");
        GWA3::GameThread::Enqueue([id]() { QuestMgr::RequestQuestInfo(id); });
        return MakeOk();
    }

    static ActionResult HandleOpenQuestLog(const json&) {
        // SetWindowVisible(WindowID_QuestLog, 1) on the game thread.
        // Opening the window primes GW to render TextLabelFrames with
        // the "encoded | '\0' | decoded | '\0'" sibling layout we need
        // for read-only quest-name decoding.
        GWA3::GameThread::Enqueue([]() { QuestMgr::ToggleQuestLogWindow(); });
        return MakeOk();
    }

    static ActionResult HandleAddHero(const json& p) {
        if (!p.contains("hero_id")) return MakeError("missing hero_id");
        uint32_t id = p["hero_id"].get<uint32_t>();
        GWA3::GameThread::Enqueue([id]() { PartyMgr::AddHero(id); });
        return MakeOk();
    }

    static ActionResult HandleKickHero(const json& p) {
        if (!p.contains("hero_id")) return MakeError("missing hero_id");
        uint32_t id = p["hero_id"].get<uint32_t>();
        GWA3::GameThread::Enqueue([id]() { PartyMgr::KickHero(id); });
        return MakeOk();
    }

    static ActionResult HandleKickAllHeroes(const json&) {
        // Intentionally deprecated: the legacy bulk sentinel is not a
        // confirmed reliable clear path in gwa3. Bridge clients should issue
        // repeated kick_hero calls for the currently present hero IDs.
        return MakeError("deprecated_use_kick_hero_individually");
    }

    static ActionResult HandleFlagHero(const json& p) {
        if (!p.contains("hero_index") || !p.contains("x") || !p.contains("y"))
            return MakeError("missing hero_index, x, or y");
        uint32_t idx = p["hero_index"].get<uint32_t>();
        float x = p["x"].get<float>();
        float y = p["y"].get<float>();
        if (std::abs(x) > 100000 || std::abs(y) > 100000) return MakeError("coordinates_out_of_range");
        GWA3::GameThread::Enqueue([idx, x, y]() { PartyMgr::FlagHero(idx, x, y); });
        return MakeOk();
    }

    static ActionResult HandleFlagAll(const json& p) {
        if (!p.contains("x") || !p.contains("y")) return MakeError("missing x or y");
        float x = p["x"].get<float>();
        float y = p["y"].get<float>();
        if (std::abs(x) > 100000 || std::abs(y) > 100000) return MakeError("coordinates_out_of_range");
        GWA3::GameThread::Enqueue([x, y]() { PartyMgr::FlagAll(x, y); });
        return MakeOk();
    }

    static ActionResult HandleUnflagAll(const json&) {
        GWA3::GameThread::Enqueue([]() { PartyMgr::UnflagAll(); });
        return MakeOk();
    }

    static ActionResult HandleSetHeroBehavior(const json& p) {
        if (!p.contains("hero_index") || !p.contains("behavior"))
            return MakeError("missing hero_index or behavior");
        uint32_t idx = p["hero_index"].get<uint32_t>();
        uint32_t beh = p["behavior"].get<uint32_t>();
        if (beh > 2) return MakeError("invalid_behavior");
        GWA3::GameThread::Enqueue([idx, beh]() { PartyMgr::SetHeroBehavior(idx, beh); });
        return MakeOk();
    }

    static ActionResult HandleLockHeroTarget(const json& p) {
        if (!p.contains("hero_index") || !p.contains("target_id"))
            return MakeError("missing hero_index or target_id");
        uint32_t idx = p["hero_index"].get<uint32_t>();
        uint32_t tid = p["target_id"].get<uint32_t>();
        GWA3::GameThread::Enqueue([idx, tid]() { PartyMgr::LockHeroTarget(idx, tid); });
        return MakeOk();
    }

    static ActionResult HandleTravel(const json& p) {
        if (!p.contains("map_id")) return MakeError("missing map_id");
        uint32_t mapId = p["map_id"].get<uint32_t>();
        if (mapId == 0 || mapId > 999) return MakeError("invalid_map_id");
        uint32_t region = p.value("region", 0u);
        uint32_t district = p.value("district", 0u);
        uint32_t language = p.value("language", 0u);
        GWA3::GameThread::Enqueue([mapId, region, district, language]() {
            MapMgr::Travel(mapId, region, district, language);
        });
        return MakeOk();
    }

    static ActionResult HandleEnterMission(const json&) {
        GWA3::GameThread::Enqueue([]() { MapMgr::EnterMission(); });
        return MakeOk();
    }

    static ActionResult HandleReturnToOutpost(const json&) {
        GWA3::GameThread::Enqueue([]() { MapMgr::ReturnToOutpost(); });
        return MakeOk();
    }

    static ActionResult HandleSetHardMode(const json& p) {
        if (!p.contains("enabled")) return MakeError("missing enabled");
        bool enabled = p["enabled"].get<bool>();
        GWA3::GameThread::Enqueue([enabled]() { MapMgr::SetHardMode(enabled); });
        return MakeOk();
    }

    static ActionResult HandleSkipCinematic(const json&) {
        GWA3::GameThread::Enqueue([]() { MapMgr::SkipCinematic(); });
        return MakeOk();
    }

    static ActionResult HandlePickUpItem(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        GWA3::GameThread::Enqueue([id]() { ItemMgr::PickUpItem(id); });
        return MakeOk();
    }

    static ActionResult HandleUseItem(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t id = p["item_id"].get<uint32_t>();
        if (!ItemMgr::GetItemById(id)) return MakeError("item_not_found");
        GWA3::GameThread::Enqueue([id]() { ItemMgr::UseItem(id); });
        return MakeOk();
    }

    static ActionResult HandleEquipItem(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t id = p["item_id"].get<uint32_t>();
        if (!ItemMgr::GetItemById(id)) return MakeError("item_not_found");
        GWA3::GameThread::Enqueue([id]() { ItemMgr::EquipItem(id); });
        return MakeOk();
    }

    static ActionResult HandleDropItem(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t id = p["item_id"].get<uint32_t>();
        if (!ItemMgr::GetItemById(id)) return MakeError("item_not_found");
        GWA3::GameThread::Enqueue([id]() { ItemMgr::DropItem(id); });
        return MakeOk();
    }

    static ActionResult HandleMoveItem(const json& p) {
        if (!p.contains("item_id") || !p.contains("bag_id") || !p.contains("slot"))
            return MakeError("missing item_id, bag_id, or slot");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t bagId = p["bag_id"].get<uint32_t>();
        uint32_t slot = p["slot"].get<uint32_t>();
        if (!ItemMgr::GetItemById(itemId)) return MakeError("item_not_found");
        GWA3::GameThread::Enqueue([itemId, bagId, slot]() { ItemMgr::MoveItem(itemId, bagId, slot); });
        return MakeOk();
    }

    static ActionResult HandleBuyMaterials(const json& p) {
        if (!p.contains("model_id") || !p.contains("quantity"))
            return MakeError("missing model_id or quantity");
        uint32_t modelId = p["model_id"].get<uint32_t>();
        uint32_t qty = p["quantity"].get<uint32_t>();
        if (MerchantMgr::GetMerchantItemCount() == 0) {
            return MakeError("merchant_not_open_call_open_merchant_first");
        }
        const bool ok = MerchantMgr::BuyMaterials(modelId, qty);
        return ok ? MakeOk() : MakeError("buy_materials_failed");
    }

    static ActionResult HandleRequestQuote(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t id = p["item_id"].get<uint32_t>();
        GWA3::GameThread::Enqueue([id]() { ItemMgr::RequestQuote(id); });
        return MakeOk();
    }

    static ActionResult HandleTransactItems(const json& p) {
        if (!p.contains("type") || !p.contains("quantity") || !p.contains("item_id"))
            return MakeError("missing type, quantity, or item_id");
        uint32_t type = p["type"].get<uint32_t>();
        uint32_t qty = p["quantity"].get<uint32_t>();
        uint32_t itemId = p["item_id"].get<uint32_t>();
        GWA3::GameThread::Enqueue([type, qty, itemId]() {
            MerchantMgr::TransactItems(type, qty, itemId);
        });
        return MakeOk();
    }

    // Safer merchant buy/sell: route through TradeMgr's native helpers
    // (TransactionBuyNative / TransactionSellNative) instead of raw packet
    // 0x4D. The packet path crashes on some merchant states the same way
    // raw 0x39 INTERACT does. The native path computes the total value
    // from the item itself so we don't have to trust LLM-supplied prices.
    static ActionResult HandleMerchantBuy(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t qty = p.value("quantity", 1u);
        if (qty == 0) return MakeError("invalid_quantity");
        // Gate: refuse if the merchant isn't server-side-open. Firing
        // native buy/sell without a live merchant context DCs the client
        // (Code=007). GetMerchantItemCount() > 0 is populated only after
        // the server sends us the merchant inventory ??? same signal that
        // powers snapshot.merchant.is_open.
        if (MerchantMgr::GetMerchantItemCount() == 0) {
            return MakeError("merchant_not_open_call_open_merchant_first");
        }
        bool ok = MerchantMgr::BuyMerchantItem(itemId, qty);
        return ok ? MakeOk() : MakeError("buy_merchant_item_failed");
    }

    static ActionResult HandleMerchantSell(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t qty = p.value("quantity", 0u);
        if (MerchantMgr::GetMerchantItemCount() == 0) {
            return MakeError("merchant_not_open_call_open_merchant_first");
        }
        bool ok = MerchantMgr::SellInventoryItem(itemId, qty);
        return ok ? MakeOk() : MakeError("sell_inventory_item_failed");
    }

    static ActionResult HandleCraftItem(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t qty = p.value("quantity", 1u);
        uint32_t gold = p.value("gold", 250u * qty);

        // Use model_id + materials if provided (proven UIMessage path via
        // CraftMerchantItemByModelId), otherwise fall back to TransactItems.
        if (p.contains("model_id") && p.contains("material_model_ids") && p.contains("material_quantities")) {
            uint32_t modelId = p["model_id"].get<uint32_t>();
            auto matIds = p["material_model_ids"].get<std::vector<uint32_t>>();
            auto matQtys = p["material_quantities"].get<std::vector<uint32_t>>();
            uint32_t matCount = static_cast<uint32_t>(matIds.size());
            if (matCount == 0 || matCount != matQtys.size()) return MakeError("material arrays mismatch");
            if (matCount > 4) return MakeError("too many materials (max 4)");

            // Copy to fixed arrays for lambda capture
            uint32_t mIds[4] = {}, mQtys[4] = {};
            for (uint32_t i = 0; i < matCount; ++i) { mIds[i] = matIds[i]; mQtys[i] = matQtys[i]; }

            GWA3::GameThread::Enqueue([modelId, qty, gold, mIds, mQtys, matCount]() {
                MerchantMgr::CraftMerchantItemByModelId(modelId, qty, gold, mIds, mQtys, matCount);
            });
        } else {
            // Fallback: raw TransactItems (type=3 = crafter)
            GWA3::GameThread::Enqueue([itemId, qty]() {
                MerchantMgr::TransactItems(3, qty, itemId);
            });
        }
        return MakeOk();
    }

    // Open merchant dialog, matching the proven ConsetOpenNPCDialog sequence
    // used by the merchant workflow. GoNPC alone is not always enough to put
    // the client into the "active trader" state needed for quote responses ???
    // the server accepts the interaction packet but the client's trader
    // context pointer remains stale, so subsequent type=0xC quote requests
    // never receive kVendorQuote callbacks. We retry and fall back to the
    // native InteractNPC path, and verify via merchant item count that the
    // dialog is actually open before returning.
    // Open the Xunlai chest by sending the raw GoNPC INTERACT_NPC packet
    // (0x39) to a nearby Xunlai agent. This mirrors MaintenanceMgr::
    // OpenXunlaiChest and the packet-based open path.
    // AgentMgr::InteractNPC's native function path opens a UI dialog that
    // disrupts player agent state and does NOT establish the server-side
    // context needed for CHANGE_GOLD / MoveItem packets.
    //
    // On success, marks ItemMgr::MarkXunlaiOpened so HandleWithdrawGold /
    // HandleDepositGold can refuse when the open window has lapsed,
    // preventing the client from sending CHANGE_GOLD without a live
    // Xunlai context (which the server DCs as Code=007).
    static ActionResult HandleOpenXunlai(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");

        std::thread([id]() {
            GWA3::GameThread::Enqueue([id]() { AgentMgr::ChangeTarget(id); });
            Sleep(250);
            GWA3::GameThread::Enqueue([id]() {
                CtoS::SendPacket(3, Packets::INTERACT_NPC, id, 0u);
            });
            Sleep(2000);
            // Close the UI dialog ??? it blocks agent reads if left open.
            // Server-side context is established by the GoNPC packet, not
            // the UI. Matches MaintenanceMgr::OpenXunlaiChest exactly.
            GWA3::GameThread::Enqueue([]() { AgentMgr::CancelAction(); });
            Sleep(500);
            // Now it's safe for subsequent CHANGE_GOLD packets.
            ItemMgr::MarkXunlaiOpened();
            Log::Info("[LLM-Action] open_xunlai: marked Xunlai-opened for agent %u", id);
        }).detach();
        return MakeOk();
    }

    static ActionResult HandleOpenMerchant(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");

        std::thread([id]() {
            for (int attempt = 0; attempt < 3; ++attempt) {
                GWA3::GameThread::Enqueue([id]() { AgentMgr::ChangeTarget(id); });
                Sleep(250);
                GWA3::GameThread::Enqueue([id]() {
                    CtoS::SendPacket(3, Packets::INTERACT_NPC, id, 0u);
                });
                Sleep(2000);
                if (MerchantMgr::GetMerchantItemCount() > 0) {
                    Log::Info("[LLM-Action] open_merchant: dialog open after attempt %d (GoNPC): %u items",
                              attempt + 1, MerchantMgr::GetMerchantItemCount());
                    return;
                }
                // GoNPC alone didn't work ??? try native InteractNPC. This sets
                // up the client trader context that raw packet sends miss.
                if (attempt == 1) {
                    Log::Info("[LLM-Action] open_merchant: GoNPC alone failed, falling back to InteractNPC");
                    GWA3::GameThread::Enqueue([id]() { AgentMgr::InteractNPC(id); });
                    Sleep(2000);
                    if (MerchantMgr::GetMerchantItemCount() > 0) {
                        Log::Info("[LLM-Action] open_merchant: dialog open via InteractNPC: %u items",
                                  MerchantMgr::GetMerchantItemCount());
                        return;
                    }
                }
                Sleep(500);
            }
            Log::Warn("[LLM-Action] open_merchant: failed to open dialog for agent %u after 3 attempts", id);
        }).detach();
        return MakeOk();
    }

    // Withdraw/deposit gold between character and Xunlai storage
    static ActionResult HandleWithdrawGold(const json& p) {
        if (!p.contains("amount")) return MakeError("missing amount");
        uint32_t amount = p["amount"].get<uint32_t>();
        // Refuse if Xunlai hasn't been opened recently. Without this gate,
        // CHANGE_GOLD packets without an active Xunlai context are
        // server-invalid and DC the client with Code=007.
        if (!ItemMgr::IsXunlaiRecentlyOpened()) {
            return MakeError("xunlai_not_open_call_open_xunlai_first");
        }
        uint32_t charGold = ItemMgr::GetGoldCharacter();
        uint32_t storageGold = ItemMgr::GetGoldStorage();
        if (amount > storageGold) return MakeError("insufficient_storage_gold");
        uint32_t newChar = charGold + amount;
        uint32_t newStorage = storageGold - amount;
        if (newChar > 100000) return MakeError("would_exceed_character_gold_cap");
        GWA3::GameThread::Enqueue([newChar, newStorage]() {
            ItemMgr::ChangeGold(newChar, newStorage);
        });
        return MakeOk();
    }

    static ActionResult HandleDepositGold(const json& p) {
        if (!p.contains("amount")) return MakeError("missing amount");
        uint32_t amount = p["amount"].get<uint32_t>();
        if (!ItemMgr::IsXunlaiRecentlyOpened()) {
            return MakeError("xunlai_not_open_call_open_xunlai_first");
        }
        uint32_t charGold = ItemMgr::GetGoldCharacter();
        uint32_t storageGold = ItemMgr::GetGoldStorage();
        if (amount > charGold) return MakeError("insufficient_character_gold");
        uint32_t newChar = charGold - amount;
        uint32_t newStorage = storageGold + amount;
        GWA3::GameThread::Enqueue([newChar, newStorage]() {
            ItemMgr::ChangeGold(newChar, newStorage);
        });
        return MakeOk();
    }

    // Immediately serialize and send a fresh tier-3 snapshot.
    // Useful after state-changing operations (buy, craft, withdraw) to
    // bypass the 2s bridge-thread snapshot cadence.
    // Optional wait_ms: sleep this long before serializing, to allow
    // recent pending game operations to settle (e.g. after trader_buy).
    static ActionResult HandleQueryState(const json& p) {
        uint32_t waitMs = p.value("wait_ms", 0u);
        if (waitMs > 0 && waitMs < 10000) {
            Sleep(waitMs);
        }
        uint32_t len = 0;
        char* snap = GameSnapshot::SerializeTier3(&len);
        if (snap) {
            IpcServer::Send(snap, len, IpcServer::OutboundPriority::Snapshot);
            delete[] snap;
        }
        return MakeOk();
    }

    // Material trader: request quote + buy in one command.
    // Uses the proven native RequestQuote + TransactItem via Engine hook command queue.
    struct TraderBuyQuoteTask { uint32_t itemId; };
    static uint32_t s_traderBuyItemId = 0;

    static void __cdecl TraderBuyQuoteInvoker(void* storage) {
        auto* t = reinterpret_cast<TraderBuyQuoteTask*>(storage);
        if (!t || !t->itemId || !GWA3::Offsets::RequestQuote) return;
        s_traderBuyItemId = t->itemId;
        uint32_t* itemIdPtr = &s_traderBuyItemId;
        const uintptr_t fn = GWA3::Offsets::RequestQuote;
        __asm {
            mov eax, itemIdPtr
            push eax        // recv.item_ids
            push 1          // recv.item_count
            push 0          // recv.unknown
            push 0          // give.item_ids
            push 0          // give.item_count
            push 0          // give.unknown
            push 0          // unknown
            push 0xC        // type = TraderBuy
            xor ecx, ecx
            mov edx, 2
            mov eax, fn
            call eax
            add esp, 0x20
        }
    }

    struct TraderBuyTransactTask { uint32_t itemId; uint32_t cost; };
    static uint32_t s_traderBuyRecvId = 0;
    static uint32_t s_traderBuyRecvQty = 1;

    static void __cdecl TraderBuyTransactInvoker(void* storage) {
        auto* t = reinterpret_cast<TraderBuyTransactTask*>(storage);
        if (!t || !t->itemId || !GWA3::Offsets::Transaction) return;
        s_traderBuyRecvId = t->itemId;
        s_traderBuyRecvQty = 1;
        uint32_t* recvIds = &s_traderBuyRecvId;
        uint32_t* recvQtys = &s_traderBuyRecvQty;
        uint32_t goldGive = t->cost;
        const uintptr_t fn = GWA3::Offsets::Transaction;
        __asm {
            push recvQtys
            push recvIds
            push 1          // recv count
            push 0          // gold_recv
            push 0          // give qtys
            push 0          // give ids
            push 0          // give count
            push goldGive
            push 0xC        // type = TraderBuy
            mov eax, fn
            call eax
            add esp, 0x24
        }
    }

    // Scan global item array for a virtual merchant item matching model_id
    // (bag==nullptr, agent_id==0). Same logic as FindTraderVirtualItemId
    // in the native salvage flow.
    static uint32_t FindVirtualItemByModel(uint32_t modelId) {
        if (!GWA3::Offsets::BasePointer) return 0;
        __try {
            uintptr_t p0 = *reinterpret_cast<uintptr_t*>(GWA3::Offsets::BasePointer);
            if (p0 < 0x10000) return 0;
            uintptr_t p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
            if (p1 < 0x10000) return 0;
            uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x40);
            if (p2 < 0x10000) return 0;
            uint32_t arraySize = *reinterpret_cast<uint32_t*>(p2 + 0xC0);
            if (arraySize == 0 || arraySize > 8192) return 0;
            uintptr_t p3 = *reinterpret_cast<uintptr_t*>(p2 + 0xB8);
            if (p3 < 0x10000) return 0;
            for (uint32_t id = 1; id < arraySize; ++id) {
                uintptr_t itemPtr = *reinterpret_cast<uintptr_t*>(p3 + id * 4);
                if (itemPtr < 0x10000) continue;
                auto* item = reinterpret_cast<Item*>(itemPtr);
                if (item->bag == nullptr && item->agent_id == 0 && item->model_id == modelId) {
                    return item->item_id;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return 0;
    }

    static ActionResult HandleTraderBuy(const json& p) {
        // Accept either item_id directly or model_id (resolved via virtual item scan)
        uint32_t itemId = p.value("item_id", 0u);
        if (itemId == 0 && p.contains("model_id")) {
            uint32_t modelId = p["model_id"].get<uint32_t>();
            itemId = FindVirtualItemByModel(modelId);
            if (itemId == 0) return MakeError("virtual_item_not_found");
            Log::Info("[LLM-Action] trader_buy: resolved model=%u to item=%u", modelId, itemId);
        }
        if (itemId == 0) return MakeError("missing item_id or model_id");
        TraderHook::Reset();

        // Run synchronously on the init thread (where actions dispatch).
        // Synchronous execution prevents concurrent trader_buy calls from
        // clobbering each other's TraderHook state.
        TraderBuyQuoteTask quoteTask{itemId};
        CtoS::EnqueueGameCommand(&TraderBuyQuoteInvoker, &quoteTask, sizeof(quoteTask));

        // Wait for kVendorQuote response (up to 2 seconds)
        for (int i = 0; i < 20; ++i) {
            Sleep(100);
            uint32_t v = TraderHook::GetCostValue();
            if (v > 0 && v < 100000) break;
        }
        uint32_t price = TraderHook::GetCostValue();
        uint32_t costItemId = TraderHook::GetCostItemId();
        if (price == 0) {
            // Server returned a zero-price quote ??? this means the material
            // trader is out of stock for this item. The LLM should treat it
            // as a permanent failure for this material in this district and
            // skip further buys / craft attempts that depend on it.
            Log::Warn("[LLM-Action] trader_buy: trader out of stock item=%u (price=0)", itemId);
            return MakeError("out_of_stock");
        }
        if (price >= 100000) {
            Log::Warn("[LLM-Action] trader_buy: quote failed item=%u price=%u (implausible)", itemId, price);
            return MakeError("quote_failed");
        }
        Log::Info("[LLM-Action] trader_buy: quote item=%u price=%u ??? transacting", costItemId, price);

        // Transact with the quoted price
        TraderBuyTransactTask txTask{costItemId, price};
        CtoS::EnqueueGameCommand(&TraderBuyTransactInvoker, &txTask, sizeof(txTask));
        return MakeOk();
    }

    static ActionResult HandleInitiateTrade(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t agentId = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(agentId)) return MakeError("agent_not_found");
        uint32_t playerNumber = p.value("player_number", 0u);
        GWA3::LLM::PauseSnapshotsFor(2000);
        std::thread([agentId, playerNumber]() {
            constexpr uint32_t kInitiateTradeDispatchDelayMs = 500;
            Sleep(kInitiateTradeDispatchDelayMs);
            GWA3::GameThread::Enqueue([agentId, playerNumber]() {
                TradeMgr::InitiateTrade(agentId, playerNumber);
            });
        }).detach();
        return MakeOk();
    }

    static ActionResult HandleOfferTradeItem(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t quantity = p.value("quantity", 1u);
        // Arm the deferred offer ??? executes inside OnUpdateTradeCart callback
        TradeMgr::OfferItem(itemId, quantity);
        return MakeOk();
    }

    static ActionResult HandleOfferTradeItemPromptMax(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        auto* item = ItemMgr::GetItemById(itemId);
        if (!item) return MakeError("item_not_found");
        if (item->quantity <= 1) return MakeError("item_not_stackable");
        GWA3::GameThread::EnqueuePost([itemId]() {
            TradeMgr::OfferItemPromptMax(itemId);
        });
        return MakeOk();
    }

    static ActionResult HandleOfferTradeItemPromptDefault(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        auto* item = ItemMgr::GetItemById(itemId);
        if (!item) return MakeError("item_not_found");
        if (item->quantity <= 1) return MakeError("item_not_stackable");
        GWA3::GameThread::Enqueue([itemId]() {
            TradeMgr::OfferItemPromptDefault(itemId);
        });
        return MakeOk();
    }

    static ActionResult HandleOfferTradeItemPromptQuantity(const json& p) {
        if (!p.contains("item_id") || !p.contains("quantity"))
            return MakeError("missing item_id or quantity");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t quantity = p["quantity"].get<uint32_t>();
        auto* item = ItemMgr::GetItemById(itemId);
        if (!item) return MakeError("item_not_found");
        if (item->quantity <= 1) return MakeError("item_not_stackable");
        if (quantity == 0) return MakeError("invalid_quantity");
        if (quantity > item->quantity) return MakeError("quantity_exceeds_stack");
        GWA3::GameThread::Enqueue([itemId, quantity]() {
            TradeMgr::OfferItemPromptValue(itemId, quantity);
        });
        return MakeOk();
    }

    static ActionResult HandleSubmitTradeOffer(const json& p) {
        uint32_t gold = p.value("gold", 0u);
        GWA3::GameThread::Enqueue([gold]() { TradeMgr::SubmitOffer(gold); });
        return MakeOk();
    }

    static ActionResult HandleAcceptTrade(const json&) {
        GWA3::GameThread::Enqueue([]() { TradeMgr::AcceptTrade(); });
        return MakeOk();
    }

    static ActionResult HandleCancelTrade(const json& p) {
        const uint32_t row = p.value("row", 10u);
        const int32_t child = p.value("child", -1);
        const uint32_t transport = p.value("transport", 9u);
        GWA3::GameThread::Enqueue([row, child, transport]() { TradeMgr::CancelTrade(row, child, transport); });
        return MakeOk();
    }

    static ActionResult HandleChangeTradeOffer(const json&) {
        GWA3::GameThread::Enqueue([]() { TradeMgr::ChangeOffer(); });
        return MakeOk();
    }

    static ActionResult HandleRemoveTradeItem(const json& p) {
        if (!p.contains("slot_or_item_id")) return MakeError("missing slot_or_item_id");
        uint32_t slotOrItemId = p["slot_or_item_id"].get<uint32_t>();
        GWA3::GameThread::Enqueue([slotOrItemId]() { TradeMgr::RemoveItem(slotOrItemId); });
        return MakeOk();
    }

    static ActionResult HandleIdentifyItem(const json& p) {
        if (!p.contains("item_id") || !p.contains("kit_id"))
            return MakeError("missing item_id or kit_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t kitId = p["kit_id"].get<uint32_t>();
        if (!ItemMgr::GetItemById(itemId)) return MakeError("item_not_found");
        if (!ItemMgr::GetItemById(kitId)) return MakeError("kit_not_found");
        GWA3::GameThread::Enqueue([itemId, kitId]() { ItemMgr::IdentifyItem(itemId, kitId); });
        return MakeOk();
    }

    static ActionResult HandleSalvageStart(const json& p) {
        if (!p.contains("item_id") || !p.contains("kit_id"))
            return MakeError("missing item_id or kit_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        uint32_t kitId = p["kit_id"].get<uint32_t>();
        if (!ItemMgr::GetItemById(itemId)) return MakeError("item_not_found");
        if (!ItemMgr::GetItemById(kitId)) return MakeError("kit_not_found");
        GWA3::GameThread::Enqueue([kitId, itemId]() { ItemMgr::SalvageSessionOpen(kitId, itemId); });
        return MakeOk();
    }

    static ActionResult HandleSalvageMaterials(const json&) {
        GWA3::GameThread::Enqueue([]() { ItemMgr::SalvageMaterials(); });
        return MakeOk();
    }

    static ActionResult HandleSalvageDone(const json&) {
        GWA3::GameThread::Enqueue([]() { ItemMgr::SalvageSessionDone(); });
        return MakeOk();
    }

    static ActionResult HandleLoadSkillbar(const json& p) {
        if (!p.contains("skill_ids")) return MakeError("missing skill_ids");
        auto ids = p["skill_ids"];
        if (!ids.is_array() || ids.size() != 8) return MakeError("skill_ids must be array of 8");
        uint32_t skillIds[8] = {};
        for (int i = 0; i < 8; i++) {
            skillIds[i] = ids[i].get<uint32_t>();
        }
        uint32_t heroIndex = p.value("hero_index", 0u);
        GWA3::GameThread::Enqueue([skillIds, heroIndex]() {
            SkillMgr::LoadSkillbar(skillIds, heroIndex);
        });
        return MakeOk();
    }

    static void RegisterMovementActions() {
        g_dispatch["move_to"] = HandleMoveTo;
        g_dispatch["aggro_move_to"] = HandleAggroMoveTo;
        g_dispatch["change_target"] = HandleChangeTarget;
        g_dispatch["cancel_action"] = HandleCancelAction;
    }

    static void RegisterCombatActions() {
        g_dispatch["attack"] = HandleAttack;
        g_dispatch["call_target"] = HandleCallTarget;
        g_dispatch["use_skill"] = HandleUseSkill;
        g_dispatch["use_hero_skill"] = HandleUseHeroSkill;
    }

    static void RegisterInteractionActions() {
        g_dispatch["interact_npc"] = HandleInteractNpc;
        g_dispatch["interact_player"] = HandleInteractPlayer;
        g_dispatch["interact_signpost"] = HandleInteractSignpost;
        g_dispatch["dialog"] = HandleDialog;
    }

    static void RegisterQuestActions() {
        g_dispatch["set_active_quest"] = HandleSetActiveQuest;
        g_dispatch["abandon_quest"] = HandleAbandonQuest;
        g_dispatch["request_quest_info"] = HandleRequestQuestInfo;
        g_dispatch["open_quest_log"] = HandleOpenQuestLog;
    }

    static void RegisterPartyActions() {
        g_dispatch["add_hero"] = HandleAddHero;
        g_dispatch["kick_hero"] = HandleKickHero;
        g_dispatch["kick_all_heroes"] = HandleKickAllHeroes;
        g_dispatch["flag_hero"] = HandleFlagHero;
        g_dispatch["flag_all"] = HandleFlagAll;
        g_dispatch["unflag_all"] = HandleUnflagAll;
        g_dispatch["set_hero_behavior"] = HandleSetHeroBehavior;
        g_dispatch["lock_hero_target"] = HandleLockHeroTarget;
    }

    static void RegisterTravelActions() {
        g_dispatch["travel"] = HandleTravel;
        g_dispatch["enter_mission"] = HandleEnterMission;
        g_dispatch["return_to_outpost"] = HandleReturnToOutpost;
        g_dispatch["set_hard_mode"] = HandleSetHardMode;
        g_dispatch["skip_cinematic"] = HandleSkipCinematic;
    }

    static void RegisterItemActions() {
        g_dispatch["pick_up_item"] = HandlePickUpItem;
        g_dispatch["use_item"] = HandleUseItem;
        g_dispatch["equip_item"] = HandleEquipItem;
        g_dispatch["drop_item"] = HandleDropItem;
        g_dispatch["move_item"] = HandleMoveItem;

        g_dispatch["identify_item"] = HandleIdentifyItem;
        g_dispatch["salvage_start"] = HandleSalvageStart;
        g_dispatch["salvage_materials"] = HandleSalvageMaterials;
        g_dispatch["salvage_done"] = HandleSalvageDone;
    }

    static void RegisterTradeAndCraftingActions() {
        g_dispatch["initiate_trade"] = HandleInitiateTrade;
        g_dispatch["offer_trade_item"] = HandleOfferTradeItem;
        g_dispatch["offer_trade_item_prompt_max"] = HandleOfferTradeItemPromptMax;
        g_dispatch["offer_trade_item_prompt_default"] = HandleOfferTradeItemPromptDefault;
        g_dispatch["offer_trade_item_prompt_quantity"] = HandleOfferTradeItemPromptQuantity;
        g_dispatch["submit_trade_offer"] = HandleSubmitTradeOffer;
        g_dispatch["accept_trade"] = HandleAcceptTrade;
        g_dispatch["cancel_trade"] = HandleCancelTrade;
        g_dispatch["change_trade_offer"] = HandleChangeTradeOffer;
        g_dispatch["remove_trade_item"] = HandleRemoveTradeItem;
        g_dispatch["buy_materials"] = HandleBuyMaterials;
        g_dispatch["request_quote"] = HandleRequestQuote;
        g_dispatch["transact_items"] = HandleTransactItems;
        g_dispatch["merchant_buy"] = HandleMerchantBuy;
        g_dispatch["merchant_sell"] = HandleMerchantSell;
        g_dispatch["craft_item"] = HandleCraftItem;
        g_dispatch["open_merchant"] = HandleOpenMerchant;
        g_dispatch["open_xunlai"] = HandleOpenXunlai;
        g_dispatch["withdraw_gold"] = HandleWithdrawGold;
        g_dispatch["deposit_gold"] = HandleDepositGold;
        g_dispatch["trader_buy"] = HandleTraderBuy;
        g_dispatch["query_state"] = HandleQueryState;
    }

    static void RegisterSkillbarAndFroggyActions() {
        g_dispatch["load_skillbar"] = HandleLoadSkillbar;
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

        RegisterMovementActions();
        RegisterCombatActions();
        RegisterInteractionActions();
        RegisterQuestActions();
        RegisterPartyActions();
        RegisterTravelActions();
        RegisterItemActions();
        RegisterTradeAndCraftingActions();
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
