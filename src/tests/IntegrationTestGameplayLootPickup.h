bool TestLootPickup() {
    IntReport("===  Loot Pickup ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        IntSkip("Loot pickup", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area || !IsSkillCastMapType(area->type)) {
        IntSkip("Loot pickup", "Current instance type is not explorable-like");
        IntReport("");
        return false;
    }

    if (!WaitForStablePlayerState()) {
        IntReport("  Player runtime state: TypeMap=0x%X ModelState=%u", GetPlayerTypeMap(), GetPlayerModelState());
        IntSkip("Loot pickup", "Player state not stable enough for loot scan");
        IntReport("");
        return false;
    }

    AgentItem* item = FindNearbyGroundItem(5000.0f);
    if (!item) {
        const bool createdOpportunity = TryForceNearbyLootDrop();
        if (!createdOpportunity) {
            IntSkip("Loot pickup", "No nearby loot and could not create a local drop opportunity");
            IntReport("");
            return false;
        }
        IntCheck("Nearby loot opportunity created", true);
        if (createdOpportunity) {
            item = FindNearbyGroundItem(5000.0f);
        }
    }
    if (!item) {
        IntSkip("Loot pickup", "No nearby ground item found");
        IntReport("");
        return false;
    }

    const uint32_t itemAgentId = item->agent_id;
    const uint32_t itemId = item->item_id;
    const float itemX = item->x;
    const float itemY = item->y;
    const InventorySnapshot inventoryBefore = CaptureInventorySnapshot();
    float myX = 0.0f;
    float myY = 0.0f;
    const bool havePlayerPos = TryReadAgentPosition(ReadMyId(), myX, myY);
    const float itemDistance = havePlayerPos ? AgentMgr::GetDistance(myX, myY, itemX, itemY) : -1.0f;

    IntReport("  Picking up item agent=%u item=%u at (%.0f, %.0f), dist=%.0f, gold=%u/%u inventoryCount=%u itemIdSum=%llu modelIdSum=%llu quantitySum=%llu",
              itemAgentId,
              itemId,
              itemX,
              itemY,
              itemDistance,
              inventoryBefore.goldCharacter,
              inventoryBefore.goldStorage,
              inventoryBefore.count,
              inventoryBefore.itemIdSum,
              inventoryBefore.modelIdSum,
              inventoryBefore.quantitySum);

    if (itemDistance < 0.0f || itemDistance > 180.0f) {
        IntReport("  Moving closer to loot before pickup...");
        MovePlayerNear(itemX, itemY, 120.0f, 12000);

        float pickupX = 0.0f;
        float pickupY = 0.0f;
        if (TryReadAgentPosition(ReadMyId(), pickupX, pickupY)) {
            IntReport("  Post-move loot distance: %.0f", AgentMgr::GetDistance(pickupX, pickupY, itemX, itemY));
        }
    }

    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (int b = 0; b < 5; ++b) {
                Bag* bag = inv->bags[b];
                if (!bag) { IntReport("    Bag[%d]: null", b); continue; }
                IntReport("    Bag[%d]: type=%u index=%u items_count=%u items.buffer=0x%08X items.size=%u",
                          b, bag->bag_type, bag->index, bag->items_count,
                          static_cast<unsigned>(reinterpret_cast<uintptr_t>(bag->items.buffer)),
                          bag->items.size);
            }
        } else {
            IntReport("    GetInventory() returned null");
        }
    }

    const DWORD pickupStart = GetTickCount();
    bool pickupDone = false;
    while ((GetTickCount() - pickupStart) < 10000 && !pickupDone) {
        GameThread::EnqueuePost([itemX, itemY, itemAgentId]() {
            AgentMgr::Move(itemX, itemY);
            ItemMgr::PickUpItem(itemAgentId);
        });
        Sleep(500);
        if (!FindGroundItemByAgentId(itemAgentId)) {
            pickupDone = true;
            break;
        }
        const InventorySnapshot snap = CaptureInventorySnapshot();
        if (InventoryChangedMeaningfully(inventoryBefore, snap)) {
            pickupDone = true;
            break;
        }
    }
    IntReport("  Pickup loop finished: done=%d elapsed=%ums", pickupDone, GetTickCount() - pickupStart);

    Sleep(1000);

    const bool pickedUpIntoInventory = pickupDone || WaitFor("pickup acknowledged by inventory or item removal", 5000, [itemAgentId, itemId, inventoryBefore]() {
        if (!FindGroundItemByAgentId(itemAgentId)) return true;
        if (ItemMgr::GetItemById(itemId)) return true;
        const InventorySnapshot inventoryAfter = CaptureInventorySnapshot();
        return InventoryChangedMeaningfully(inventoryBefore, inventoryAfter);
    });

    const InventorySnapshot inventoryAfter = CaptureInventorySnapshot();
    Item* pickedItem = ItemMgr::GetItemById(itemId);
    AgentItem* remainingGroundItem = FindGroundItemByAgentId(itemAgentId);
    IntReport("  After pickup: gold=%u/%u inventoryCount=%u itemIdSum=%llu modelIdSum=%llu quantitySum=%llu pickedItem=%p groundItem=%p",
              inventoryAfter.goldCharacter,
              inventoryAfter.goldStorage,
              inventoryAfter.count,
              inventoryAfter.itemIdSum,
              inventoryAfter.modelIdSum,
              inventoryAfter.quantitySum,
              pickedItem,
              remainingGroundItem);
    if (pickedItem) {
        IntReport("  Picked item in inventory: item_id=%u model_id=%u quantity=%u bag=%p slot=%u",
                  pickedItem->item_id,
                  pickedItem->model_id,
                  pickedItem->quantity,
                  pickedItem->bag,
                  pickedItem->slot);
    }

    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (int b = 0; b < 5; ++b) {
                Bag* bag = inv->bags[b];
                if (!bag) continue;
                IntReport("    After Bag[%d]: type=%u items_count=%u items.size=%u",
                          b, bag->bag_type, bag->items_count, bag->items.size);
            }
        }
    }

    const bool inventoryChanged = InventoryChangedMeaningfully(inventoryBefore, inventoryAfter);
    const bool groundItemGone = (remainingGroundItem == nullptr);
    IntReport("  groundItemGone=%d inventoryChanged=%d pickedItem=%p",
              groundItemGone, inventoryChanged, pickedItem);
    IntCheck("Loot pickup changes inventory state", inventoryChanged || pickedItem != nullptr);

    IntReport("");
    return pickedUpIntoInventory;
}
