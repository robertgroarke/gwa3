#include <gwa3/managers/MerchantMgr.h>
// Consumable crafting integration test body. Included by IntegrationTestSession.cpp
// so it can use the session-local harness helpers while extraction continues.

// ===== CONSET CRAFT CYCLE =====

static bool ConsetMoveToNPC(float x, float y, const char* label) {
    IntReport("  Moving to %s at (%.0f, %.0f)...", label, x, y);
    const bool arrived = MovePlayerNear(x, y, 250.0f, 45000);
    if (!arrived) IntReport("  Failed to reach %s", label);
    else IntReport("  Arrived at %s", label);
    return arrived;
}

// Open an NPC dialog using the proven AutoIt GoNPC + Dialog packet sequence.
// ChangeTarget → GoNPC (0x39) → wait → Dialog (0x3A) → wait → verify merchant open.
// This matches GWA2's GoToNPC + Dialog flow and GWA3 MaintenanceMgr's Xunlai open.
static bool ConsetOpenNPCDialog(uint32_t npcId, const char* label) {
    IntReport("  Opening %s NPC %u via GoNPC packet sequence...", label, npcId);
    for (int attempt = 0; attempt < 3; ++attempt) {
        // Target the NPC first
        AgentMgr::ChangeTarget(npcId);
        Sleep(250);
        // GoNPC packet (0x39) — initiates interaction and may open merchant directly
    CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        Sleep(2000);
        if (MerchantMgr::GetMerchantItemCount() > 0) {
            IntReport("  %s opened on attempt %d (GoNPC only): %u items",
                      label, attempt + 1, MerchantMgr::GetMerchantItemCount());
            return true;
        }
        // If GoNPC alone didn't work, try InteractNPC native as fallback
        if (attempt == 1) {
            IntReport("  GoNPC alone didn't work, trying native InteractNPC...");
            AgentMgr::InteractNPC(npcId);
            Sleep(2000);
            if (MerchantMgr::GetMerchantItemCount() > 0) {
                IntReport("  %s opened via native InteractNPC: %u items",
                          label, MerchantMgr::GetMerchantItemCount());
                return true;
            }
        }
        IntReport("  %s not open after attempt %d, retrying...", label, attempt + 1);
        Sleep(500);
    }
    IntReport("  Failed to open %s after 3 attempts", label);
    return false;
}

static uint32_t ConsetFindNearestNPC(float x, float y, float maxDist = 500.0f) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    uint32_t bestId = 0;
    float bestDistSq = maxDist * maxDist;
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 6) continue;
        const float distSq = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (distSq < bestDistSq) { bestDistSq = distSq; bestId = i; }
    }
    return bestId;
}

// Find a trader virtual item by model ID in the global item array.
// AutoIt's TraderRequest scans [BasePtr]+0x18+0x40+0xB8 for items with
// bag==nullptr and agent_id==0 — these are merchant offering slots.
static uint32_t FindTraderVirtualItemId(uint32_t modelId) {
    // Scan global item array for virtual merchant items (bag==nullptr, agent_id==0)
    // Read item array size from [BasePtr]+0x18+0x40+0xC0 (same as AutoIt)
    uint32_t arraySize = 0;
    __try {
        uintptr_t p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x40);
        arraySize = *reinterpret_cast<uint32_t*>(p2 + 0xC0);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (arraySize == 0 || arraySize > 8192) return 0;
    IntReport("  FindTraderVirtualItemId: model=%u arraySize=%u", modelId, arraySize);
    uint32_t virtualCount = 0;
    for (uint32_t id = 1; id < arraySize; ++id) {
        __try {
            // Use the base pointer path: [BasePtr]+0x18+0x40+0xB8+[id*4]
            uintptr_t p0 = 0, p1 = 0, p2 = 0, p3 = 0, itemPtr = 0;
            if (!Offsets::BasePointer) break;
            p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
            if (p0 < 0x10000) break;
            p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
            if (p1 < 0x10000) break;
            p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x40);
            if (p2 < 0x10000) break;
            p3 = *reinterpret_cast<uintptr_t*>(p2 + 0xB8);
            if (p3 < 0x10000) break;
            itemPtr = *reinterpret_cast<uintptr_t*>(p3 + id * 4);
            if (itemPtr < 0x10000) continue;
            auto* item = reinterpret_cast<Item*>(itemPtr);
            if (item->bag == nullptr && item->agent_id == 0) {
                ++virtualCount;
                if (item->model_id == modelId) {
                    IntReport("  Found virtual item: id=%u model=%u at index=%u (scanned %u virtual items)",
                              item->item_id, item->model_id, id, virtualCount);
                    return item->item_id;
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
    }
    IntReport("  Virtual item model=%u NOT FOUND (scanned %u items, %u virtual)", modelId, arraySize, virtualCount);
    return 0;
}

// Request a trader quote via GameThread — calls the native RequestQuote
// function with type=0xC (TraderBuy). This avoids RenderHook shellcode
// which may not be active after bootstrap.
struct TraderQuoteTask { uint32_t itemId; };
static uint32_t s_traderQuoteItemId = 0;

static void __cdecl TraderQuoteInvoker(void* storage) {
    auto* t = reinterpret_cast<TraderQuoteTask*>(storage);
    if (!t || !t->itemId || !Offsets::RequestQuote) {
        Log::Warn("[INTG] TraderQuoteInvoker: bad args item=%u fn=0x%08X",
                  t ? t->itemId : 0, static_cast<unsigned>(Offsets::RequestQuote));
        return;
    }

    s_traderQuoteItemId = t->itemId;
    uint32_t* itemIdPtr = &s_traderQuoteItemId;
    const uintptr_t fn = Offsets::RequestQuote;
    Log::Info("[INTG] TraderQuoteInvoker: calling RequestQuote(0xC) item=%u itemAddr=0x%08X fn=0x%08X",
              t->itemId, reinterpret_cast<uintptr_t>(itemIdPtr), static_cast<unsigned>(fn));
    __asm {
        mov eax, itemIdPtr
        push eax        // recv.item_ids
        push 1          // recv.item_count
        push 0          // recv.unknown
        push 0          // give.item_ids
        push 0          // give.item_count
        push 0          // give.unknown
        push 0          // unknown arg2
        push 0xC        // type = TraderBuy (0xC)
        xor ecx, ecx
        mov edx, 2
        mov eax, fn
        call eax
        add esp, 0x20
    }
    Log::Info("[INTG] TraderQuoteInvoker: returned from RequestQuote — quoteId=%u costItem=%u costValue=%u",
              TraderHook::GetQuoteId(), TraderHook::GetCostItemId(), TraderHook::GetCostValue());
}

// Buy material packs from the material trader at Embark Beach.
// Reproduces AutoIt BuyMaterialIfMissing: quote → buy → re-quote → buy loop.
// Native TransactionFunction invoker for TraderBuy (type=0xC).
// Must be a standalone __cdecl function (not lambda) for inline ASM.
struct TraderTransactTask { uint32_t itemId; uint32_t cost; };
static uint32_t s_traderRecvItemId = 0;
static uint32_t s_traderRecvQty = 1;

static void __cdecl TraderTransactInvoker(void* storage) {
    auto* t = reinterpret_cast<TraderTransactTask*>(storage);
    if (!t || !t->itemId || !Offsets::Transaction) return;

    s_traderRecvItemId = t->itemId;
    s_traderRecvQty = 1;
    uint32_t* recvIds = &s_traderRecvItemId;
    uint32_t* recvQtys = &s_traderRecvQty;
    uint32_t goldGive = t->cost;
    const uintptr_t fn = Offsets::Transaction;
    __asm {
        push recvQtys      // recv.item_quantities
        push recvIds       // recv.item_ids
        push 1             // recv.item_count
        push 0             // gold_recv
        push 0             // give.item_quantities
        push 0             // give.item_ids
        push 0             // give.item_count
        push goldGive      // gold_give
        push 0xC           // type = TraderBuy
        mov eax, fn
        call eax
        add esp, 0x24
    }
    Log::Info("[INTG] TraderTransact: returned (item=%u cost=%u)", t->itemId, t->cost);
}

struct BuyResult { uint32_t finalCount; bool outOfStock; };

static BuyResult ConsetBuyMaterial(uint32_t modelId, uint32_t neededTotal) {
    const uint32_t have = CountInventoryModelQuantity(modelId);
    if (have >= neededTotal) return {have, false};
    const uint32_t missing = neededTotal - have;
    const uint32_t packs = (missing + 9) / 10;
    IntReport("  Buying material model=%u: have=%u need=%u missing=%u packs=%u",
              modelId, have, neededTotal, missing, packs);

    // Material buying uses the native RequestQuote/TransactItem API, NOT UI frame clicks.
    // We only need the virtual item ID from the global item array — the merchant list
    // position is irrelevant for the native API path.
    // Verify the material trader dialog is still open (merchant item count > 0).
    if (MerchantMgr::GetMerchantItemCount() == 0) {
        IntReport("  Material trader dialog not open — cannot buy model=%u", modelId);
        return {have, false};
    }

    uint32_t bought = 0;
    for (uint32_t p = 0; p < packs; ++p) {
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const uint32_t matBefore = CountInventoryModelQuantity(modelId);

        // Find the virtual item ID for this material
        const uint32_t traderItemId = FindTraderVirtualItemId(modelId);
        if (!traderItemId) {
            IntReport("  Virtual item not found for model=%u on pack %u", modelId, p + 1);
            break;
        }

        // Request quote via native RequestQuote (type=0xC) — this tells the server
        // we want to buy this item. The server responds with the price.
        TraderHook::Reset();
        IntReport("  Requesting quote for item=%u (pack %u/%u)...", traderItemId, p + 1, packs);
        TraderQuoteTask quoteTask{traderItemId};
        CtoS::EnqueueGameCommand(&TraderQuoteInvoker, &quoteTask, sizeof(quoteTask));

        // Wait for kVendorQuote UIMessage OR TraderHook response (whichever works)
        const bool quoteArrived = WaitFor("trader quote", 5000, []() {
            const uint32_t v = TraderHook::GetCostValue();
            return v > 0 && v < 100000; // sane gold range
        });
        const uint32_t quotedCost = TraderHook::GetCostValue();
        const uint32_t quotedItemId = TraderHook::GetCostItemId();
        IntReport("  Quote: arrived=%u item=%u price=%u", quoteArrived, quotedItemId, quotedCost);

        // Check if the trader has supply (price 0 = out of stock) or if we can afford it
        if (!quoteArrived || quotedCost == 0) {
            IntReport("  Material OUT OF STOCK or no quote — stopping buy for model=%u", modelId);
            const uint32_t oosFinal = CountInventoryModelQuantity(modelId);
            IntReport("  Material model=%u: bought=%u packs, final count=%u (OUT OF STOCK)", modelId, bought, oosFinal);
            return {oosFinal, true};
        }
        if (goldBefore < quotedCost + 1000) { // keep 1000g reserve for craft fees
            IntReport("  Low gold (%u) — stopping buy for model=%u", goldBefore, modelId);
            break;
        }

        // After RequestQuote, the game caches the quote. The AutoIt TraderBuy
        // passes TraderCostValue (from hook) as goldGive. Since we can't read the
        // Use the REAL quoted price from kVendorQuote UIMessage callback
        uint32_t itemForBuy = TraderHook::GetCostItemId();
        uint32_t goldForBuy = quotedCost;
        IntReport("  TransactItem(0xC): item=%u goldGive=%u", itemForBuy, goldForBuy);
        TraderTransactTask txTask{itemForBuy, goldForBuy};
        CtoS::EnqueueGameCommand(&TraderTransactInvoker, &txTask, sizeof(txTask));

        // Wait for gold decrease or inventory increase
        const bool buyOk = WaitFor("material buy", 5000, [goldBefore, modelId, matBefore]() {
            return ItemMgr::GetGoldCharacter() < goldBefore
                || CountInventoryModelQuantity(modelId) > matBefore;
        });
        const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
        const uint32_t matAfter = CountInventoryModelQuantity(modelId);
        if (buyOk) {
            ++bought;
            IntReport("  Bought pack %u/%u: gold=%u->%u mat=%u->%u",
                      p + 1, packs, goldBefore, goldAfter, matBefore, matAfter);
        } else {
            IntReport("  Buy failed at pack %u (gold=%u->%u mat=%u->%u)",
                      p + 1, goldBefore, goldAfter, matBefore, matAfter);
            break;
        }
        Sleep(ChatMgr::GetPing() + 500);
        // Every 20 packs, pause longer to let the game catch up
        if ((p + 1) % 20 == 0) {
            IntReport("  Pausing after %u packs to let game settle...", p + 1);
            Sleep(2000);
        }
    }
    const uint32_t finalCount = CountInventoryModelQuantity(modelId);
    IntReport("  Material model=%u: bought=%u packs, final count=%u", modelId, bought, finalCount);
    return {finalCount, false};
}

// Craft ALL affordable units of a consumable at one crafter NPC.
// Walks to the crafter once, opens dialog, then uses MerchantMgr::CraftMerchantItemByModelId
// which calls the native Transaction function (via UIMessage or direct call).
// This avoids the UI button-click approach which breaks when multiple merchant
// dialogs are opened in the same session (stale frame contexts).
static uint32_t ConsetCraftAllAtNPC(const char* traderLabel, float traderX, float traderY,
                                     uint32_t targetModelId, uint32_t maxCrafts,
                                     const uint32_t* matModels, const uint32_t* matQtys,
                                     uint32_t matCount) {
    IntReport("  --- Crafting up to %u at %s (model=%u) ---", maxCrafts, traderLabel, targetModelId);
    if (maxCrafts == 0) return 0;

    if (!ConsetMoveToNPC(traderX, traderY, traderLabel)) {
        IntReport("  Failed to reach %s", traderLabel);
        return 0;
    }
    uint32_t npc = ConsetFindNearestNPC(traderX, traderY);
    if (!npc) { IntReport("  No NPC near %s", traderLabel); return 0; }
    if (!ConsetOpenNPCDialog(npc, traderLabel)) return 0;
    IntReport("  %s merchant open: %u items", traderLabel, MerchantMgr::GetMerchantItemCount());

    // Batch crafting: call Transaction with qty>1 per call.
    // Each material stack caps at 250, and each recipe needs 50 per craft,
    // so max batch = 250/50 = 5 per call (limited by single-stack lookup).
    // We loop in batches of up to 5, which is much faster than one-at-a-time.
    constexpr uint32_t kMaxBatchPerCall = 5u; // 5 * 50 = 250 = max stack size

    uint32_t crafted = 0;

    while (crafted < maxCrafts) {
        uint32_t remaining = maxCrafts - crafted;
        uint32_t batchSize = (remaining > kMaxBatchPerCall) ? kMaxBatchPerCall : remaining;
        const uint32_t batchGold = 250u * batchSize;
        const uint32_t loopBefore = CountInventoryModelQuantity(targetModelId);
        const uint32_t loopGoldBefore = ItemMgr::GetGoldCharacter();

        if (loopGoldBefore < batchGold) {
            // Try fewer if we can't afford the full batch
            batchSize = loopGoldBefore / 250u;
            if (batchSize == 0) {
                IntReport("  Out of gold (%u) — stopping craft at %s", loopGoldBefore, traderLabel);
                break;
            }
        }

        const uint32_t actualGold = 250u * batchSize;
        const bool dispatched = MerchantMgr::CraftMerchantItemByModelId(
            targetModelId, batchSize, actualGold, matModels, matQtys, matCount);
        if (!dispatched) {
            // If batch rejected, try qty=1 as last resort
            if (batchSize > 1) {
                IntReport("  Batch=%u rejected, trying qty=1 at %s", batchSize, traderLabel);
                batchSize = 1;
                const bool single = MerchantMgr::CraftMerchantItemByModelId(
                    targetModelId, 1, 250, matModels, matQtys, matCount);
                if (!single) {
                    IntReport("  CraftMerchantItemByModelId rejected (missing materials?) at %s", traderLabel);
                    break;
                }
            } else {
                IntReport("  CraftMerchantItemByModelId rejected (missing materials?) at %s", traderLabel);
                break;
            }
        }

        const bool ok = WaitFor("craft completion", 8000, [targetModelId, loopBefore, loopGoldBefore]() {
            return CountInventoryModelQuantity(targetModelId) > loopBefore || ItemMgr::GetGoldCharacter() < loopGoldBefore;
        });
        if (!ok) {
            IntReport("  Craft completion wait timed out at %s", traderLabel);
        }
        Sleep(300); // let server settle
        const uint32_t loopAfter = CountInventoryModelQuantity(targetModelId);
        const uint32_t batchCrafted = (loopAfter > loopBefore) ? (loopAfter - loopBefore) : 0;
        if (batchCrafted > 0) {
            crafted += batchCrafted;
            IntReport("  Crafted %u (batch=%u) total=%u/%u at %s (gold=%u->%u)",
                      batchCrafted, batchSize, crafted, maxCrafts, traderLabel,
                      loopGoldBefore, ItemMgr::GetGoldCharacter());
        } else {
            IntReport("  Craft failed at %s (count unchanged %u, gold=%u->%u) — likely out of materials",
                      traderLabel, loopAfter, loopGoldBefore, ItemMgr::GetGoldCharacter());
            break;
        }
        Sleep(ChatMgr::GetPing() + 300);
    }
    IntReport("  Finished at %s: crafted %u/%u", traderLabel, crafted, maxCrafts);
    return crafted;
}
