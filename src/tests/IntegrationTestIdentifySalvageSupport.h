namespace {

static constexpr uint16_t kRarityGold = 2624u;

struct IdentifySalvageCandidate {
    uint32_t bagIndex = 0;
    uint32_t slotIndex = 0;
    uint32_t itemId = 0;
    uint32_t modelId = 0;
    uint8_t type = 0;
    uint16_t rarity = 0;
    uint16_t quantity = 0;
    uint16_t requirement = 0;
    uint16_t formula = 0;
    uint8_t salvageable = 0;
    uint32_t interaction = 0;
    Bag* bagPtr = nullptr;
    Item* trackedPtr = nullptr;
};

static uint16_t GetHarnessItemRarity(Item* item) {
    if (!item) return 0;
    wchar_t* nameStr = item->complete_name_enc;
    if (!nameStr) nameStr = item->name_enc;
    return nameStr ? static_cast<uint16_t>(nameStr[0]) : 0u;
}

static bool IsHarnessItemIdentified(Item* item) {
    return item && (item->interaction & 0x1) != 0;
}

static bool IsHarnessWeapon(Item* item) {
    if (!item) return false;
    switch (item->type) {
    case 2:  // axe
    case 5:  // bow
    case 12: // offhand
    case 15: // hammer
    case 22: // wand
    case 24: // shield
    case 26: // staff
    case 27: // sword
    case 32: // dagger
    case 35: // scythe
    case 36: // spear
        return true;
    default:
        return false;
    }
}

static bool IsHarnessKit(uint32_t modelId) {
    return modelId == ItemModelIds::SUPERIOR_SALVAGE_KIT ||
           modelId == ItemModelIds::SALVAGE_KIT ||
           modelId == ItemModelIds::EXPERT_SALVAGE_KIT ||
           modelId == ItemModelIds::RARE_SALVAGE_KIT ||
           modelId == ItemModelIds::SUPERIOR_IDENTIFICATION_KIT ||
           modelId == ItemModelIds::ALT_IDENTIFICATION_KIT ||
           modelId == ItemModelIds::ALT_SALVAGE_KIT ||
           modelId == ItemModelIds::IDENTIFICATION_KIT;
}

static Item* FindHarnessIdKit() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item) continue;
            if (item->model_id == ItemModelIds::SUPERIOR_IDENTIFICATION_KIT ||
                item->model_id == ItemModelIds::IDENTIFICATION_KIT ||
                item->model_id == ItemModelIds::ALT_IDENTIFICATION_KIT) {
                return item;
            }
        }
    }
    return nullptr;
}

static Item* FindHarnessSalvageKit() {
    Inventory* inv = ItemMgr::GetInventory();
    if (inv) {
        static constexpr uint32_t kPreferredSalvageModels[] = {
            ItemModelIds::SALVAGE_KIT,
            ItemModelIds::RARE_SALVAGE_KIT,
            ItemModelIds::ALT_SALVAGE_KIT,
            ItemModelIds::EXPERT_SALVAGE_KIT,
            ItemModelIds::SUPERIOR_SALVAGE_KIT
        };
        for (uint32_t preferredModel : kPreferredSalvageModels) {
            for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
                Bag* bag = inv->bags[bagIdx];
                if (!bag || !bag->items.buffer) continue;
                for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
                    Item* item = bag->items.buffer[slot];
                    if (item && item->model_id == preferredModel) {
                        return item;
                    }
                }
            }
        }
    }
    return nullptr;
}

static uint32_t CountIdentifyCandidates() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id == 0) continue;
            if (IsHarnessItemIdentified(item)) continue;
            if (IsHarnessKit(item->model_id)) continue;
            if (MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            ++count;
        }
    }
    return count;
}

static bool FindGoldSalvageCandidate(IdentifySalvageCandidate& out) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id == 0) continue;
            if (!IsHarnessItemIdentified(item)) continue;
            if (IsHarnessKit(item->model_id) || MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            if (!MaintenanceMgr::ShouldSellItem(item)) continue;
            if (!IsHarnessWeapon(item)) continue;
            if (GetHarnessItemRarity(item) != kRarityGold) continue;
            if (item->is_material_salvageable == 0) continue;

            out.bagIndex = bagIdx;
            out.slotIndex = slot;
            out.itemId = item->item_id;
            out.modelId = item->model_id;
            out.type = item->type;
            out.rarity = GetHarnessItemRarity(item);
            out.quantity = item->quantity;
            out.requirement = item->h0026;
            out.formula = item->item_formula;
            out.salvageable = item->is_material_salvageable;
            out.interaction = item->interaction;
            out.bagPtr = bag;
            out.trackedPtr = item;
            return true;
        }
    }
    return false;
}

static uint32_t CollectGoldSalvageCandidates(IdentifySalvageCandidate* out, uint32_t maxCount) {
    if (!out || maxCount == 0) return 0;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4 && count < maxCount; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size && count < maxCount; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id == 0) continue;
            if (!IsHarnessItemIdentified(item)) continue;
            if (IsHarnessKit(item->model_id) || MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            if (!MaintenanceMgr::ShouldSellItem(item)) continue;
            if (!IsHarnessWeapon(item)) continue;
            if (GetHarnessItemRarity(item) != kRarityGold) continue;
            if (item->is_material_salvageable == 0) continue;

            auto& candidate = out[count++];
            candidate.bagIndex = bagIdx;
            candidate.slotIndex = slot;
            candidate.itemId = item->item_id;
            candidate.modelId = item->model_id;
            candidate.type = item->type;
            candidate.rarity = GetHarnessItemRarity(item);
            candidate.quantity = item->quantity;
            candidate.requirement = item->h0026;
            candidate.formula = item->item_formula;
            candidate.salvageable = item->is_material_salvageable;
            candidate.interaction = item->interaction;
            candidate.bagPtr = bag;
            candidate.trackedPtr = item;
        }
    }
    return count;
}

static Item* ResolveCandidateItemFromCachedBag(const IdentifySalvageCandidate& candidate) {
    if (!candidate.bagPtr || !candidate.bagPtr->items.buffer) return nullptr;
    __try {
        if (candidate.slotIndex >= candidate.bagPtr->items.size) {
            return nullptr;
        }
        return candidate.bagPtr->items.buffer[candidate.slotIndex];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static void ReportIdentifySalvageSummary(const char* label) {
    Item* idKit = FindHarnessIdKit();
    Item* salvageKit = FindHarnessSalvageKit();
    uint32_t cheapSalv = 0;
    uint32_t basicSalv = 0;
    uint32_t expertSalv = 0;
    uint32_t rareSalv = 0;
    uint32_t altSalv = 0;
    if (Inventory* inv = ItemMgr::GetInventory()) {
        for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
            Bag* bag = inv->bags[bagIdx];
            if (!bag || !bag->items.buffer) continue;
            for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
                Item* item = bag->items.buffer[slot];
                if (!item) continue;
                switch (item->model_id) {
                case ItemModelIds::SALVAGE_KIT: ++cheapSalv; break;
                case ItemModelIds::SUPERIOR_SALVAGE_KIT: ++basicSalv; break;
                case ItemModelIds::EXPERT_SALVAGE_KIT: ++expertSalv; break;
                case ItemModelIds::RARE_SALVAGE_KIT: ++rareSalv; break;
                case ItemModelIds::ALT_SALVAGE_KIT: ++altSalv; break;
                default: break;
                }
            }
        }
    }

    IntReport("  %s: map=%u freeSlots=%u charGold=%u storageGold=%u idKit=%u(idModel=%u) salvageKit=%u(sModel=%u) identifyCandidates=%u cheapSalv=%u basicSalv=%u expertSalv=%u rareSalv=%u altSalv=%u",
              label,
              ReadMapId(),
              MaintenanceMgr::CountFreeSlots(),
              ItemMgr::GetGoldCharacter(),
              ItemMgr::GetGoldStorage(),
              idKit ? idKit->item_id : 0u,
              idKit ? idKit->model_id : 0u,
              salvageKit ? salvageKit->item_id : 0u,
              salvageKit ? salvageKit->model_id : 0u,
              CountIdentifyCandidates(),
              cheapSalv,
              basicSalv,
              expertSalv,
              rareSalv,
              altSalv);
}

static void ReportGoldSalvageCandidates(const char* label, uint32_t maxCount = 8u) {
    Inventory* inv = ItemMgr::GetInventory();
    IntReport("  %s:", label);
    if (!inv) {
        IntReport("    inventory unavailable");
        return;
    }

    uint32_t shown = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4 && shown < maxCount; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size && shown < maxCount; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id == 0) continue;
            if (IsHarnessKit(item->model_id) || MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            if (!IsHarnessWeapon(item)) continue;
            if (GetHarnessItemRarity(item) != kRarityGold) continue;
            IntReport("    bag=%u slot=%u item=%u model=%u type=%u qty=%u rarity=%u id=%u salvageable=%u shouldSell=%u req=%u formula=%u",
                      bagIdx,
                      slot,
                      item->item_id,
                      item->model_id,
                      item->type,
                      item->quantity,
                      GetHarnessItemRarity(item),
                      IsHarnessItemIdentified(item) ? 1u : 0u,
                      item->is_material_salvageable,
                      MaintenanceMgr::ShouldSellItem(item) ? 1u : 0u,
                      item->h0026,
                      item->item_formula);
            ++shown;
        }
    }
    if (shown == 0) {
        IntReport("    no gold weapon candidates in bags 1-4");
    }
}

} // namespace
