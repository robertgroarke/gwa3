static bool AttemptExplorableLootPickup(const ExplorableLootCandidate& candidate, const InventorySnapshot& inventoryBefore) {
    const DWORD pickupStart = GetTickCount();
    bool pickupDone = false;
    while ((GetTickCount() - pickupStart) < 10000 && !pickupDone) {
        GameThread::EnqueuePost([candidate]() {
            AgentMgr::Move(candidate.x, candidate.y);
            ItemMgr::PickUpItem(candidate.agentId);
        });
        Sleep(500);
        if (!FindGroundItemByAgentId(candidate.agentId)) {
            pickupDone = true;
            break;
        }
        const InventorySnapshot snap = CaptureInventorySnapshot();
        if (InventoryChangedMeaningfully(inventoryBefore, snap)) {
            pickupDone = true;
            break;
        }
    }

    bool pickedUpIntoInventory = pickupDone;
    const DWORD pickupAckStart = GetTickCount();
    while (!pickedUpIntoInventory && (GetTickCount() - pickupAckStart) < 5000) {
        if (!FindGroundItemByAgentId(candidate.agentId) || ItemMgr::GetItemById(candidate.itemId)) {
            pickedUpIntoInventory = true;
            break;
        }
        const InventorySnapshot inventoryAfterWait = CaptureInventorySnapshot();
        if (InventoryChangedMeaningfully(inventoryBefore, inventoryAfterWait)) {
            pickedUpIntoInventory = true;
            break;
        }
        Sleep(250);
    }

    return pickedUpIntoInventory;
}
