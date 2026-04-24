#include <gwa3/bot/DungeonBundle.h>
#include <gwa3/bot/DungeonInteractions.h>
#include <gwa3/testing/TestFramework.h>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace ItemStubs = GWA3::TestStubs::ItemMgr;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void AddGadgetAgent(uint32_t agentId, float x, float y, uint32_t gadgetId);
void AddItemAgent(uint32_t agentId, float x, float y, uint32_t itemId, uint32_t owner);
void SetPlayerAgent(float x, float y, float hp);
uint32_t LastInteractedSignpostId();
uint32_t LastChangedTargetId();
uint32_t MoveCount();
float LastMoveX();
float LastMoveY();
uint32_t SignpostInteractionCount();
uint32_t ActionInteractCount();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
uint32_t LastPickedItemAgentId();

} // namespace GWA3::TestStubs::ItemMgr

using namespace GWA3::Bot::DungeonBundle;

GWA3_TEST(dungeon_bundle_interacts_with_nearest_signpost, {
    AgentStubs::ResetAgents();
    AgentStubs::AddAgent(10u, 500.0f, 0.0f, 0x200u);
    AgentStubs::AddAgent(12u, 100.0f, 0.0f, 0x200u);

    GWA3_ASSERT(InteractSignpostNearPoint(0.0f, 0.0f, 1000.0f, 2, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 12u);
})

GWA3_TEST(dungeon_bundle_bundle_acquire_tries_world_action_before_signpost_fallback, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddGadgetAgent(60u, 50.0f, 0.0f, 8608u);

    GWA3_ASSERT(!InteractSignpostAndAcquireHeldBundleNearPoint(0.0f, 0.0f, 1000.0f, 1, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 60u);
    GWA3_ASSERT_EQ(AgentStubs::SignpostInteractionCount(), 4u);
})

GWA3_TEST(dungeon_bundle_autoit_bundle_acquire_uses_two_legacy_signpost_bursts, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddGadgetAgent(61u, 50.0f, 0.0f, 8608u);

    GWA3_ASSERT(!AutoItGoToSignpostAndAcquireHeldBundleNearPoint(0.0f, 0.0f, 1000.0f, 2, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 61u);
    GWA3_ASSERT_EQ(AgentStubs::SignpostInteractionCount(), 4u);
    GWA3_ASSERT_EQ(AgentStubs::ActionInteractCount(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::LastChangedTargetId(), 0u);
})

GWA3_TEST(dungeon_bundle_picks_up_nearest_item, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddAgent(20u, 900.0f, 0.0f, 0x400u);
    AgentStubs::AddAgent(21u, 150.0f, 0.0f, 0x400u);

    GWA3_ASSERT(PickUpNearestItemNearPoint(0.0f, 0.0f, 1000.0f, 2, 0u));
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 21u);
    GWA3_ASSERT(AgentStubs::MoveCount() >= 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastChangedTargetId(), 0u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 150);
})

GWA3_TEST(dungeon_bundle_picks_up_nearest_item_by_model, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 3u);
    ItemStubs::SetBagItem(1u, 0u, 1001u, 930u, 11u, 1u, 0u, 0u, 0u);
    ItemStubs::SetBagItem(1u, 1u, 1002u, 24350u, 6u, 1u, 0u, 0u, 0u);
    ItemStubs::SetBagItem(1u, 2u, 1003u, 24350u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::AddItemAgent(20u, 100.0f, 0.0f, 1001u, 0u);
    AgentStubs::AddItemAgent(21u, 300.0f, 0.0f, 1002u, 0u);
    AgentStubs::AddItemAgent(22u, 500.0f, 0.0f, 1003u, 0u);

    GWA3_ASSERT(PickUpNearestItemByModelNearPoint(0.0f, 0.0f, 24350u, 1000.0f, 1, 0u));
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 21u);
    GWA3_ASSERT(AgentStubs::MoveCount() >= 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastChangedTargetId(), 0u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_bundle_opens_chest_and_picks_bundle, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::AddAgent(30u, 50.0f, 0.0f, 0x200u);
    AgentStubs::AddAgent(31u, 75.0f, 0.0f, 0x400u);

    GWA3_ASSERT(OpenChestAndPickUpBundle(0.0f, 0.0f, 1000.0f, 1000.0f, 2, 3, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 30u);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 31u);
})

GWA3_TEST(dungeon_bundle_opens_chest_and_acquires_held_bundle_by_model, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 2u);
    ItemStubs::SetBagItem(1u, 0u, 2001u, 22342u, 6u, 1u, 0u, 0u, 0u);
    ItemStubs::SetBagItem(1u, 1u, 2002u, 930u, 11u, 1u, 0u, 0u, 0u);
    AgentStubs::AddAgent(40u, 50.0f, 0.0f, 0x200u);
    AgentStubs::AddItemAgent(41u, 75.0f, 0.0f, 2001u, 0u);
    AgentStubs::AddItemAgent(42u, 100.0f, 0.0f, 2002u, 0u);

    GWA3_ASSERT(OpenChestAndAcquireHeldBundleByModel(0.0f, 0.0f, 22342u, 1000.0f, 1000.0f, 2, 3, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 40u);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 41u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonInteractions::GetHeldBundleItemId(), 2001u);
})

GWA3_TEST(dungeon_bundle_prefers_chest_signpost_when_opening_bundle_chest, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 1u);
    ItemStubs::SetBagItem(1u, 0u, 3001u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::AddGadgetAgent(50u, 60.0f, 0.0f, 9999u);
    AgentStubs::AddGadgetAgent(51u, 120.0f, 0.0f, 6062u);
    AgentStubs::AddItemAgent(52u, 125.0f, 0.0f, 3001u, 0u);

    GWA3_ASSERT(OpenChestAndAcquireHeldBundleByModel(0.0f, 0.0f, 22342u, 1000.0f, 1000.0f, 1, 1, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 51u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonInteractions::GetHeldBundleItemId(), 3001u);
})

GWA3_TEST(dungeon_bundle_chest_preferred_open_uses_loose_chest_offset_when_exact_point_is_generic, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 1u);
    ItemStubs::SetBagItem(1u, 0u, 3501u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::AddGadgetAgent(60u, 40.0f, 0.0f, 3u);
    AgentStubs::AddGadgetAgent(61u, 800.0f, 0.0f, 8141u);
    AgentStubs::AddItemAgent(62u, 805.0f, 0.0f, 3501u, 0u);

    GWA3_ASSERT(OpenChestAndAcquireHeldBundleByModelChestPreferred(
        0.0f,
        0.0f,
        22342u,
        1000.0f,
        1000.0f,
        1,
        1,
        0u,
        0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 61u);
    GWA3_ASSERT_EQ(AgentStubs::ActionInteractCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonInteractions::GetHeldBundleItemId(), 3501u);
})

GWA3_TEST(dungeon_bundle_chest_preferred_tries_center_generic_followup_when_no_bundle_state_appears, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddGadgetAgent(70u, 40.0f, 0.0f, 3u);
    AgentStubs::AddGadgetAgent(71u, 800.0f, 0.0f, 8141u);

    GWA3_ASSERT(!OpenChestAndAcquireHeldBundleByModelChestPreferred(
        0.0f,
        0.0f,
        22342u,
        1000.0f,
        1000.0f,
        1,
        1,
        0u,
        0u));
    GWA3_ASSERT_EQ(AgentStubs::LastChangedTargetId(), 0u);
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 70u);
    GWA3_ASSERT_EQ(AgentStubs::ActionInteractCount(), 0u);
})

GWA3_TEST(dungeon_bundle_tries_legacy_generic_signpost_path_before_chest_fallback, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 1u);
    ItemStubs::SetBagItem(1u, 0u, 4001u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::AddGadgetAgent(70u, 40.0f, 0.0f, 3u);
    AgentStubs::AddGadgetAgent(71u, 800.0f, 0.0f, 8141u);
    AgentStubs::AddItemAgent(72u, 45.0f, 0.0f, 4001u, 0u);

    GWA3_ASSERT(OpenChestAndAcquireHeldBundleByModelLegacy(0.0f, 0.0f, 22342u, 1000.0f, 1000.0f, 1, 1, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 70u);
    GWA3_ASSERT_EQ(AgentStubs::ActionInteractCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonInteractions::GetHeldBundleItemId(), 4001u);
})

GWA3_TEST(dungeon_bundle_action_interact_bundle_open_matches_raven_torch_flow, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 1u);
    ItemStubs::SetBagItem(1u, 0u, 5001u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::AddGadgetAgent(80u, 40.0f, 0.0f, 3u);
    AgentStubs::AddItemAgent(81u, 45.0f, 0.0f, 5001u, 0u);

    GWA3_ASSERT(OpenChestAndAcquireHeldBundleByModelActionInteract(
        0.0f,
        0.0f,
        22342u,
        1000.0f,
        2,
        3,
        0u,
        0u));
    GWA3_ASSERT_EQ(AgentStubs::ActionInteractCount(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 0u);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 81u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonInteractions::GetHeldBundleItemId(), 5001u);
})
