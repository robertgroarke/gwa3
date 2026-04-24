#include <gwa3/bot/DungeonInteractions.h>
#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/bot/DungeonLoot.h>
#include <gwa3/game/Agent.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetGold(uint32_t charGold, uint32_t storageGold);
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
uint32_t LastPickedItemAgentId();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void SetPlayerAgent(float x, float y, float hp);
void AddItemAgent(uint32_t agentId, float x, float y, uint32_t itemId, uint32_t owner);
void AddGadgetAgent(uint32_t agentId, float x, float y, uint32_t gadgetId);
uint32_t LastInteractedSignpostId();

} // namespace GWA3::TestStubs::AgentMgr

using namespace GWA3::Bot::DungeonLoot;

namespace {

void MoveNear(float, float, float) {
}

} // namespace

GWA3_TEST(dungeon_loot_pickup_model_filters_match_froggy_rules, {
    GWA3_ASSERT(IsAlwaysPickupModel(930u));
    GWA3_ASSERT(IsAlwaysPickupModel(22751u));
    GWA3_ASSERT(!IsAlwaysPickupModel(12345u));

    GWA3_ASSERT(IsQuestPickupModel(22342u));
    GWA3_ASSERT(IsQuestPickupModel(21796u));
    GWA3_ASSERT(!IsQuestPickupModel(9999u));
})

GWA3_TEST(dungeon_loot_should_pick_up_item_agent_honors_owner_and_inventory_guard, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetGold(0u, 0u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 100u, 930u, TYPE_MATERIAL, 1u, 0u, 0u,
                                         GWA3::Bot::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::AgentMgr::AddItemAgent(30u, 100.0f, 0.0f, 100u, 1u);

    auto* ownedAgent = GWA3::AgentMgr::GetAgentByID(30u);
    auto* itemAgent = static_cast<const GWA3::AgentItem*>(ownedAgent);
    GWA3_ASSERT_EQ(itemAgent->owner, 1u);
    GWA3_ASSERT_EQ(itemAgent->item_id, 100u);
    GWA3_ASSERT(!ShouldPickUpItemAgent(ownedAgent, 1u, 0u));
    GWA3_ASSERT(ShouldPickUpItemAgent(ownedAgent, 1u, 2u));
    GWA3_ASSERT(!ShouldPickUpItemAgent(ownedAgent, 2u, 0u));
})

GWA3_TEST(dungeon_loot_pick_up_nearby_loot_picks_eligible_item, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 3u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 200u, 930u, TYPE_MATERIAL, 1u, 0u, 0u,
                                         GWA3::Bot::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::AgentMgr::AddItemAgent(40u, 50.0f, 0.0f, 200u, 1u);

    const int picked = PickUpNearbyLoot(200.0f);
    GWA3_ASSERT_EQ(picked, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastPickedItemAgentId(), 40u);
})

GWA3_TEST(dungeon_loot_open_nearby_chest_marks_tracker_and_interacts, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddGadgetAgent(50u, 100.0f, 0.0f, 6062u);

    GWA3::Bot::DungeonInteractions::OpenedChestTracker tracker;
    tracker.ResetForMap(0u);

    GWA3_ASSERT(OpenNearbyChest(200.0f, tracker, &MoveNear));
    GWA3_ASSERT(tracker.IsOpened(50u));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedSignpostId(), 50u);
})
