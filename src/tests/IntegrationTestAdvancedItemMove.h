bool TestItemMove() {
    IntReport("=== Item Move ===");

    if (ReadMyId() == 0) { IntSkip("ItemMove", "Not in game"); IntReport(""); return false; }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) { IntSkip("ItemMove", "No inventory"); IntReport(""); return false; }

    // Find an item and an empty slot in a different bag
    Item* item = FindFirstBackpackItem();
    if (!item || !item->bag) {
        IntSkip("ItemMove", "No item in backpack");
        IntReport("");
        return false;
    }

    uint32_t srcBagIdx = item->bag->index;
    uint32_t srcSlot = item->slot;
    uint32_t itemId = item->item_id;
    const uint32_t itemsBefore = CountBackpackItems();
    Bag* srcBagPtr = item->bag;
    const uint32_t srcInvBagSlot = FindInventoryBagSlot(inv, srcBagPtr);
    IntReport("  Moving item %u (model=%u) from bag %u slot %u...",
              itemId, item->model_id, srcBagIdx, srcSlot);
    IntReport("  Source: invBag[%u] runtimeIndex=%u packetBagId=%u slot %u",
              srcInvBagSlot,
              srcBagPtr ? srcBagPtr->index : 0,
              srcBagPtr ? srcBagPtr->h0008 : 0,
              srcSlot);

    // Find any free slot across all backpack bags (1-4)
    uint32_t dstBagIdx = UINT32_MAX;
    uint32_t freeSlot = UINT32_MAX;
    for (uint32_t bIdx = 1; bIdx <= 4 && freeSlot == UINT32_MAX; ++bIdx) {
        Bag* bag = inv->bags[bIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            // Skip the source slot itself
            if (bIdx == srcBagIdx && i == srcSlot) continue;
            if (!bag->items.buffer[i]) {
                dstBagIdx = bIdx;
                freeSlot = i;
                break;
            }
        }
    }

    if (freeSlot == UINT32_MAX) {
        IntSkip("ItemMove", "No free slot in any backpack bag");
        IntReport("");
        return true;
    }

    Bag* targetBagPtr = inv->bags[dstBagIdx];
    IntReport("  Target: invBag[%u] runtimeIndex=%u packetBagId=%u slot %u",
              dstBagIdx,
              targetBagPtr ? targetBagPtr->index : 0,
              targetBagPtr ? targetBagPtr->h0008 : 0,
              freeSlot);
    ItemMgr::MoveItem(itemId, dstBagIdx, freeSlot);
    Sleep(500 + ChatMgr::GetPing());

    // Verify item moved
    Item* movedItem = ItemMgr::GetItemById(itemId);
    if (movedItem) {
        IntReport("  After move: bagIndex=%u packetBagId=%u slot=%u",
                  movedItem->bag ? movedItem->bag->index : 0,
                  movedItem->bag ? movedItem->bag->h0008 : 0,
                  movedItem->slot);
        Bag* dstBag = inv->bags[dstBagIdx];
        Item* dstSlotItem = (dstBag && dstBag->items.buffer && freeSlot < dstBag->items.size)
            ? dstBag->items.buffer[freeSlot]
            : nullptr;
        Item* srcSlotItem = (srcBagPtr && srcBagPtr->items.buffer && srcSlot < srcBagPtr->items.size)
            ? srcBagPtr->items.buffer[srcSlot]
            : nullptr;
        const uint32_t itemsAfterMove = CountBackpackItems();
        bool moved = (movedItem->bag == targetBagPtr &&
                      movedItem->slot == static_cast<uint8_t>(freeSlot) &&
                      dstSlotItem && dstSlotItem->item_id == itemId &&
                      (!srcSlotItem || srcSlotItem->item_id != itemId) &&
                      itemsAfterMove == itemsBefore);
        IntCheck("Item moved to target slot", moved);
    } else {
        IntCheck("Item still exists after move", false);
    }

    // Move back to original position
    if (srcInvBagSlot == UINT32_MAX) {
        IntCheck("Original inventory bag slot resolved", false);
        IntReport("");
        return true;
    }

    ItemMgr::MoveItem(itemId, srcInvBagSlot, srcSlot);
    Sleep(500 + ChatMgr::GetPing());
    Item* restoredItem = ItemMgr::GetItemById(itemId);
    if (restoredItem) {
        IntReport("  After restore: bagIndex=%u packetBagId=%u slot=%u",
                  restoredItem->bag ? restoredItem->bag->index : 0,
                  restoredItem->bag ? restoredItem->bag->h0008 : 0,
                  restoredItem->slot);
    }
    bool restored = restoredItem && restoredItem->bag == srcBagPtr &&
                    restoredItem->slot == static_cast<uint8_t>(srcSlot) &&
                    CountBackpackItems() == itemsBefore;
    IntCheck("Item restored to original slot", restored);

    IntReport("");
    return true;
}
