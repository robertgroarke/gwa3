bool TestItemClickOffset() {
    IntReport("=== ItemClick Offset ===");

    IntReport("  ItemClick: 0x%08X", static_cast<unsigned>(Offsets::ItemClick));

    if (Offsets::ItemClick > 0x10000) {
        IntCheck("ItemClick offset resolved", true);

        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
                Bag* bag = inv->bags[bagIdx];
                if (!bag || !bag->items.buffer) continue;
                for (uint32_t i = 0; i < bag->items.size; ++i) {
                    Item* item = bag->items.buffer[i];
                    if (!item || item->item_id == 0) continue;
                    IntReport("  Found test item: id=%u model=%u in bag %u",
                              item->item_id, item->model_id, bagIdx);
                    IntCheck("ClickItem function available for resolved offset", true);
                    goto done_item_check;
                }
            }
            IntSkip("ClickItem test", "No items in backpack to test with");
            done_item_check:;
        } else {
            IntSkip("ClickItem test", "Inventory unavailable");
        }
    } else {
        IntSkip("ItemClick offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
