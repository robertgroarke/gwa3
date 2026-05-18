#include <gwa3/managers/MerchantMgr.h>
void ReportMerchantPreInteractState(const char* label, uint32_t npcId, float npcX, float npcY) {
    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const uint32_t currentTarget = AgentMgr::GetTargetId();
    const bool dialogOpen = DialogMgr::IsDialogOpen();
    const uint32_t dialogSender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t dialogButtons = DialogMgr::GetButtonCount();
    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(kMerchantRootHash);
    const uint32_t merchantItems = MerchantMgr::GetMerchantItemCount();
    const uint32_t heroCount = PartyMgr::CountPartyHeroes();
    const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    IntReport("  %s: npc=%u playerPos=(%.0f, %.0f) npcPos=(%.0f, %.0f) dist=%.0f target=%u dialogOpen=%d sender=%u buttons=%u merchantFrame=0x%08X items=%u heroes=%u",
              label,
              npcId,
              meX, meY,
              npcX, npcY,
              dist,
              currentTarget,
              dialogOpen ? 1 : 0,
              dialogSender,
              dialogButtons,
              static_cast<unsigned>(merchantFrame),
              merchantItems,
              heroCount);
}

void ReportMerchantRuntimeContext(const char* label) {
    IntReport("  %s: GameThread=%d onGameThread=%d RenderHook=%d hb=%u TraderHook=%d TargetLogHook=%d targetCalls=%u targetStores=%u CtoSHook=%d ctoSHb=%u",
              label,
              GameThread::IsInitialized() ? 1 : 0,
              GameThread::IsOnGameThread() ? 1 : 0,
              RenderHook::IsInitialized() ? 1 : 0,
              RenderHook::GetHeartbeat(),
              TraderHook::IsInitialized() ? 1 : 0,
              TargetLogHook::IsInitialized() ? 1 : 0,
              TargetLogHook::GetCallCount(),
              TargetLogHook::GetStoreCount(),
              CtoSHook::IsInitialized() ? 1 : 0,
              CtoSHook::GetHeartbeat());
}

void EmitMerchantScreenshotMarker(const char* reason) {
    IntReport("MERCHANT_SCREENSHOT_NOW: %s", reason);
}

void ReportMerchantTradeState(const char* label) {
    uintptr_t p0 = 0;
    uintptr_t p1 = 0;
    uintptr_t p2 = 0;
    uintptr_t merchantBase = 0;
    uintptr_t merchantSize = 0;
    bool ok = false;

    __try {
        if (Offsets::BasePointer > 0x10000) {
            p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
            if (p0 > 0x10000) {
                p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
                if (p1 > 0x10000) {
                    p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
                    if (p2 > 0x10000) {
                        merchantBase = *reinterpret_cast<uintptr_t*>(p2 + 0x24);
                        merchantSize = *reinterpret_cast<uintptr_t*>(p2 + 0x28);
                        ok = true;
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }

    IntReport("  %s: tradePtrs ok=%d p0=0x%08X p1=0x%08X p2=0x%08X merchantBase=0x%08X merchantSize=%u quoteId=%u costItem=%u costValue=%u",
              label,
              ok ? 1 : 0,
              static_cast<unsigned>(p0),
              static_cast<unsigned>(p1),
              static_cast<unsigned>(p2),
              static_cast<unsigned>(merchantBase),
              static_cast<unsigned>(merchantSize),
              TraderHook::GetQuoteId(),
              TraderHook::GetCostItemId(),
              TraderHook::GetCostValue());
}

void ReportMerchantInventoryList(const char* label, uint32_t limit = 16) {
    const uint32_t merchantCount = MerchantMgr::GetMerchantItemCount();
    IntReport("  %s: merchant item count=%u", label, merchantCount);
    const uint32_t capped = (merchantCount < limit) ? merchantCount : limit;
    for (uint32_t i = 0; i < capped; ++i) {
        Item* item = MerchantMgr::GetMerchantItemByPosition(i);
        if (!item) {
            IntReport("    [%u] <null>", i);
            continue;
        }
        IntReport("    [%u] item=%u model=%u type=%u value=%u quantity=%u", i,
                  item->item_id, item->model_id, item->type, item->value, item->quantity);
    }
    if (merchantCount > capped) {
        IntReport("    ... %u more merchant items not shown", merchantCount - capped);
    }
}

void BuildMerchantInventorySummary(char* out, size_t outSize, uint32_t limit = 8) {
    if (!out || outSize == 0) return;
    out[0] = '\0';

    const uint32_t merchantCount = MerchantMgr::GetMerchantItemCount();
    char tmp[64];
    sprintf_s(tmp, "count=%u models=", merchantCount);
    strcat_s(out, outSize, tmp);

    const uint32_t capped = (merchantCount < limit) ? merchantCount : limit;
    for (uint32_t i = 0; i < capped; ++i) {
        Item* item = MerchantMgr::GetMerchantItemByPosition(i);
        if (!item) {
            strcat_s(out, outSize, "null");
        } else {
            sprintf_s(tmp, "%u", item->model_id);
            strcat_s(out, outSize, tmp);
        }
        if (i + 1 < capped) {
            strcat_s(out, outSize, ",");
        }
    }
}

uint32_t CountInventoryModelQuantity(uint32_t modelId) {
    return CountBagModelQuantity(modelId, 1u, 4u);
}
