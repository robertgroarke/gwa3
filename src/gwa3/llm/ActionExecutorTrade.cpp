#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/LlmBridge.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MerchantMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/game/Item.h>

#include <Windows.h>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {
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

    void RegisterTradeAndCraftingActions(ActionDispatchTable& dispatch) {
        dispatch["initiate_trade"] = HandleInitiateTrade;
        dispatch["offer_trade_item"] = HandleOfferTradeItem;
        dispatch["offer_trade_item_prompt_max"] = HandleOfferTradeItemPromptMax;
        dispatch["offer_trade_item_prompt_default"] = HandleOfferTradeItemPromptDefault;
        dispatch["offer_trade_item_prompt_quantity"] = HandleOfferTradeItemPromptQuantity;
        dispatch["submit_trade_offer"] = HandleSubmitTradeOffer;
        dispatch["accept_trade"] = HandleAcceptTrade;
        dispatch["cancel_trade"] = HandleCancelTrade;
        dispatch["change_trade_offer"] = HandleChangeTradeOffer;
        dispatch["remove_trade_item"] = HandleRemoveTradeItem;
        dispatch["buy_materials"] = HandleBuyMaterials;
        dispatch["request_quote"] = HandleRequestQuote;
        dispatch["transact_items"] = HandleTransactItems;
        dispatch["merchant_buy"] = HandleMerchantBuy;
        dispatch["merchant_sell"] = HandleMerchantSell;
        dispatch["craft_item"] = HandleCraftItem;
        dispatch["open_merchant"] = HandleOpenMerchant;
        dispatch["open_xunlai"] = HandleOpenXunlai;
        dispatch["withdraw_gold"] = HandleWithdrawGold;
        dispatch["deposit_gold"] = HandleDepositGold;
        dispatch["trader_buy"] = HandleTraderBuy;
        dispatch["query_state"] = HandleQueryState;
    }


} // namespace GWA3::LLM::ActionExecutor