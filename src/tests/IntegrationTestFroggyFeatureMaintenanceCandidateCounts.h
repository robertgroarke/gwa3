static uint32_t CountUnidentifiedMaintenanceItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;
            if (IsIdentifiedForTest(item)) continue;
            if (IsKitModelForTest(item->model_id)) continue;
            if (MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            ++count;
        }
    }
    return count;
}

static uint32_t CountSalvageCandidatesForMaintenance() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;
            if (IsKitModelForTest(item->model_id)) continue;
            if (MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            if (!IsIdentifiedForTest(item)) continue;
            if (item->is_material_salvageable == 0) continue;

            const uint16_t rarity = GetItemRarityForTest(item);
            if (rarity != 2621 && rarity != 2623) continue;

            switch (item->type) {
            case 2:
            case 4:
            case 5:
            case 7:
            case 12:
            case 13:
            case 15:
            case 16:
            case 19:
            case 22:
            case 24:
            case 26:
            case 27:
            case 32:
            case 35:
            case 36:
                ++count;
                break;
            default:
                break;
            }
        }
    }
    return count;
}

static uint32_t CountSellCandidatesForMaintenance() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (MaintenanceMgr::ShouldSellItem(item)) ++count;
        }
    }
    return count;
}
