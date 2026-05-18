// Consolidated test module generated from small test files.
#include "DungeonInventoryTestSupport.h"
#include "DungeonItemActionsTestSupport.h"
#include <gwa3/game/SkillIds.h>
#include <gwa3/managers/ItemMgr.h>
#include "DungeonItemPolicyTestSupport.h"

// --- tests/test_dungeon_inventory_counts.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_inventory_counts {

using namespace GWA3::DungeonInventory;

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

GWA3_TEST(dungeon_inventory_counts_buffer_empty_slot_when_item_count_is_stale, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 13u, 930u, 11u, 1u, 0u, 0u, RARITY_GOLD);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 14u, 945u, 11u, 1u, 0u, 0u, RARITY_GOLD);
    GWA3::TestStubs::ItemMgr::ClearBagItem(1u, 1u);

    auto* bag = GWA3::ItemMgr::GetBag(1u);
    GWA3_ASSERT(bag != nullptr);
    bag->items_count = bag->items.size;

    GWA3_ASSERT_EQ(CountFreeSlots(), 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_inventory_counts

// --- tests/test_dungeon_inventory_find_item.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_inventory_find_item {

using namespace GWA3::DungeonInventory;

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

GWA3_TEST(dungeon_inventory_emergency_free_slot_drops_white_weapon_first, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 30u, 16001u, 27u, 1u, 60u, 0u, RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 31u, GWA3::ItemModelIds::SUPERIOR_SALVAGE_KIT,
                                         29u, 1u, 100u, 0u, RARITY_BLUE);

    GWA3_ASSERT_EQ(CountFreeSlots(), 0u);
    const auto result = DropEmergencyInventoryItemForFreeSlot();
    GWA3_ASSERT(result.dropped);
    GWA3_ASSERT_EQ(result.item_id, 30u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 30u);
    GWA3_ASSERT_EQ(CountFreeSlots(), 1u);
})

GWA3_TEST(dungeon_inventory_emergency_free_slot_falls_back_to_salvage_kit, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 40u, 930u, 11u, 1u, 0u, 0u, RARITY_GOLD);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 41u, GWA3::ItemModelIds::SALVAGE_KIT,
                                         29u, 1u, 100u, 0u, RARITY_BLUE);

    GWA3_ASSERT_EQ(CountFreeSlots(), 0u);
    const auto result = DropEmergencyInventoryItemForFreeSlot();
    GWA3_ASSERT(result.dropped);
    GWA3_ASSERT_EQ(result.item_id, 41u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 41u);
    GWA3_ASSERT_EQ(CountFreeSlots(), 1u);
})

GWA3_TEST(dungeon_inventory_emergency_free_slot_protects_event_items_before_salvage_kit, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 42u, GWA3::ItemModelIds::VICTORY_TOKEN,
                                         0u, 1u, 0u, 0u, RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 43u, GWA3::ItemModelIds::SALVAGE_KIT,
                                         29u, 1u, 100u, 0u, RARITY_BLUE);

    GWA3_ASSERT_EQ(CountFreeSlots(), 0u);
    const auto result = DropEmergencyInventoryItemForFreeSlot();
    GWA3_ASSERT(result.dropped);
    GWA3_ASSERT_EQ(result.item_id, 43u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 43u);
    GWA3_ASSERT_EQ(CountFreeSlots(), 1u);
})

GWA3_TEST(dungeon_inventory_emergency_free_slot_can_drop_green_when_enabled, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 45u, 16003u, 27u, 1u, 60u, 0u, RARITY_GREEN);

    const auto defaultResult = DropEmergencyInventoryItemForFreeSlot();
    GWA3_ASSERT(!defaultResult.dropped);
    GWA3_ASSERT_EQ(CountFreeSlots(), 0u);

    EmergencyFreeSlotOptions options;
    options.allow_green_items = true;
    const auto greenResult = DropEmergencyInventoryItemForFreeSlot(options);
    GWA3_ASSERT(greenResult.dropped);
    GWA3_ASSERT_EQ(greenResult.item_id, 45u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 45u);
    GWA3_ASSERT_EQ(CountFreeSlots(), 1u);
})

GWA3_TEST(dungeon_inventory_emergency_ensure_free_slots_drops_until_threshold, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 4u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 50u, 16001u, 27u, 1u, 60u, 0u, RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 51u, 16002u, 27u, 1u, 60u, 0u, RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 52u, 930u, 11u, 1u, 0u, 0u, RARITY_GOLD);

    GWA3_ASSERT_EQ(CountFreeSlots(), 1u);
    const uint32_t dropped = EnsureEmergencyFreeSlots(3u);
    GWA3_ASSERT_EQ(dropped, 2u);
    GWA3_ASSERT_EQ(CountFreeSlots(), 3u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_inventory_find_item

// --- tests/test_dungeon_item_actions_deposit_items.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_deposit_items {

using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_deposit_items_moves_matches_to_storage, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(8u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 400u, 930u,
                                         GWA3::DungeonItemPolicy::ITEM_TYPE_MATERIAL,
                                         1u, 0u, 0u, GWA3::DungeonInventory::RARITY_GOLD);

    const int deposited = DepositItemsToStorage(&GWA3::DungeonItemPolicy::ShouldStoreItem, &ItemActionSupport::ItemActionsNoWait);
    GWA3_ASSERT_EQ(deposited, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMovedItemId(), 400u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMoveBagId(), 8u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMoveSlot(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_deposit_items

// --- tests/test_dungeon_item_actions_identify_items.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_identify_items {

using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_identify_items_uses_kit_and_marks_targets, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 4u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 100u, GWA3::ItemModelIds::IDENTIFICATION_KIT,
                                         29u, 1u, 100u, 0u, GWA3::DungeonInventory::RARITY_BLUE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 101u, 16001u,
                                         27u, 1u, 100u, 0u, GWA3::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 102u, 16002u,
                                         27u, 1u, 100u, 0u, GWA3::DungeonInventory::RARITY_PURPLE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 3u, 103u, 16003u,
                                         27u, 1u, 100u, 0u, GWA3::DungeonInventory::RARITY_WHITE);

    const int identified = IdentifyItems(&ItemActionSupport::ShouldIdentifyGoldOrPurple, &ItemActionSupport::ItemActionsNoWait);
    GWA3_ASSERT_EQ(identified, 2);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::IdentifyCount(), 2u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastIdentifyKitId(), 100u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastIdentifiedItemId(), 102u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_identify_items

// --- tests/test_dungeon_item_actions_salvage_items.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_salvage_items {

using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_salvage_items_consumes_matching_inventory_items, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 3u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 200u, GWA3::ItemModelIds::SUPERIOR_SALVAGE_KIT,
                                         29u, 1u, 100u, 0u, GWA3::DungeonInventory::RARITY_BLUE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 201u, 17001u,
                                         27u, 1u, 50u, 0u, GWA3::DungeonInventory::RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 202u, 17002u,
                                         27u, 1u, 50u, 0u, GWA3::DungeonInventory::RARITY_GOLD);

    const int salvaged = SalvageItems(&GWA3::DungeonItemPolicy::ShouldSalvageItem, &ItemActionSupport::ItemActionsNoWait);
    GWA3_ASSERT_EQ(salvaged, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastSalvageKitId(), 200u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastSalvagedItemId(), 201u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::SalvageOpenCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::SalvageDoneCount(), 1u);
    GWA3_ASSERT(GWA3::DungeonInventory::FindItemByModel(17001u) == nullptr);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_salvage_items

// --- tests/test_dungeon_item_actions_sell_items.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_sell_items {

using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_sell_items_transacts_matching_items, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 300u, 18001u,
                                         27u, 1u, 75u, 0x1u, GWA3::DungeonInventory::RARITY_BLUE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 301u, GWA3::ItemModelIds::ARMOR_OF_SALVATION,
                                         9u, 1u, 75u, 0x1u, GWA3::DungeonInventory::RARITY_WHITE);

    const int sold = SellItems(&GWA3::DungeonItemPolicy::ShouldSellItem, &ItemActionSupport::ItemActionsNoWait);
    GWA3_ASSERT_EQ(sold, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::TransactCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::LastTransactType(), 0xBu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::LastTransactItemId(), 300u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_sell_items

// --- tests/test_dungeon_item_actions_use_dp_removal.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_dp_removal {

using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_use_dp_removal_sweet_resets_wipe_count_after_use, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 630u, GWA3::ItemModelIds::HONEYCOMB,
                                         9u, 1u, 0u, 0u, GWA3::DungeonInventory::RARITY_WHITE);

    uint32_t wipeCount = 2u;
    UseItemOptions options;
    options.delay_ms = 0u;
    const DpRemovalUseResult result =
        UseDpRemovalSweetIfNeeded(&wipeCount, &ItemActionSupport::ItemActionsNoWait, options);

    GWA3_ASSERT(result.attempted);
    GWA3_ASSERT_EQ(result.previous_wipe_count, 2u);
    GWA3_ASSERT_EQ(result.used_model_id, GWA3::ItemModelIds::HONEYCOMB);
    GWA3_ASSERT(result.wipe_count_reset);
    GWA3_ASSERT_EQ(wipeCount, 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastUsedItemId(), 630u);
})

GWA3_TEST(dungeon_item_actions_use_dp_removal_sweet_skips_without_wipes, {
    GWA3::TestStubs::ItemMgr::Reset();

    uint32_t wipeCount = 0u;
    const DpRemovalUseResult result =
        UseDpRemovalSweetIfNeeded(&wipeCount, &ItemActionSupport::ItemActionsNoWait);

    GWA3_ASSERT(!result.attempted);
    GWA3_ASSERT_EQ(result.used_model_id, 0u);
    GWA3_ASSERT_EQ(wipeCount, 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_dp_removal

// --- tests/test_dungeon_item_actions_use_first_model.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_first_model {

using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_use_first_item_by_model_uses_first_available_model, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 620u, GWA3::ItemModelIds::SLICE_OF_BIRTHDAY_CAKE,
                                         9u, 1u, 0u, 0u, GWA3::DungeonInventory::RARITY_WHITE);

    const uint32_t models[] = {
        999999u,
        GWA3::ItemModelIds::SLICE_OF_BIRTHDAY_CAKE,
        GWA3::ItemModelIds::CANDY_CORN,
    };

    const uint32_t usedModelId = UseFirstItemByModel(models, _countof(models), &ItemActionSupport::ItemActionsNoWait);
    GWA3_ASSERT_EQ(usedModelId, GWA3::ItemModelIds::SLICE_OF_BIRTHDAY_CAKE);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastUsedItemId(), 620u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_first_model

// --- tests/test_dungeon_item_actions_use_full_conset.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_full_conset {


using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_use_full_conset_uses_missing_pieces_and_reports_active_state, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::EffectMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 3u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 610u, GWA3::ItemModelIds::ARMOR_OF_SALVATION,
                                         9u, 1u, 0u, 0u, GWA3::DungeonInventory::RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 611u, GWA3::ItemModelIds::ESSENCE_OF_CELERITY,
                                         9u, 1u, 0u, 0u, GWA3::DungeonInventory::RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 612u, GWA3::ItemModelIds::GRAIL_OF_MIGHT,
                                         9u, 1u, 0u, 0u, GWA3::DungeonInventory::RARITY_WHITE);

    const ConsetUseResult result = UseFullConsetIfNeeded(55u, &ItemActionSupport::ItemActionsNoWait);
    GWA3_ASSERT(result.used_armor);
    GWA3_ASSERT(result.used_essence);
    GWA3_ASSERT(result.used_grail);
    GWA3_ASSERT(!result.full_active);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastUsedItemId(), 612u);

    GWA3::TestStubs::EffectMgr::AddEffectForAgent(55u, GWA3::SkillIds::ARMOR_OF_SALVATION, 10.0f);
    GWA3::TestStubs::EffectMgr::AddEffectForAgent(55u, GWA3::SkillIds::ESSENCE_OF_CELERITY, 10.0f);
    GWA3::TestStubs::EffectMgr::AddEffectForAgent(55u, GWA3::SkillIds::GRAIL_OF_MIGHT, 10.0f);

    const ConsetUseResult alreadyActive = UseFullConsetIfNeeded(55u, &ItemActionSupport::ItemActionsNoWait);
    GWA3_ASSERT(!alreadyActive.used_armor);
    GWA3_ASSERT(!alreadyActive.used_essence);
    GWA3_ASSERT(!alreadyActive.used_grail);
    GWA3_ASSERT(alreadyActive.full_active);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_full_conset

// --- tests/test_dungeon_item_actions_use_item_by_model.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_item_by_model {

using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_use_item_by_model_uses_matching_inventory_item, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 500u, GWA3::ItemModelIds::ARMOR_OF_SALVATION,
                                         9u, 1u, 0u, 0u, GWA3::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(UseItemByModel(GWA3::ItemModelIds::ARMOR_OF_SALVATION, &ItemActionSupport::ItemActionsNoWait));
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastUsedItemId(), 500u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_item_by_model

// --- tests/test_dungeon_item_actions_use_item_effect_missing.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_item_effect_missing {


using namespace GWA3::DungeonItemActions;
namespace ItemActionSupport = GWA3::Tests::DungeonItemActionsSupport;

GWA3_TEST(dungeon_item_actions_use_item_if_effect_missing_skips_active_effects, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::EffectMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 600u, GWA3::ItemModelIds::ARMOR_OF_SALVATION,
                                         9u, 1u, 0u, 0u, GWA3::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(UseItemIfEffectMissing(55u, GWA3::SkillIds::ARMOR_OF_SALVATION, GWA3::ItemModelIds::ARMOR_OF_SALVATION, &ItemActionSupport::ItemActionsNoWait));
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastUsedItemId(), 600u);

    GWA3::TestStubs::EffectMgr::AddEffectForAgent(55u, GWA3::SkillIds::ARMOR_OF_SALVATION, 10.0f);
    GWA3_ASSERT(!UseItemIfEffectMissing(55u, GWA3::SkillIds::ARMOR_OF_SALVATION, GWA3::ItemModelIds::ARMOR_OF_SALVATION, &ItemActionSupport::ItemActionsNoWait));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_actions_use_item_effect_missing

// --- tests/test_dungeon_item_policy_salvage.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_policy_salvage {

using namespace GWA3::DungeonItemPolicy;
using GWA3::Tests::DungeonItemPolicy::MakeItem;

GWA3_TEST(dungeon_item_policy_salvage_rejects_protected_and_non_junk_items, {
    auto whiteWeapon = MakeItem(4u, 15055u, 27u, GWA3::DungeonInventory::RARITY_WHITE);
    auto blueWeapon = MakeItem(5u, 15056u, 27u, GWA3::DungeonInventory::RARITY_BLUE);
    auto material = MakeItem(6u, 948u, ITEM_TYPE_MATERIAL, GWA3::DungeonInventory::RARITY_WHITE);
    auto salvageKit = MakeItem(7u, GWA3::ItemModelIds::SUPERIOR_SALVAGE_KIT, ITEM_TYPE_KIT, GWA3::DungeonInventory::RARITY_WHITE);
    auto goldWeapon = MakeItem(8u, 15057u, 27u, GWA3::DungeonInventory::RARITY_GOLD);

    GWA3_ASSERT(ShouldSalvageItem(&whiteWeapon));
    GWA3_ASSERT(ShouldSalvageItem(&blueWeapon));
    GWA3_ASSERT(!ShouldSalvageItem(&material));
    GWA3_ASSERT(!ShouldSalvageItem(&salvageKit));
    GWA3_ASSERT(!ShouldSalvageItem(&goldWeapon));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_policy_salvage

// --- tests/test_dungeon_item_policy_sell.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_policy_sell {

using namespace GWA3::DungeonItemPolicy;
using GWA3::Tests::DungeonItemPolicy::MakeItem;

GWA3_TEST(dungeon_item_policy_sell_respects_identification_and_protected_models, {
    auto whiteWeapon = MakeItem(9u, 16001u, 27u, GWA3::DungeonInventory::RARITY_WHITE);
    auto blueUnid = MakeItem(10u, 16002u, 27u, GWA3::DungeonInventory::RARITY_BLUE, 0u, 80u);
    auto blueId = MakeItem(11u, 16003u, 27u, GWA3::DungeonInventory::RARITY_BLUE, 0x1u, 80u);
    auto purpleUnid = MakeItem(12u, 16004u, 27u, GWA3::DungeonInventory::RARITY_PURPLE, 0u, 10u);
    auto goldIdLowValue = MakeItem(13u, 16005u, 27u, GWA3::DungeonInventory::RARITY_GOLD, 0x1u, 50u);
    auto protectedModel = MakeItem(14u, GWA3::ItemModelIds::ARMOR_OF_SALVATION, ITEM_TYPE_USABLE, GWA3::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(ShouldSellItem(&whiteWeapon));
    GWA3_ASSERT(!ShouldSellItem(&blueUnid));
    GWA3_ASSERT(ShouldSellItem(&blueId));
    GWA3_ASSERT(!ShouldSellItem(&purpleUnid));
    GWA3_ASSERT(ShouldSellItem(&goldIdLowValue));
    GWA3_ASSERT(!ShouldSellItem(&protectedModel));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_policy_sell

// --- tests/test_dungeon_item_policy_store.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_item_policy_store {

using namespace GWA3::DungeonItemPolicy;
using GWA3::Tests::DungeonItemPolicy::MakeItem;

GWA3_TEST(dungeon_item_policy_store_keeps_rare_materials_and_greens, {
    auto ecto = MakeItem(1u, 930u, ITEM_TYPE_MATERIAL, GWA3::DungeonInventory::RARITY_GOLD);
    auto green = MakeItem(2u, 555u, 0u, GWA3::DungeonInventory::RARITY_GREEN);
    auto junk = MakeItem(3u, 12345u, 0u, GWA3::DungeonInventory::RARITY_WHITE);

    GWA3_ASSERT(ShouldStoreItem(&ecto));
    GWA3_ASSERT(ShouldStoreItem(&green));
    GWA3_ASSERT(!ShouldStoreItem(&junk));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_item_policy_store
