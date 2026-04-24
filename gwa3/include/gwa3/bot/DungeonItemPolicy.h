#pragma once

#include <cstdint>

namespace GWA3 {
struct Item;
}

namespace GWA3::Bot::DungeonItemPolicy {

inline constexpr uint8_t ITEM_TYPE_MATERIAL = 11u;
inline constexpr uint8_t ITEM_TYPE_USABLE = 9u;
inline constexpr uint8_t ITEM_TYPE_KEY = 18u;
inline constexpr uint8_t ITEM_TYPE_KIT = 29u;

inline constexpr uint32_t MODEL_ID_KIT = 2992u;
inline constexpr uint32_t MODEL_SUP_ID_KIT = 5899u;
inline constexpr uint32_t MODEL_SALV_KIT = 2993u;
inline constexpr uint32_t MODEL_EXP_SALV_KIT = 2991u;
inline constexpr uint32_t MODEL_ARMOR_SALV = 5900u;
inline constexpr uint32_t MODEL_ESSENCE_CEL = 5901u;
inline constexpr uint32_t MODEL_GRAIL_MIGHT = 5902u;
inline constexpr uint32_t MODEL_BIRTHDAY_CUPCAKE = 22269u;
inline constexpr uint32_t MODEL_SLICE_BIRTHDAY = 28436u;
inline constexpr uint32_t MODEL_CANDY_CORN = 28431u;

bool ShouldSalvageItem(const Item* item);
bool ShouldSellItem(const Item* item);
bool ShouldStoreItem(const Item* item);

} // namespace GWA3::Bot::DungeonItemPolicy
