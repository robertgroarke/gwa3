#include <gwa3/bot/DungeonItemPolicy.h>

#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/game/Item.h>

namespace GWA3::Bot::DungeonItemPolicy {

namespace {

bool IsProtectedModel(uint32_t modelId) {
    switch (modelId) {
    case MODEL_ID_KIT:
    case MODEL_SUP_ID_KIT:
    case MODEL_SALV_KIT:
    case MODEL_EXP_SALV_KIT:
    case MODEL_ARMOR_SALV:
    case MODEL_ESSENCE_CEL:
    case MODEL_GRAIL_MIGHT:
    case MODEL_BIRTHDAY_CUPCAKE:
    case MODEL_SLICE_BIRTHDAY:
    case MODEL_CANDY_CORN:
        return true;
    default:
        return false;
    }
}

bool IsStoredValuableModel(uint32_t modelId) {
    switch (modelId) {
    case 930u:
    case 935u:
    case 936u:
    case 937u:
    case 938u:
    case 945u:
        return true;
    default:
        return false;
    }
}

} // namespace

bool ShouldSalvageItem(const Item* item) {
    if (!item || item->item_id == 0u || item->model_id == 0u) return false;
    if (item->equipped || item->customized) return false;

    const uint16_t rarity = DungeonInventory::GetItemRarity(item);
    if (rarity != DungeonInventory::RARITY_WHITE &&
        rarity != DungeonInventory::RARITY_BLUE) {
        return false;
    }

    if (IsProtectedModel(item->model_id)) return false;
    if (item->type == ITEM_TYPE_MATERIAL) return false;
    if (item->type == ITEM_TYPE_KEY) return false;
    if (item->type == ITEM_TYPE_USABLE) return false;
    if (item->type == ITEM_TYPE_KIT) return false;
    return true;
}

bool ShouldSellItem(const Item* item) {
    if (!item || item->item_id == 0u || item->model_id == 0u) return false;
    if (item->equipped || item->customized) return false;

    const uint16_t rarity = DungeonInventory::GetItemRarity(item);
    if (rarity == DungeonInventory::RARITY_GREEN) return false;
    if ((rarity == DungeonInventory::RARITY_GOLD ||
         rarity == DungeonInventory::RARITY_PURPLE) &&
        !DungeonInventory::IsIdentified(item)) {
        return false;
    }

    if (IsProtectedModel(item->model_id)) return false;
    if (rarity == DungeonInventory::RARITY_WHITE) return true;
    if (rarity == DungeonInventory::RARITY_BLUE &&
        DungeonInventory::IsIdentified(item)) {
        return true;
    }
    if (DungeonInventory::IsIdentified(item) && item->value < 100u) return true;
    return false;
}

bool ShouldStoreItem(const Item* item) {
    if (!item || item->item_id == 0u) return false;
    if (IsStoredValuableModel(item->model_id)) return true;
    return DungeonInventory::GetItemRarity(item) == DungeonInventory::RARITY_GREEN;
}

} // namespace GWA3::Bot::DungeonItemPolicy
