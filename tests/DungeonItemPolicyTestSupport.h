#pragma once

#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/dungeon/DungeonItemPolicy.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::Tests::DungeonItemPolicy {

inline GWA3::Item MakeItem(uint32_t itemId, uint32_t modelId, uint8_t type, uint16_t rarity,
                           uint32_t interaction = 0u, uint16_t value = 0u) {
    static wchar_t names[16][2] = {};
    static std::size_t nextName = 0u;

    GWA3::Item item = {};
    item.item_id = itemId;
    item.model_id = modelId;
    item.type = type;
    item.interaction = interaction;
    item.value = value;

    auto& name = names[nextName++ % 16u];
    name[0] = static_cast<wchar_t>(rarity);
    name[1] = 0;
    item.name_enc = name;
    return item;
}

} // namespace GWA3::Tests::DungeonItemPolicy
