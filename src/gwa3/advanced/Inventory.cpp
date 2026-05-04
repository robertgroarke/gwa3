#include <gwa3/advanced/Inventory.h>

#include <gwa3/core/Log.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>

#include <Windows.h>

namespace GWA3::AdvancedInventory {

uint16_t GetItemRarity(const Item* item) {
    if (!item) return 0u;
    const wchar_t* raritySource = item->complete_name_enc ? item->complete_name_enc : item->name_enc;
    if (!raritySource) return 0u;
    __try {
        return *reinterpret_cast<const uint16_t*>(raritySource);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0u;
    }
}

bool IsIdentified(const Item* item) {
    return item && (item->interaction & 0x1u) != 0u;
}

uint32_t CountFreeSlots(uint32_t firstBag, uint32_t lastBag) {
    if (firstBag > lastBag) return 0u;
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return 0u;

    uint32_t freeSlots = 0u;
    for (uint32_t bagIdx = firstBag; bagIdx <= lastBag; ++bagIdx) {
        auto* bag = ItemMgr::GetBag(bagIdx);
        if (!bag) continue;
        freeSlots += (bag->items.size > bag->items_count) ? (bag->items.size - bag->items_count) : 0u;
    }
    return freeSlots;
}

uint32_t CountItemByModel(uint32_t modelId, uint32_t firstBag, uint32_t lastBag) {
    if (firstBag > lastBag) return 0u;
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return 0u;

    uint32_t total = 0u;
    for (uint32_t bagIdx = firstBag; bagIdx <= lastBag; ++bagIdx) {
        auto* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (item && item->model_id == modelId) {
                total += item->quantity;
            }
        }
    }
    return total;
}

Item* FindItemByModel(uint32_t modelId, uint32_t firstBag, uint32_t lastBag) {
    if (firstBag > lastBag) return nullptr;
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;

    for (uint32_t bagIdx = firstBag; bagIdx <= lastBag; ++bagIdx) {
        auto* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (item && item->model_id == modelId) {
                return item;
            }
        }
    }
    return nullptr;
}

namespace {

const char* Prefix(const char* prefix) {
    return prefix ? prefix : "DungeonInventory";
}

void CallWait(WaitFn wait_fn, uint32_t ms) {
    if (wait_fn) {
        wait_fn(ms);
        return;
    }
    Sleep(ms);
}

bool IsWeaponType(uint8_t type) {
    switch (type) {
    case 2u:  // axe
    case 5u:  // bow
    case 12u: // offhand
    case 15u: // hammer
    case 22u: // wand
    case 24u: // shield
    case 26u: // staff
    case 27u: // sword
    case 32u: // dagger
    case 35u: // scythe
    case 36u: // spear
        return true;
    default:
        return false;
    }
}

bool IsSalvageKitModel(uint32_t modelId) {
    switch (modelId) {
    case ItemModelIds::SALVAGE_KIT:
    case ItemModelIds::ALT_SALVAGE_KIT:
    case ItemModelIds::RARE_SALVAGE_KIT:
    case ItemModelIds::EXPERT_SALVAGE_KIT:
    case ItemModelIds::SUPERIOR_SALVAGE_KIT:
        return true;
    default:
        return false;
    }
}

int SalvageKitDropPriority(uint32_t modelId) {
    switch (modelId) {
    case ItemModelIds::SALVAGE_KIT:
        return 0;
    case ItemModelIds::ALT_SALVAGE_KIT:
        return 1;
    case ItemModelIds::RARE_SALVAGE_KIT:
        return 2;
    case ItemModelIds::EXPERT_SALVAGE_KIT:
        return 3;
    case ItemModelIds::SUPERIOR_SALVAGE_KIT:
        return 4;
    default:
        return 999;
    }
}

bool IsDroppableInventoryItem(const Item* item) {
    return item != nullptr &&
           item->item_id != 0u &&
           item->model_id != 0u &&
           item->bag != nullptr &&
           !item->equipped &&
           item->customized == nullptr;
}

bool IsEmergencyWhiteWeapon(const Item* item) {
    return IsDroppableInventoryItem(item) &&
           IsWeaponType(item->type) &&
           GetItemRarity(item) == RARITY_WHITE &&
           !MaintenanceMgr::IsRareSkin(item->model_id);
}

bool IsEmergencySalvageKit(const Item* item) {
    return IsDroppableInventoryItem(item) && IsSalvageKitModel(item->model_id);
}

Item* FindFirstEmergencyWhiteWeapon(uint32_t firstBag, uint32_t lastBag) {
    if (firstBag > lastBag) return nullptr;
    for (uint32_t bagIdx = firstBag; bagIdx <= lastBag; ++bagIdx) {
        auto* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (IsEmergencyWhiteWeapon(item)) {
                return item;
            }
        }
    }
    return nullptr;
}

Item* FindLowestPriorityEmergencySalvageKit(uint32_t firstBag, uint32_t lastBag) {
    if (firstBag > lastBag) return nullptr;
    Item* best = nullptr;
    int bestPriority = 999;
    for (uint32_t bagIdx = firstBag; bagIdx <= lastBag; ++bagIdx) {
        auto* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (!IsEmergencySalvageKit(item)) continue;
            const int priority = SalvageKitDropPriority(item->model_id);
            if (priority < bestPriority) {
                best = item;
                bestPriority = priority;
            }
        }
    }
    return best;
}

} // namespace

UnclaimedItemClaimResult ClaimUnclaimedItemsByModel(const UnclaimedItemClaimOptions& options) {
    UnclaimedItemClaimResult result;
    if (!options.model_ids || options.model_id_count == 0u) {
        return result;
    }

    for (std::size_t i = 0u; i < options.model_id_count; ++i) {
        const uint32_t modelId = options.model_ids[i];
        if (modelId == 0u) continue;
        result.matching_quantity += CountItemByModel(
            modelId,
            options.unclaimed_bag_index,
            options.unclaimed_bag_index);
    }

    if (result.matching_quantity == 0u) {
        return result;
    }

    Log::Info("%s: accepting %u configured unclaimed item(s) from bag %u",
              Prefix(options.log_prefix),
              result.matching_quantity,
              options.unclaimed_bag_index);
    ItemMgr::AcceptAllUnclaimedItems(options.unclaimed_bag_index);
    result.accepted = true;
    CallWait(options.wait_ms, options.post_accept_wait_ms);
    return result;
}

EmergencyFreeSlotDropResult DropEmergencyInventoryItemForFreeSlot(
    const EmergencyFreeSlotOptions& options) {
    EmergencyFreeSlotDropResult result;
    const char* prefix = Prefix(options.log_prefix);

    if (CountFreeSlots(options.first_bag, options.last_bag) > 0u) {
        return result;
    }

    Item* drop = FindFirstEmergencyWhiteWeapon(options.first_bag, options.last_bag);
    result.reason = "white weapon";
    if (drop == nullptr) {
        drop = FindLowestPriorityEmergencySalvageKit(options.first_bag, options.last_bag);
        result.reason = "salvage kit";
    }

    if (drop == nullptr) {
        Log::Warn("%s: no safe emergency inventory item found to drop for free slot", prefix);
        return result;
    }

    result.item_id = drop->item_id;
    result.model_id = drop->model_id;
    result.item_type = drop->type;
    result.rarity = GetItemRarity(drop);
    Log::Warn("%s: dropping emergency %s for free slot item=%u model=%u type=%u rarity=%u",
              prefix,
              result.reason,
              result.item_id,
              result.model_id,
              result.item_type,
              result.rarity);
    ItemMgr::DropItem(result.item_id);
    result.dropped = true;
    CallWait(options.wait_ms, options.post_drop_wait_ms);
    return result;
}

} // namespace GWA3::AdvancedInventory
