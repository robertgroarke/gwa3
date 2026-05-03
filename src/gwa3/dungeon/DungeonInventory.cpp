#include <gwa3/dungeon/DungeonInventory.h>

#include <gwa3/core/Log.h>
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

} // namespace GWA3::DungeonInventory
