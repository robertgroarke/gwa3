#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/bot/DungeonItemActions.h>
#include <gwa3/bot/DungeonItemPolicy.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
uint32_t LastUsedItemId();
uint32_t LastIdentifiedItemId();
uint32_t LastIdentifyKitId();
uint32_t IdentifyCount();
uint32_t LastSalvagedItemId();
uint32_t LastSalvageKitId();
uint32_t SalvageOpenCount();
uint32_t SalvageDoneCount();
uint32_t LastMovedItemId();
uint32_t LastMoveBagId();
uint32_t LastMoveSlot();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::TradeMgr {

uint32_t LastTransactType();
uint32_t LastTransactQuantity();
uint32_t LastTransactItemId();
uint32_t TransactCount();

} // namespace GWA3::TestStubs::TradeMgr

namespace GWA3::TestStubs::EffectMgr {

void Reset();
void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration);

} // namespace GWA3::TestStubs::EffectMgr

using namespace GWA3::Bot::DungeonItemActions;

namespace {

void ItemActionsNoWait(uint32_t) {
}

bool ShouldIdentifyGoldOrPurple(const GWA3::Item* item) {
    if (!item || (item->interaction & 0x1u) != 0u) return false;
    const uint16_t rarity = GWA3::Bot::DungeonInventory::GetItemRarity(item);
    return rarity == GWA3::Bot::DungeonInventory::RARITY_GOLD ||
           rarity == GWA3::Bot::DungeonInventory::RARITY_PURPLE;
}

} // namespace

GWA3_TEST(dungeon_item_actions_identify_items_uses_kit_and_marks_targets, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 4u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 100u, GWA3::Bot::DungeonItemPolicy::MODEL_ID_KIT,
                                         29u, 1u, 100u, 0u, GWA3::Bot::DungeonInventory::RARITY_BLUE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 101u, 16001u,
                                         27u, 1u, 100u, 0u, GWA3::Bot::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 102u, 16002u,
                                         27u, 1u, 100u, 0u, GWA3::Bot::DungeonInventory::RARITY_PURPLE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 3u, 103u, 16003u,
                                         27u, 1u, 100u, 0u, GWA3::Bot::DungeonInventory::RARITY_WHITE);

    const int identified = IdentifyItems(&ShouldIdentifyGoldOrPurple, &ItemActionsNoWait);
    GWA3_ASSERT_EQ(identified, 2);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::IdentifyCount(), 2u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastIdentifyKitId(), 100u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastIdentifiedItemId(), 102u);
})

GWA3_TEST(dungeon_item_actions_salvage_items_consumes_matching_inventory_items, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 3u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 200u, GWA3::Bot::DungeonItemPolicy::MODEL_SALV_KIT,
                                         29u, 1u, 100u, 0u, GWA3::Bot::DungeonInventory::RARITY_BLUE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 201u, 17001u,
                                         27u, 1u, 50u, 0u, GWA3::Bot::DungeonInventory::RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 202u, 17002u,
                                         27u, 1u, 50u, 0u, GWA3::Bot::DungeonInventory::RARITY_GOLD);

    const int salvaged = SalvageItems(&GWA3::Bot::DungeonItemPolicy::ShouldSalvageItem, &ItemActionsNoWait);
    GWA3_ASSERT_EQ(salvaged, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastSalvageKitId(), 200u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastSalvagedItemId(), 201u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::SalvageOpenCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::SalvageDoneCount(), 1u);
    GWA3_ASSERT(GWA3::Bot::DungeonInventory::FindItemByModel(17001u) == nullptr);
})

GWA3_TEST(dungeon_item_actions_sell_items_transacts_matching_items, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 300u, 18001u,
                                         27u, 1u, 75u, 0x1u, GWA3::Bot::DungeonInventory::RARITY_BLUE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 301u, GWA3::Bot::DungeonItemPolicy::MODEL_ARMOR_SALV,
                                         9u, 1u, 75u, 0x1u, GWA3::Bot::DungeonInventory::RARITY_WHITE);

    const int sold = SellItems(&GWA3::Bot::DungeonItemPolicy::ShouldSellItem, &ItemActionsNoWait);
    GWA3_ASSERT_EQ(sold, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::TransactCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::LastTransactType(), 0xBu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::LastTransactItemId(), 300u);
})

GWA3_TEST(dungeon_item_actions_deposit_items_moves_matches_to_storage, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(8u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 400u, 930u,
                                         GWA3::Bot::DungeonItemPolicy::ITEM_TYPE_MATERIAL,
                                         1u, 0u, 0u, GWA3::Bot::DungeonInventory::RARITY_GOLD);

    const int deposited = DepositItemsToStorage(&GWA3::Bot::DungeonItemPolicy::ShouldStoreItem, &ItemActionsNoWait);
    GWA3_ASSERT_EQ(deposited, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMovedItemId(), 400u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMoveBagId(), 8u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMoveSlot(), 0u);
})

GWA3_TEST(dungeon_item_actions_use_item_by_model_uses_matching_inventory_item, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 500u, GWA3::Bot::DungeonItemPolicy::MODEL_ARMOR_SALV,
                                         9u, 1u, 0u, 0u, GWA3::Bot::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(UseItemByModel(GWA3::Bot::DungeonItemPolicy::MODEL_ARMOR_SALV, &ItemActionsNoWait));
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastUsedItemId(), 500u);
})

GWA3_TEST(dungeon_item_actions_use_item_if_effect_missing_skips_active_effects, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::EffectMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 600u, GWA3::Bot::DungeonItemPolicy::MODEL_ARMOR_SALV,
                                         9u, 1u, 0u, 0u, GWA3::Bot::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(UseItemIfEffectMissing(55u, 2053u, GWA3::Bot::DungeonItemPolicy::MODEL_ARMOR_SALV, &ItemActionsNoWait));
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastUsedItemId(), 600u);

    GWA3::TestStubs::EffectMgr::AddEffectForAgent(55u, 2053u, 10.0f);
    GWA3_ASSERT(!UseItemIfEffectMissing(55u, 2053u, GWA3::Bot::DungeonItemPolicy::MODEL_ARMOR_SALV, &ItemActionsNoWait));
})
