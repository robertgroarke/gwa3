struct ConsumableMaterialCounter {
    const char* label;
    uint32_t modelId;
    uint32_t bags14 = 0;
    uint32_t storage = 0;
};

struct ConsumableRecipeMaterial {
    uint32_t modelId;
    uint32_t quantity;
};

struct ConsumableCraftRecipe {
    uint32_t fee = 0;
    uint32_t materialCount = 0;
    ConsumableRecipeMaterial materials[2]{};
};

uint32_t CountBagModelQuantity(uint32_t modelId, uint32_t bagStart, uint32_t bagEnd) {
    uint32_t total = 0;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    for (uint32_t bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
        Bag* bag = inv->bags[bagIndex];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (item && item->model_id == modelId) {
                total += item->quantity;
            }
        }
    }
    return total;
}

void FillConsumableMaterialCounters(ConsumableMaterialCounter (&counters)[7]) {
    counters[0] = {"iron", kMaterialIronIngot};
    counters[1] = {"dust", kMaterialDust};
    counters[2] = {"bone", kMaterialBone};
    counters[3] = {"feather", kMaterialFeather};
    counters[4] = {"granite", kMaterialGraniteSlab};
    counters[5] = {"fiber", kMaterialPlantFiber};
    counters[6] = {"scale", kMaterialScale};

    for (auto& counter : counters) {
        counter.bags14 = CountBagModelQuantity(counter.modelId, 1u, 4u);
        counter.storage = CountBagModelQuantity(counter.modelId, 6u, 6u);
    }
}

void FormatConsumableMaterialSnapshot(char* out, size_t outSize,
                                      const ConsumableMaterialCounter (&before)[7],
                                      const ConsumableMaterialCounter (&after)[7]) {
    if (!out || !outSize) return;
    const uint32_t goldChar = ItemMgr::GetGoldCharacter();
    const uint32_t goldStorage = ItemMgr::GetGoldStorage();
    sprintf_s(
        out, outSize,
        "goldChar=%u goldStorage=%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u",
        goldChar, goldStorage,
        before[0].label, before[0].bags14, before[0].storage, after[0].bags14, after[0].storage,
        before[1].label, before[1].bags14, before[1].storage, after[1].bags14, after[1].storage,
        before[2].label, before[2].bags14, before[2].storage, after[2].bags14, after[2].storage,
        before[3].label, before[3].bags14, before[3].storage, after[3].bags14, after[3].storage,
        before[4].label, before[4].bags14, before[4].storage, after[4].bags14, after[4].storage,
        before[5].label, before[5].bags14, before[5].storage, after[5].bags14, after[5].storage,
        before[6].label, before[6].bags14, before[6].storage, after[6].bags14, after[6].storage);
}
