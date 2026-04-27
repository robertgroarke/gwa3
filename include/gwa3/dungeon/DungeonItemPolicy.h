#pragma once

#include <cstdint>

namespace GWA3 {
struct Item;
}

namespace GWA3::DungeonItemPolicy {

inline constexpr uint8_t ITEM_TYPE_MATERIAL = 11u;
inline constexpr uint8_t ITEM_TYPE_USABLE = 9u;
inline constexpr uint8_t ITEM_TYPE_KEY = 18u;
inline constexpr uint8_t ITEM_TYPE_KIT = 29u;

bool ShouldSalvageItem(const Item* item);
bool ShouldSellItem(const Item* item);
bool ShouldStoreItem(const Item* item);

} // namespace GWA3::DungeonItemPolicy
