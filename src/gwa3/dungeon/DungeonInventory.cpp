#include <gwa3/dungeon/DungeonInventory.h>

#include <gwa3/managers/ItemMgr.h>

#include <Windows.h>

namespace GWA3::DungeonInventory {

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

} // namespace GWA3::DungeonInventory
