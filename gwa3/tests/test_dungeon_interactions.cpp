#include <gwa3/bot/DungeonInteractions.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
void SetHeldBundleItemId(uint32_t itemId);
uint32_t LastDroppedItemId();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::UIMgr {

void Reset();
void SetActionKeyDownResult(bool result);
void SetPerformUiActionResult(bool result);
uint32_t LastActionKeyDown();
uint32_t ActionKeyDownCount();
uint32_t LastPerformUiAction();
uint32_t PerformUiActionCount();
uint32_t LastPerformUiActionDirect();
uint32_t PerformUiActionDirectCount();

} // namespace GWA3::TestStubs::UIMgr

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void AddItemAgent(uint32_t agentId, float x, float y, uint32_t itemId, uint32_t owner);
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);

} // namespace GWA3::TestStubs::AgentMgr

using namespace GWA3::Bot::DungeonInteractions;

GWA3_TEST(dungeon_interactions_known_chest_ids, {
    GWA3_ASSERT(IsChestGadgetId(6062u));
    GWA3_ASSERT(IsChestGadgetId(4579u));
    GWA3_ASSERT(IsChestGadgetId(4582u));
    GWA3_ASSERT(IsChestGadgetId(8141u));
    GWA3_ASSERT(IsChestGadgetId(74u));
    GWA3_ASSERT(IsChestGadgetId(68u));
    GWA3_ASSERT(IsChestGadgetId(9157u));
    GWA3_ASSERT(!IsChestGadgetId(9999u));
})

GWA3_TEST(dungeon_interactions_opened_chest_tracker_resets_by_map, {
    OpenedChestTracker tracker;
    tracker.ResetForMap(100u);
    tracker.MarkOpened(5u);
    tracker.MarkOpened(6u);
    tracker.MarkOpened(6u);

    GWA3_ASSERT_EQ(tracker.map_id(), 100u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(tracker.count()), 2u);
    GWA3_ASSERT(tracker.IsOpened(5u));
    GWA3_ASSERT(tracker.IsOpened(6u));
    GWA3_ASSERT(!tracker.IsOpened(7u));

    tracker.ResetForMap(101u);
    GWA3_ASSERT_EQ(tracker.map_id(), 101u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(tracker.count()), 0u);
    GWA3_ASSERT(!tracker.IsOpened(5u));
})

GWA3_TEST(dungeon_interactions_get_held_bundle_item_id_returns_zero_when_empty, {
    GWA3::TestStubs::ItemMgr::Reset();

    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 0u);
})

GWA3_TEST(dungeon_interactions_get_held_bundle_item_id_returns_bundle_item_id, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(1337u);

    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 1337u);
})

GWA3_TEST(dungeon_interactions_drop_held_bundle_returns_false_without_bundle, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();

    GWA3_ASSERT(!DropHeldBundle());
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 0u);
})

GWA3_TEST(dungeon_interactions_drop_held_bundle_falls_back_to_inventory_drop_when_ui_action_unavailable, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(4242u);

    GWA3_ASSERT(DropHeldBundle());
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 4242u);
    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastPerformUiAction(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastPerformUiActionDirect(), 0xCDu);
})

GWA3_TEST(dungeon_interactions_drop_held_bundle_can_disable_inventory_fallback, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(4242u);

    GWA3_ASSERT(!DropHeldBundle(false, false));
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 4242u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastPerformUiActionDirect(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastPerformUiAction(), 0xCDu);
})

GWA3_TEST(dungeon_interactions_drop_held_bundle_uses_action_key_down_when_available, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(4242u);
    GWA3::TestStubs::UIMgr::SetActionKeyDownResult(true);

    GWA3_ASSERT(DropHeldBundle());
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 4242u);
})

GWA3_TEST(dungeon_interactions_drop_held_bundle_uses_perform_ui_action_when_action_key_unavailable, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(4242u);
    GWA3::TestStubs::UIMgr::SetPerformUiActionResult(true);

    GWA3_ASSERT(DropHeldBundle());
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastPerformUiActionDirect(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 4242u);
})

GWA3_TEST(dungeon_interactions_drop_held_bundle_can_assume_effect_only_bundle, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::UIMgr::SetPerformUiActionResult(true);

    GWA3_ASSERT(DropHeldBundle(true));
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastPerformUiActionDirect(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
})

GWA3_TEST(dungeon_interactions_drop_held_bundle_can_assume_effect_only_bundle_with_action_key, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::UIMgr::SetActionKeyDownResult(true);

    GWA3_ASSERT(DropHeldBundle(true));
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
})

GWA3_TEST(dungeon_interactions_find_nearest_signpost_and_item, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::AddAgent(4u, 600.0f, 0.0f, 0x200u);
    GWA3::TestStubs::AgentMgr::AddAgent(7u, 200.0f, 0.0f, 0x200u);
    GWA3::TestStubs::AgentMgr::AddAgent(9u, 500.0f, 0.0f, 0x400u);
    GWA3::TestStubs::AgentMgr::AddAgent(11u, 150.0f, 0.0f, 0x400u);

    GWA3_ASSERT_EQ(FindNearestSignpost(0.0f, 0.0f, 1000.0f), 7u);
    GWA3_ASSERT_EQ(FindNearestItem(0.0f, 0.0f, 1000.0f), 11u);
})

GWA3_TEST(dungeon_interactions_find_nearest_item_by_model_filters_ground_loot, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 3u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 1001u, 930u, 11u, 1u, 0u, 0u, 0u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 1002u, 24350u, 6u, 1u, 0u, 0u, 0u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 2u, 1003u, 24350u, 6u, 1u, 0u, 0u, 0u);
    GWA3::TestStubs::AgentMgr::AddItemAgent(21u, 100.0f, 0.0f, 1001u, 0u);
    GWA3::TestStubs::AgentMgr::AddItemAgent(22u, 250.0f, 0.0f, 1002u, 0u);
    GWA3::TestStubs::AgentMgr::AddItemAgent(23u, 400.0f, 0.0f, 1003u, 0u);

    GWA3_ASSERT_EQ(FindNearestItem(0.0f, 0.0f, 1000.0f), 21u);
    GWA3_ASSERT_EQ(FindNearestItemByModel(0.0f, 0.0f, 1000.0f, 24350u), 22u);
    GWA3_ASSERT_EQ(FindNearestItemByModel(0.0f, 0.0f, 1000.0f, 99999u), 0u);
})

GWA3_TEST(dungeon_interactions_find_nearest_npc_filters_non_npcs_and_dead_npcs, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::AddNpc(3u, 700.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(5u, 150.0f, 0.0f, 5u, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(6u, 100.0f, 0.0f, 6u, 0.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(8u, 250.0f, 0.0f, 6u, 1.0f);

    GWA3_ASSERT_EQ(FindNearestNpc(0.0f, 0.0f, 1000.0f), 8u);
})

GWA3_TEST(dungeon_interactions_collect_nearest_npcs_sorts_candidates, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::AddNpc(3u, 700.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(5u, 150.0f, 0.0f, 5u, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(6u, 100.0f, 0.0f, 6u, 0.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(8u, 250.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(10u, 450.0f, 0.0f, 6u, 1.0f);

    uint32_t npcIds[4] = {};
    const std::size_t count = CollectNearestNpcs(0.0f, 0.0f, 1000.0f, npcIds, 4u);

    GWA3_ASSERT_EQ(static_cast<unsigned>(count), 3u);
    GWA3_ASSERT_EQ(npcIds[0], 8u);
    GWA3_ASSERT_EQ(npcIds[1], 10u);
    GWA3_ASSERT_EQ(npcIds[2], 3u);
})
