#include <gwa3/llm/ActionExecutor.h>
#include "ActionExecutorInternal.h"
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/LlmBridge.h>
#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/llm/Protocol.h>
#include <gwa3/core/Log.h>

#include <nlohmann/json.hpp>
#include <string>
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
        RegisterFroggyActions(g_dispatch);
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
