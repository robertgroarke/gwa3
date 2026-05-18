#include <gwa3/managers/MerchantMgr.h>
bool CraftConsumableNatively(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                             uint32_t merchantItemPosition, uint32_t beforeCount,
                             uint32_t& afterCount, char* detail, size_t detailSize) {
    ConsumableCraftRecipe recipe{};
    if (!TryGetConsumableCraftRecipe(targetModelId, recipe) || merchantItemPosition == UINT32_MAX || merchantItemPosition == 0u) {
        return false;
    }

    const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
    MerchantStoCTap tap{};
    StartMerchantStoCTap(tap);
    CtoS::ResetPacketTap();

    // Use the proven working craft path from FroggyHM CraftConsetsIfNeeded / Gemma craft_item:
    // TransactItems(type=3, quantity=1, merchantItemId) — simple TRANSACT_ITEMS packet.
    // No quote, no UIMessage struct, no material arrays.
    // The game resolves materials and gold cost internally from the merchant context.
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "transact_craft_start itemPos=%u itemId=%u gold=%u",
                  merchantItemPosition, targetItemId, goldBefore);
    }
    WriteConsumableHarnessStatus("transact_craft_start", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, detail ? detail : "");

    // Dispatch TransactItems on the game thread — matching FroggyHM's GameThread::Enqueue path.
    // Direct calls to TransactItems from the test thread crash (SendPacket goes through
    // the CtoS sender thread, wrong execution context for merchant transactions).
    struct CraftTransactTask { uint32_t item_id; };
    static auto CraftTransactInvoker = [](void* storage) {
        auto* t = reinterpret_cast<CraftTransactTask*>(storage);
        if (t && t->item_id) MerchantMgr::TransactItems(3, 1, t->item_id);
    };
    CraftTransactTask craftTask{targetItemId};
    IntReport("  TransactItems(3, 1, %u) via GameThread — proven FroggyHM/Gemma craft path", targetItemId);
    GameThread::EnqueueRaw(CraftTransactInvoker, &craftTask, sizeof(craftTask));
    const bool craftQueued = true;
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "transact_craft_queued itemPos=%u itemId=%u gold=%u",
                  merchantItemPosition, targetItemId, goldBefore);
    }
    WriteConsumableHarnessStatus("transact_craft_queued", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, detail ? detail : "");
    if (!craftQueued) {
        StopMerchantStoCTap(tap);
        return false;
    }

    CtoS::ResetPacketTap();
    const bool craftObserved = WaitFor("native crafter transaction", 4000, [targetModelId, beforeCount, goldBefore]() {
        return CountInventoryModelQuantity(targetModelId) > beforeCount
            || ItemMgr::GetGoldCharacter() < goldBefore;
    });
    afterCount = CountInventoryModelQuantity(targetModelId);
    const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
    char stoCSummary[256] = {};
    char ctoSSummary[256] = {};
    FormatMerchantStoCTapSummary(stoCSummary, sizeof(stoCSummary), tap);
    FormatCtoSPacketTapSummary(ctoSSummary, sizeof(ctoSSummary), CtoS::GetPacketTapSnapshot());
    StopMerchantStoCTap(tap);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "native_craft_complete observed=%u before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                  craftObserved ? 1u : 0u, beforeCount, afterCount, goldBefore, goldAfter, stoCSummary, ctoSSummary);
    }
    WriteConsumableHarnessStatus("native_craft_complete", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, afterCount, afterCount > beforeCount ? 1u : 0u,
                                 detail ? detail : "");
    return afterCount > beforeCount;
}

bool CraftConsumableByPacket(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                             uint32_t merchantItemPosition, uint32_t beforeCount,
                             uint32_t& afterCount, char* detail, size_t detailSize) {
    (void)merchantItemPosition;
    (void)detail;
    (void)detailSize;
    afterCount = beforeCount;

    // Raw SendPacket(0x4C) and SendPacket(0x4D) crash the GW client in crafter context.
    // Both SendPacketViaGameCommand (PID 37560) and normal SendPacket (PID 30232) caused
    // Gw.exe crash dialogs. The game expects these operations to go through native
    // RequestQuoteFunction / TransactionFunction, not raw packet injection.
    // This path is disabled; CraftConsumableNatively now uses direct function calls.
    IntReport("  CraftConsumableByPacket DISABLED — raw 0x4C/0x4D packets crash the client");
    WriteConsumableHarnessStatus("packet_path_disabled", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 0,
                                 "raw_packet_quote_transact_crashes_client");
    return false;
}
