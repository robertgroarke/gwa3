#include <gwa3/advanced/ItemPolicy.h>

#include <gwa3/advanced/Inventory.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/ItemModelIds.h>

namespace GWA3::AdvancedItemPolicy {

namespace {

bool IsProtectedModel(uint32_t modelId) {
    switch (modelId) {
    case GWA3::ItemModelIds::IDENTIFICATION_KIT:
    case GWA3::ItemModelIds::SUPERIOR_IDENTIFICATION_KIT:
    case GWA3::ItemModelIds::SALVAGE_KIT:
    case GWA3::ItemModelIds::SUPERIOR_SALVAGE_KIT:
    case GWA3::ItemModelIds::EXPERT_SALVAGE_KIT:
    case GWA3::ItemModelIds::RARE_SALVAGE_KIT:
    case GWA3::ItemModelIds::ARMOR_OF_SALVATION:
    case GWA3::ItemModelIds::ESSENCE_OF_CELERITY:
    case GWA3::ItemModelIds::GRAIL_OF_MIGHT:
    case GWA3::ItemModelIds::BIRTHDAY_CUPCAKE:
    case GWA3::ItemModelIds::SLICE_OF_BIRTHDAY_CAKE:
    case GWA3::ItemModelIds::CANDY_APPLE:
    case GWA3::ItemModelIds::CANDY_CORN:
        return true;
    default:
        return false;
    }
}

bool IsStoredValuableModel(uint32_t modelId) {
    switch (modelId) {
    case GWA3::ItemModelIds::GLOB_OF_ECTOPLASM:
    case GWA3::ItemModelIds::DIAMOND:
    case GWA3::ItemModelIds::ONYX_GEMSTONE:
    case GWA3::ItemModelIds::RUBY:
    case GWA3::ItemModelIds::SAPPHIRE:
    case GWA3::ItemModelIds::OBSIDIAN_SHARD:
        return true;
    default:
        return false;
    }
}

} // namespace

bool ShouldSalvageItem(const Item* item) {
    if (!item || item->item_id == 0u || item->model_id == 0u) return false;
    if (item->equipped || item->customized) return false;

    const uint16_t rarity = AdvancedInventory::GetItemRarity(item);
    if (rarity != AdvancedInventory::RARITY_WHITE &&
        rarity != AdvancedInventory::RARITY_BLUE) {
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

    const uint16_t rarity = AdvancedInventory::GetItemRarity(item);
    if (rarity == AdvancedInventory::RARITY_GREEN) return false;
    if ((rarity == AdvancedInventory::RARITY_GOLD ||
         rarity == AdvancedInventory::RARITY_PURPLE) &&
        !AdvancedInventory::IsIdentified(item)) {
        return false;
    }

    if (IsProtectedModel(item->model_id)) return false;
    if (rarity == AdvancedInventory::RARITY_WHITE) return true;
    if (rarity == AdvancedInventory::RARITY_BLUE &&
        AdvancedInventory::IsIdentified(item)) {
        return true;
    }
    if (AdvancedInventory::IsIdentified(item) && item->value < 100u) return true;
    return false;
}

bool ShouldStoreItem(const Item* item) {
    if (!item || item->item_id == 0u) return false;
    if (IsStoredValuableModel(item->model_id)) return true;
    return AdvancedInventory::GetItemRarity(item) == AdvancedInventory::RARITY_GREEN;
}

} // namespace GWA3::AdvancedItemPolicy
