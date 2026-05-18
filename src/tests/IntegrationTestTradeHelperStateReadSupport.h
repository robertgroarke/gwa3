static uint32_t ReadTradeFlagsForHelper() {
    uintptr_t gc = Offsets::ResolveGameContext();
    if (!gc) return 0;
    __try {
        uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        if (trade <= 0x10000) return 0;
        return *reinterpret_cast<uint32_t*>(trade + 0x0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

struct HelperPartnerItemInfo {
    uint32_t item_id;
    uint32_t model_id;
    uint32_t quantity;
};

static bool ReadTradeStateForHelper(uint32_t& playerGold, uint32_t& partnerGold,
                                    uint32_t& playerItemCount, uint32_t& partnerItemCount) {
    playerGold = 0;
    partnerGold = 0;
    playerItemCount = 0;
    partnerItemCount = 0;
    uintptr_t gc = Offsets::ResolveGameContext();
    if (!gc) return false;
    __try {
        uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        if (trade <= 0x10000) return false;
        auto* ctx = reinterpret_cast<HelperTradeContextView*>(trade);
        playerGold = ctx->player.gold;
        partnerGold = ctx->partner.gold;
        playerItemCount = ctx->player.items.size;
        partnerItemCount = ctx->partner.items.size;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static size_t ReadTradePartnerItemsForHelper(HelperPartnerItemInfo* out, size_t capacity) {
    if (!out || capacity == 0) return 0;
    uintptr_t gc = Offsets::ResolveGameContext();
    if (!gc) return 0;
    __try {
        uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        if (trade <= 0x10000) return 0;
        auto* ctx = reinterpret_cast<HelperTradeContextView*>(trade);
        const uint32_t count = ctx->partner.items.size;
        const auto* items = ctx->partner.items.buffer;
        if (!items || count == 0) return 0;
        size_t written = 0;
        for (uint32_t i = 0; i < count && written < capacity; ++i) {
            const uint32_t itemId = items[i].item_id;
            const uint32_t qty = items[i].quantity;
            uint32_t modelId = 0;
            auto* fullItem = ItemMgr::GetItemById(itemId);
            if (fullItem) modelId = fullItem->model_id;
            out[written++] = {itemId, modelId, qty};
        }
        return written;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
