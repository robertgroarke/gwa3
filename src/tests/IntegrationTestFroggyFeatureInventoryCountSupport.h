static uint32_t CountInventoryModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    __try {
        for (uint32_t bagIndex = 0; bagIndex < 23; ++bagIndex) {
            Bag* bag = inv->bags[bagIndex];
            if (!bag || !bag->items.buffer) continue;

            for (uint32_t i = 0; i < bag->items.size; ++i) {
                Item* item = bag->items.buffer[i];
                if (!item) continue;
                if (item->model_id != modelId) continue;
                count += (item->quantity > 0 ? item->quantity : 1);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    return count;
}

static uint32_t CountInventoryModels(const uint32_t* modelIds, size_t modelCount) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv || !modelIds || modelCount == 0) return 0;

    uint32_t count = 0;
    for (uint32_t bagIndex = 0; bagIndex < 23; ++bagIndex) {
        Bag* bag = inv->bags[bagIndex];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item) continue;
            for (size_t m = 0; m < modelCount; ++m) {
                if (item->model_id != modelIds[m]) continue;
                count += (item->quantity > 0 ? item->quantity : 1);
                break;
            }
        }
    }
    return count;
}

static uint32_t CountSuperiorIdKits() {
    return CountInventoryModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
}

static uint32_t CountSalvageKitFamily() {
    static const uint32_t kSalvageModels[] = {
        ItemModelIds::SALVAGE_KIT,
        ItemModelIds::EXPERT_SALVAGE_KIT,
        ItemModelIds::RARE_SALVAGE_KIT,
        ItemModelIds::SUPERIOR_SALVAGE_KIT,
        ItemModelIds::ALT_SALVAGE_KIT
    };
    return CountInventoryModels(kSalvageModels, _countof(kSalvageModels));
}
