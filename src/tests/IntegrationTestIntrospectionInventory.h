bool TestInventoryIntrospection() {
    IntReport("=== Inventory Deep Introspection ===");

    if (ReadMyId() == 0) {
        IntSkip("Inventory introspection", "Not in game");
        IntReport("");
        return false;
    }

    Inventory* inv = ItemMgr::GetInventory();
    IntReport("  Inventory struct: %p", inv);
    IntCheck("Inventory struct available", inv != nullptr);
    if (!inv) {
        IntReport("");
        return false;
    }

    const uint32_t goldChar = inv->gold_character;
    const uint32_t goldStore = inv->gold_storage;
    IntReport("  Gold: character=%u storage=%u", goldChar, goldStore);
    IntCheck("Character gold plausible (< 1M)", goldChar < 1000000);
    IntCheck("Storage gold plausible (< 10M)", goldStore < 10000000);

    const uint32_t goldCharApi = ItemMgr::GetGoldCharacter();
    const uint32_t goldStoreApi = ItemMgr::GetGoldStorage();
    IntCheck("GetGoldCharacter matches inventory struct", goldCharApi == goldChar);
    IntCheck("GetGoldStorage matches inventory struct", goldStoreApi == goldStore);

    uint32_t totalItems = 0;
    uint32_t bagsPopulated = 0;
    uint32_t maxModelId = 0;
    uint32_t minItemId = UINT32_MAX;

    for (uint32_t bagIdx = 0; bagIdx < 23; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;

        bagsPopulated++;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item) continue;
            totalItems++;

            if (item->model_id > maxModelId) maxModelId = item->model_id;
            if (item->item_id < minItemId) minItemId = item->item_id;

            if (totalItems <= 3) {
                IntReport("    Sample item [bag%u slot%u]: item_id=%u model_id=%u qty=%u type=%u value=%u",
                          bagIdx, i, item->item_id, item->model_id, item->quantity,
                          item->type, item->value);
            }
        }
    }

    IntReport("  Bag stats: populated=%u totalItems=%u maxModelId=%u minItemId=%u",
              bagsPopulated, totalItems, maxModelId, minItemId == UINT32_MAX ? 0 : minItemId);
    IntCheck("At least 1 bag populated", bagsPopulated > 0);
    IntCheck("At least 1 item in inventory", totalItems > 0);

    IntReport("  Active weapon set: %u", inv->active_weapon_set);
    IntCheck("Active weapon set in range (0-3)", inv->active_weapon_set < 4);

    if (minItemId != UINT32_MAX) {
        Item* lookedUp = ItemMgr::GetItemById(minItemId);
        IntReport("  GetItemById(%u) round-trip: %p", minItemId, lookedUp);
        IntCheck("GetItemById returns non-null for known item", lookedUp != nullptr);
        if (lookedUp) {
            IntCheck("GetItemById item_id matches", lookedUp->item_id == minItemId);
        }
    }

    Bag* bag1 = ItemMgr::GetBag(1);
    IntReport("  GetBag(1): %p", bag1);
    IntCheck("GetBag(1) returns a bag (backpack)", bag1 != nullptr);

    IntReport("");
    return true;
}
