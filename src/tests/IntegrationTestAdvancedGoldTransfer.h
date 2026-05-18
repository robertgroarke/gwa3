bool TestGoldTransfer() {
    IntReport("=== Gold Transfer ===");

    if (ReadMyId() == 0) { IntSkip("GoldTransfer", "Not in game"); IntReport(""); return false; }

    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t storeGold = ItemMgr::GetGoldStorage();
    IntReport("  Before: char=%u storage=%u", charGold, storeGold);

    if (charGold < 100) {
        IntSkip("GoldTransfer", "Not enough character gold (< 100)");
        IntReport("");
        return true;
    }

    // Transfer 100 gold to storage
    ItemMgr::ChangeGold(charGold - 100, storeGold + 100);
    Sleep(500);

    uint32_t charAfter = ItemMgr::GetGoldCharacter();
    uint32_t storeAfter = ItemMgr::GetGoldStorage();
    IntReport("  After transfer: char=%u storage=%u", charAfter, storeAfter);
    IntCheck("Character gold decreased by exactly 100", charAfter + 100 == charGold);
    IntCheck("Storage gold increased by exactly 100", storeAfter == storeGold + 100);

    // Transfer back
    ItemMgr::ChangeGold(charAfter + 100, storeAfter - 100);
    Sleep(500);

    uint32_t charRestored = ItemMgr::GetGoldCharacter();
    uint32_t storeRestored = ItemMgr::GetGoldStorage();
    IntReport("  After restore: char=%u storage=%u", charRestored, storeRestored);
    IntCheck("Character gold restored to original value", charRestored == charGold);
    IntCheck("Storage gold restored to original value", storeRestored == storeGold);

    IntReport("");
    return true;
}
