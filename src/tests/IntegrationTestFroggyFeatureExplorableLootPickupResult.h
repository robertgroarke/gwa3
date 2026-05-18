static bool ValidateExplorableLootPickupResult(const ExplorableLootCandidate& candidate,
                                               const InventorySnapshot& inventoryBefore,
                                               bool pickedUpIntoInventory) {
    const InventorySnapshot inventoryAfter = CaptureInventorySnapshot();
    Item* pickedItem = ItemMgr::GetItemById(candidate.itemId);
    AgentItem* remainingGroundItem = FindGroundItemByAgentId(candidate.agentId);
    const bool inventoryChanged = InventoryChangedMeaningfully(inventoryBefore, inventoryAfter);

    FroggyFeatureReport("  Loot result: acknowledged=%d inventoryChanged=%d pickedItem=%p groundItem=%p count=%u->%u gold=%u/%u->%u/%u",
              pickedUpIntoInventory ? 1 : 0,
              inventoryChanged ? 1 : 0,
              pickedItem,
              remainingGroundItem,
              inventoryBefore.count, inventoryAfter.count,
              inventoryBefore.goldCharacter, inventoryBefore.goldStorage,
              inventoryAfter.goldCharacter, inventoryAfter.goldStorage);

    FroggyFeatureCheck("Loot pickup changes inventory state or inventory contains the picked item",
             inventoryChanged || pickedItem != nullptr);
    FroggyFeatureCheck("Loot pickup removes the ground item or acknowledges inventory change",
             remainingGroundItem == nullptr || pickedUpIntoInventory);
    return pickedUpIntoInventory;
}
