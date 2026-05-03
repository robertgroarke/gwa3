#pragma once

#include <cstddef>
#include <cstdint>

namespace GWA3 {
struct Item;
}

namespace GWA3::AdvancedInventory {

inline constexpr uint16_t RARITY_WHITE = 2621u;
inline constexpr uint16_t RARITY_BLUE = 2623u;
inline constexpr uint16_t RARITY_GOLD = 2624u;
inline constexpr uint16_t RARITY_PURPLE = 2626u;
inline constexpr uint16_t RARITY_GREEN = 2627u;

uint16_t GetItemRarity(const Item* item);
bool IsIdentified(const Item* item);
uint32_t CountFreeSlots(uint32_t firstBag = 1u, uint32_t lastBag = 4u);
uint32_t CountItemByModel(uint32_t modelId, uint32_t firstBag = 1u, uint32_t lastBag = 4u);
Item* FindItemByModel(uint32_t modelId, uint32_t firstBag = 1u, uint32_t lastBag = 4u);

using WaitFn = void(*)(uint32_t ms);

struct UnclaimedItemClaimOptions {
    const uint32_t* model_ids = nullptr;
    std::size_t model_id_count = 0u;
    uint32_t unclaimed_bag_index = 7u;
    uint32_t post_accept_wait_ms = 500u;
    const char* log_prefix = nullptr;
    WaitFn wait_ms = nullptr;
};

struct UnclaimedItemClaimResult {
    uint32_t matching_quantity = 0u;
    bool accepted = false;
};

UnclaimedItemClaimResult ClaimUnclaimedItemsByModel(const UnclaimedItemClaimOptions& options);

} // namespace GWA3::AdvancedInventory
