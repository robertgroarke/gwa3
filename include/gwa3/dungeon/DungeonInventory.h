#pragma once

#include <cstdint>

namespace GWA3 {
struct Item;
}

namespace GWA3::DungeonInventory {

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

} // namespace GWA3::DungeonInventory
