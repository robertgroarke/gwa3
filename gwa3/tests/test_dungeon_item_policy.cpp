#include <gwa3/bot/DungeonItemPolicy.h>
#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/game/Item.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot::DungeonItemPolicy;

namespace {

GWA3::Item MakeItem(uint32_t itemId, uint32_t modelId, uint8_t type, uint16_t rarity,
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

} // namespace

GWA3_TEST(dungeon_item_policy_store_keeps_rare_materials_and_greens, {
    auto ecto = MakeItem(1u, 930u, ITEM_TYPE_MATERIAL, GWA3::Bot::DungeonInventory::RARITY_GOLD);
    auto green = MakeItem(2u, 555u, 0u, GWA3::Bot::DungeonInventory::RARITY_GREEN);
    auto junk = MakeItem(3u, 12345u, 0u, GWA3::Bot::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(ShouldStoreItem(&ecto));
    GWA3_ASSERT(ShouldStoreItem(&green));
    GWA3_ASSERT(!ShouldStoreItem(&junk));
})

GWA3_TEST(dungeon_item_policy_salvage_rejects_protected_and_non_junk_items, {
    auto whiteWeapon = MakeItem(4u, 15055u, 27u, GWA3::Bot::DungeonInventory::RARITY_WHITE);
    auto blueWeapon = MakeItem(5u, 15056u, 27u, GWA3::Bot::DungeonInventory::RARITY_BLUE);
    auto material = MakeItem(6u, 948u, ITEM_TYPE_MATERIAL, GWA3::Bot::DungeonInventory::RARITY_WHITE);
    auto salvageKit = MakeItem(7u, MODEL_SALV_KIT, ITEM_TYPE_KIT, GWA3::Bot::DungeonInventory::RARITY_WHITE);
    auto goldWeapon = MakeItem(8u, 15057u, 27u, GWA3::Bot::DungeonInventory::RARITY_GOLD);

    GWA3_ASSERT(ShouldSalvageItem(&whiteWeapon));
    GWA3_ASSERT(ShouldSalvageItem(&blueWeapon));
    GWA3_ASSERT(!ShouldSalvageItem(&material));
    GWA3_ASSERT(!ShouldSalvageItem(&salvageKit));
    GWA3_ASSERT(!ShouldSalvageItem(&goldWeapon));
})

GWA3_TEST(dungeon_item_policy_sell_respects_identification_and_protected_models, {
    auto whiteWeapon = MakeItem(9u, 16001u, 27u, GWA3::Bot::DungeonInventory::RARITY_WHITE);
    auto blueUnid = MakeItem(10u, 16002u, 27u, GWA3::Bot::DungeonInventory::RARITY_BLUE, 0u, 80u);
    auto blueId = MakeItem(11u, 16003u, 27u, GWA3::Bot::DungeonInventory::RARITY_BLUE, 0x1u, 80u);
    auto purpleUnid = MakeItem(12u, 16004u, 27u, GWA3::Bot::DungeonInventory::RARITY_PURPLE, 0u, 10u);
    auto goldIdLowValue = MakeItem(13u, 16005u, 27u, GWA3::Bot::DungeonInventory::RARITY_GOLD, 0x1u, 50u);
    auto protectedModel = MakeItem(14u, MODEL_ARMOR_SALV, ITEM_TYPE_USABLE, GWA3::Bot::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(ShouldSellItem(&whiteWeapon));
    GWA3_ASSERT(!ShouldSellItem(&blueUnid));
    GWA3_ASSERT(ShouldSellItem(&blueId));
    GWA3_ASSERT(!ShouldSellItem(&purpleUnid));
    GWA3_ASSERT(ShouldSellItem(&goldIdLowValue));
    GWA3_ASSERT(!ShouldSellItem(&protectedModel));
})
