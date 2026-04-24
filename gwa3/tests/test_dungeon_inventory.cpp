#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/game/Item.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
void ClearBagItem(uint32_t bagIndex, uint32_t slotIndex);

} // namespace GWA3::TestStubs::ItemMgr

using namespace GWA3::Bot::DungeonInventory;

GWA3_TEST(dungeon_inventory_counts_free_slots_and_items_by_model, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 5u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 10u, 930u, 11u, 3u, 0u, 0u, RARITY_GOLD);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 11u, 930u, 11u, 2u, 0u, 0u, RARITY_GOLD);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 4u, 12u, 945u, 11u, 1u, 0u, 0u, RARITY_PURPLE);

    GWA3_ASSERT_EQ(CountFreeSlots(), 2u);
    GWA3_ASSERT_EQ(CountItemByModel(930u), 5u);
    GWA3_ASSERT_EQ(CountItemByModel(945u), 1u);
    GWA3_ASSERT_EQ(CountItemByModel(999u), 0u);
})

GWA3_TEST(dungeon_inventory_finds_item_by_model_and_reads_rarity_and_identification, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 20u, 2992u, 29u, 1u, 100u, 0x1u, RARITY_BLUE);

    auto* item = FindItemByModel(2992u);
    GWA3_ASSERT(item != nullptr);
    GWA3_ASSERT_EQ(item->item_id, 20u);
    GWA3_ASSERT_EQ(GetItemRarity(item), RARITY_BLUE);
    GWA3_ASSERT(IsIdentified(item));

    GWA3::TestStubs::ItemMgr::ClearBagItem(1u, 0u);
    GWA3_ASSERT(FindItemByModel(2992u) == nullptr);
})
