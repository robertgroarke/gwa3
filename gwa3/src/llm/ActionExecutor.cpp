#include <gwa3/llm/ActionExecutor.h>
#include <gwa3/llm/IpcServer.h>
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
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/game/Agent.h>

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <string>
#include <chrono>
#include <cstring>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    // Rate limiter: max 10 actions per second
    static constexpr int MAX_ACTIONS_PER_SECOND = 10;
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

    static ActionResult MakeOk() {
        ActionResult r;
        r.success = true;
        r.error[0] = '\0';
        return r;
    }

    static ActionResult MakeError(const char* msg) {
        ActionResult r;
        r.success = false;
        strncpy_s(r.error, msg, sizeof(r.error) - 1);
        return r;
    }

    // Send action_result back to bridge
    static void SendResult(const char* requestId, bool success, const char* error) {
        json j;
        j["type"] = "action_result";
        j["request_id"] = requestId ? requestId : "";
        j["success"] = success;
        j["error"] = (error && error[0]) ? error : nullptr;
        std::string s = j.dump();
        IpcServer::Send(s.c_str(), static_cast<uint32_t>(s.size()));
    }

    // --- Action handlers ---

    using ActionHandler = std::function<ActionResult(const json& params)>;
    static std::unordered_map<std::string, ActionHandler> g_dispatch;

    static ActionResult HandleMoveTo(const json& p) {
        if (!p.contains("x") || !p.contains("y")) return MakeError("missing x or y");
        float x = p["x"].get<float>();
        float y = p["y"].get<float>();
        if (std::abs(x) > 100000 || std::abs(y) > 100000) return MakeError("coordinates_out_of_range");
        if (!MapMgr::GetIsMapLoaded()) return MakeError("map_not_loaded");
        GWA3::GameThread::Enqueue([x, y]() { AgentMgr::Move(x, y); });
        return MakeOk();
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
        if (!MapMgr::GetIsMapLoaded()) return MakeError("map_not_loaded");
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
        if (!MapMgr::GetIsMapLoaded()) return MakeError("map_not_loaded");

        // Check recharge
        auto* skill = SkillMgr::GetSkillbarSkill(slot);
        if (skill && skill->recharge > 0) return MakeError("skill_on_recharge");

        uint32_t target = p.value("target_agent_id", 0u);
        uint32_t callTarget = p.value("call_target", 0u);
        GWA3::GameThread::Enqueue([slot, target, callTarget]() {
            SkillMgr::UseSkill(slot, target, callTarget);
        });
        return MakeOk();
    }

    static ActionResult HandleUseHeroSkill(const json& p) {
        if (!p.contains("hero_index") || !p.contains("slot")) return MakeError("missing hero_index or slot");
        uint32_t heroIdx = p["hero_index"].get<uint32_t>();
        uint32_t slot = p["slot"].get<uint32_t>();
        if (slot >= 8) return MakeError("invalid_slot");
        uint32_t target = p.value("target_agent_id", 0u);
        GWA3::GameThread::Enqueue([heroIdx, slot, target]() {
            SkillMgr::UseHeroSkill(heroIdx, slot, target);
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
        GWA3::GameThread::Enqueue([]() { PartyMgr::KickAllHeroes(); });
        return MakeOk();
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
        GWA3::GameThread::Enqueue([modelId, qty]() { ItemMgr::BuyMaterials(modelId, qty); });
        return MakeOk();
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
            TradeMgr::TransactItems(type, qty, itemId);
        });
        return MakeOk();
    }

    static ActionResult HandleSendChat(const json& p) {
        if (!p.contains("message") || !p.contains("channel"))
            return MakeError("missing message or channel");
        std::string message = p["message"].get<std::string>();
        std::string channel = p["channel"].get<std::string>();
        if (message.empty()) return MakeError("empty_message");

        wchar_t wMsg[256] = {};
        MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wMsg, 255);
        wchar_t ch = '!'; // default to all chat
        if (channel == "team" || channel == "party") ch = '#';
        else if (channel == "guild") ch = '@';
        else if (channel == "trade") ch = '$';
        else if (channel == "all") ch = '!';

        GWA3::GameThread::Enqueue([wMsg, ch]() {
            ChatMgr::SendChat(wMsg, ch);
        });
        return MakeOk();
    }

    static ActionResult HandleWait(const json& p) {
        // Wait is a no-op on the C++ side — the bridge handles timing
        (void)p;
        return MakeOk();
    }

    bool Initialize() {
        g_dispatch.clear();
        g_rateWindow = std::chrono::steady_clock::now();
        g_rateCount = 0;

        // Movement
        g_dispatch["move_to"] = HandleMoveTo;
        g_dispatch["change_target"] = HandleChangeTarget;
        g_dispatch["cancel_action"] = HandleCancelAction;

        // Combat
        g_dispatch["attack"] = HandleAttack;
        g_dispatch["call_target"] = HandleCallTarget;
        g_dispatch["use_skill"] = HandleUseSkill;
        g_dispatch["use_hero_skill"] = HandleUseHeroSkill;

        // Interaction
        g_dispatch["interact_npc"] = HandleInteractNpc;
        g_dispatch["interact_player"] = HandleInteractPlayer;
        g_dispatch["interact_signpost"] = HandleInteractSignpost;
        g_dispatch["dialog"] = HandleDialog;

        // Party/Hero
        g_dispatch["add_hero"] = HandleAddHero;
        g_dispatch["kick_hero"] = HandleKickHero;
        g_dispatch["kick_all_heroes"] = HandleKickAllHeroes;
        g_dispatch["flag_hero"] = HandleFlagHero;
        g_dispatch["flag_all"] = HandleFlagAll;
        g_dispatch["unflag_all"] = HandleUnflagAll;
        g_dispatch["set_hero_behavior"] = HandleSetHeroBehavior;
        g_dispatch["lock_hero_target"] = HandleLockHeroTarget;

        // Travel
        g_dispatch["travel"] = HandleTravel;
        g_dispatch["enter_mission"] = HandleEnterMission;
        g_dispatch["return_to_outpost"] = HandleReturnToOutpost;
        g_dispatch["set_hard_mode"] = HandleSetHardMode;
        g_dispatch["skip_cinematic"] = HandleSkipCinematic;

        // Items
        g_dispatch["pick_up_item"] = HandlePickUpItem;
        g_dispatch["use_item"] = HandleUseItem;
        g_dispatch["equip_item"] = HandleEquipItem;
        g_dispatch["drop_item"] = HandleDropItem;
        g_dispatch["move_item"] = HandleMoveItem;

        // Trade
        g_dispatch["buy_materials"] = HandleBuyMaterials;
        g_dispatch["request_quote"] = HandleRequestQuote;
        g_dispatch["transact_items"] = HandleTransactItems;

        // Utility
        g_dispatch["send_chat"] = HandleSendChat;
        g_dispatch["wait"] = HandleWait;

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
                auto r = MakeError("invalid_params_json");
                SendResult(requestId, false, r.error);
                return r;
            }
        }

        GWA3::Log::Info("[LLM-Action] Executing: %s", actionName);
        ActionResult result = it->second(params);
        SendResult(requestId, result.success, result.error);
        return result;
    }

} // namespace GWA3::LLM::ActionExecutor
