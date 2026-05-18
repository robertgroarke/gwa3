static void RunBehavioralInventoryTest() {
    CmdReport("--- Test 8: Inventory ---");
    Inventory* inv = ItemMgr::GetInventory();
    if (inv) {
        uint32_t gold = inv->gold_character;
        CmdReport("Character gold: %u", gold);
        CmdCheck("Inventory readable", true);
        CmdCheck("Gold is plausible (< 1M)", gold < 1000000);

        Bag* backpack = ItemMgr::GetBag(1);
        if (backpack) {
            CmdReport("Backpack slots: %u items", backpack->items_count);
            CmdCheck("Backpack accessible", true);
        }
    } else {
        CmdReport("[SKIP] Inventory not accessible");
    }
    CmdReport("");
}
