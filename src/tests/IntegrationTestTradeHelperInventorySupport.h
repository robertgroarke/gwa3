static uint32_t FindHelperInventoryItemByModel(uint32_t modelId) {
    if (modelId == 0) return 0;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (item && item->model_id == modelId && item->item_id > 0) {
                return item->item_id;
            }
        }
    }
    return 0;
}

static uint32_t CountHelperInventoryFreeSlots() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t totalFreeSlots = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = ItemMgr::GetBag(bagIdx);
        if (!bag) continue;
        const uint32_t capacity = bag->items.size;
        const uint32_t used = bag->items_count;
        totalFreeSlots += (capacity > used) ? (capacity - used) : 0;
    }
    return totalFreeSlots;
}

static uint32_t CountHelperInventoryModelQuantity(uint32_t modelId) {
    if (modelId == 0) return 0;

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t totalQuantity = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->item_id == 0 || item->model_id != modelId) continue;
            totalQuantity += item->quantity > 0 ? item->quantity : 1u;
        }
    }
    return totalQuantity;
}

static bool FindHelperStackableInventoryCandidate(uint32_t* outModelId, uint32_t* outQuantity) {
    if (outModelId) *outModelId = 0;
    if (outQuantity) *outQuantity = 0;

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->item_id == 0 || item->model_id == 0 || item->quantity < 2) continue;
            if (outModelId) *outModelId = item->model_id;
            if (outQuantity) *outQuantity = item->quantity;
            return true;
        }
    }
    return false;
}

static bool FindHelperSafeSingletonInventoryCandidate(uint32_t* outModelId) {
    if (outModelId) *outModelId = 0;

    static const uint32_t safeModels[] = {
        ItemModelIds::IDENTIFICATION_KIT,
        ItemModelIds::SALVAGE_KIT,
        ItemModelIds::EXPERT_SALVAGE_KIT,
    };

    for (uint32_t i = 0; i < _countof(safeModels); ++i) {
        const uint32_t modelId = safeModels[i];
        const uint32_t itemId = FindHelperInventoryItemByModel(modelId);
        if (itemId == 0) continue;
        auto* item = ItemMgr::GetItemById(itemId);
        if (!item || item->quantity != 1) continue;
        if (outModelId) *outModelId = modelId;
        return true;
    }
    return false;
}
