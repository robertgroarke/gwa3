#include <gwa3/llm/ActionExecutor.h>
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/LlmBridge.h>
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
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/game/Agent.h>
#include <gwa3/bot/BotFramework.h>

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <string>
#include <thread>
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
        GWA3::Log::Info("[LLM-Action] SendResult begin: request_id=%s success=%d bytes=%u",
                        requestId ? requestId : "",
                        success ? 1 : 0,
                        static_cast<uint32_t>(s.size()));
        IpcServer::Send(s.c_str(), static_cast<uint32_t>(s.size()));
        GWA3::Log::Info("[LLM-Action] SendResult end: request_id=%s", requestId ? requestId : "");
    }

    static void SendResultAfter(const char* requestId, bool success, const char* error, uint32_t delayMs) {
        const std::string req = requestId ? requestId : "";
        const std::string err = (error && error[0]) ? error : "";
        std::thread([req, success, err, delayMs]() {
            Sleep(delayMs);
            SendResult(req.c_str(), success, err.c_str());
            GWA3::Log::Info("[LLM-Action] Delayed SendResult done: initiate_trade after %u ms", delayMs);
        }).detach();
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
        // Dispatch move on GameThread post-dispatch to avoid crashing.
        // Both native Move and CtoS::MoveToCoord crash in LLM mode when
        // dispatched via GameThread::Enqueue (pre-dispatch). Using EnqueuePost
        // (post-dispatch) matches the proven MovePlayerNear pattern.
        if (GameThread::IsInitialized()) {
            GameThread::EnqueuePost([x, y]() {
                if (!MapMgr::GetIsMapLoaded() || AgentMgr::GetMyId() == 0) return;
                CtoS::MoveToCoord(x, y);
            });
        }
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
                TradeMgr::CraftMerchantItemByModelId(modelId, qty, gold, mIds, mQtys, matCount);
            });
        } else {
            // Fallback: raw TransactItems (type=3 = crafter)
            GWA3::GameThread::Enqueue([itemId, qty]() {
                TradeMgr::TransactItems(3, qty, itemId);
            });
        }
        return MakeOk();
    }

    // Open merchant dialog via GoNPC packet (proven working cadence).
    // Uses ChangeTarget + GoNPC(0x39) — matches the conset cycle approach.
    static ActionResult HandleOpenMerchant(const json& p) {
        if (!p.contains("agent_id")) return MakeError("missing agent_id");
        uint32_t id = p["agent_id"].get<uint32_t>();
        if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
        // Dispatch ChangeTarget + GoNPC on game thread with a small delay between
        std::thread([id]() {
            GWA3::GameThread::Enqueue([id]() { AgentMgr::ChangeTarget(id); });
            Sleep(250);
            GWA3::GameThread::Enqueue([id]() {
                CtoS::SendPacket(3, Packets::INTERACT_NPC, id, 0u);
            });
        }).detach();
        return MakeOk();
    }

    // Withdraw/deposit gold between character and Xunlai storage
    static ActionResult HandleWithdrawGold(const json& p) {
        if (!p.contains("amount")) return MakeError("missing amount");
        uint32_t amount = p["amount"].get<uint32_t>();
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

    static ActionResult HandleTraderBuy(const json& p) {
        if (!p.contains("item_id")) return MakeError("missing item_id");
        uint32_t itemId = p["item_id"].get<uint32_t>();
        TraderHook::Reset();

        // Run quote+transact on a background thread to avoid blocking the IPC
        std::thread([itemId]() {
            // Request quote via Engine hook command queue
            TraderBuyQuoteTask quoteTask{itemId};
            CtoS::EnqueueGameCommand(&TraderBuyQuoteInvoker, &quoteTask, sizeof(quoteTask));

            // Wait for kVendorQuote response (up to 3 seconds)
            for (int i = 0; i < 30; ++i) {
                Sleep(100);
                uint32_t v = TraderHook::GetCostValue();
                if (v > 0 && v < 100000) break;
            }
            uint32_t price = TraderHook::GetCostValue();
            uint32_t costItemId = TraderHook::GetCostItemId();
            if (price == 0 || price >= 100000) {
                Log::Warn("[LLM-Action] trader_buy: quote failed item=%u price=%u", itemId, price);
                return;
            }
            Log::Info("[LLM-Action] trader_buy: quote item=%u price=%u — transacting", costItemId, price);

            // Transact with the quoted price
            TraderBuyTransactTask txTask{costItemId, price};
            CtoS::EnqueueGameCommand(&TraderBuyTransactInvoker, &txTask, sizeof(txTask));
        }).detach();
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
        // Arm the deferred offer — executes inside OnUpdateTradeCart callback
        TradeMgr::OfferItem(itemId, quantity);
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

    static ActionResult HandleDropGold(const json& p) {
        if (!p.contains("amount")) return MakeError("missing amount");
        uint32_t amount = p["amount"].get<uint32_t>();
        GWA3::GameThread::Enqueue([amount]() { ItemMgr::DropGold(amount); });
        return MakeOk();
    }

    static ActionResult HandleSetCombatMode(const json& p) {
        if (!p.contains("mode")) return MakeError("missing mode");
        std::string mode = p["mode"].get<std::string>();
        auto& cfg = GWA3::Bot::GetConfig();
        if (mode == "builtin") {
            cfg.combat_mode = GWA3::Bot::CombatMode::Builtin;
        } else if (mode == "llm") {
            cfg.combat_mode = GWA3::Bot::CombatMode::LLM;
        } else {
            return MakeError("unknown_mode");
        }
        GWA3::Log::Info("[LLM-Action] Combat mode set to: %s", mode.c_str());
        return MakeOk();
    }

    static ActionResult HandleSetBotState(const json& p) {
        if (!p.contains("state")) return MakeError("missing state");
        std::string stateName = p["state"].get<std::string>();

        GWA3::Bot::BotState target;
        if (stateName == "idle") target = GWA3::Bot::BotState::Idle;
        else if (stateName == "in_town") target = GWA3::Bot::BotState::InTown;
        else if (stateName == "traveling") target = GWA3::Bot::BotState::Traveling;
        else if (stateName == "in_dungeon") target = GWA3::Bot::BotState::InDungeon;
        else if (stateName == "looting") target = GWA3::Bot::BotState::Looting;
        else if (stateName == "merchant") target = GWA3::Bot::BotState::Merchant;
        else if (stateName == "maintenance") target = GWA3::Bot::BotState::Maintenance;
        else if (stateName == "llm_controlled") target = GWA3::Bot::BotState::LLMControlled;
        else return MakeError("unknown_state");

        GWA3::Bot::SetState(target);
        GWA3::Log::Info("[LLM-Action] Bot state overridden to: %s", stateName.c_str());
        return MakeOk();
    }

    static ActionResult HandleResign(const json&) {
        wchar_t msg[] = L"/resign";
        GWA3::GameThread::Enqueue([msg]() {
            ChatMgr::SendChat(msg, L'/');
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

        // Salvage & Identify
        g_dispatch["identify_item"] = HandleIdentifyItem;
        g_dispatch["salvage_start"] = HandleSalvageStart;
        g_dispatch["salvage_materials"] = HandleSalvageMaterials;
        g_dispatch["salvage_done"] = HandleSalvageDone;

        // Trade & Crafting
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
        g_dispatch["craft_item"] = HandleCraftItem;
        g_dispatch["open_merchant"] = HandleOpenMerchant;
        g_dispatch["withdraw_gold"] = HandleWithdrawGold;
        g_dispatch["deposit_gold"] = HandleDepositGold;
        g_dispatch["trader_buy"] = HandleTraderBuy;

        // Skillbar
        g_dispatch["load_skillbar"] = HandleLoadSkillbar;

        // Bot control (advisory mode)
        g_dispatch["set_bot_state"] = HandleSetBotState;
        g_dispatch["set_combat_mode"] = HandleSetCombatMode;

        // Utility
        g_dispatch["send_chat"] = HandleSendChat;
        g_dispatch["drop_gold"] = HandleDropGold;
        g_dispatch["resign"] = HandleResign;
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
        GWA3::Log::Info("[LLM-Action] Handler returned: %s success=%d error=%s",
                        actionName, result.success ? 1 : 0, result.error[0] ? result.error : "(none)");
        const bool fireAndForget = !requestId || !requestId[0];
        if (fireAndForget) {
            GWA3::Log::Info("[LLM-Action] Fire-and-forget: skipping action_result for %s", actionName);
        } else {
            SendResult(requestId, result.success, result.error);
            GWA3::Log::Info("[LLM-Action] SendResult done: %s", actionName);
        }
        return result;
    }

} // namespace GWA3::LLM::ActionExecutor
