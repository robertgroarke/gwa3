// Consolidated test module generated from small test files.
#include "DungeonBundleTestSupport.h"

// --- tests/test_dungeon_bundle_acquire_fallback.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_acquire_fallback {

using namespace GWA3::DungeonBundle;

GWA3_TEST(dungeon_bundle_bundle_acquire_tries_world_action_before_signpost_fallback, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddGadgetAgent(60u, 50.0f, 0.0f, 8608u);

    GWA3_ASSERT(!InteractSignpostAndAcquireHeldBundleNearPoint(0.0f, 0.0f, 1000.0f, 1, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 60u);
    GWA3_ASSERT_EQ(AgentStubs::SignpostInteractionCount(), 4u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_acquire_fallback

// --- tests/test_dungeon_bundle_action_interact.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_action_interact {

using namespace GWA3::DungeonBundle;

GWA3_TEST(dungeon_bundle_action_interact_bundle_open_matches_raven_torch_flow, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    GWA3::TestStubs::UIMgr::Reset();
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
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 5001u);
})

GWA3_TEST(dungeon_bundle_model_acquire_accepts_matching_equipped_torch_without_inventory_bundle, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 1u);
    ItemStubs::SetBagItem(1u, 0u, 506u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::SetPlayerEquippedItems(506u, 0u);
    GWA3::TestStubs::UIMgr::SetActionKeyDownResult(true);

    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 0u);
    GWA3_ASSERT_EQ(GetHeldOrEquippedBundleItemIdByModel(22342u), 506u);
    GWA3_ASSERT(DropHeldOrEquippedBundleByModel(22342u));
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);

    ItemStubs::Reset();
    AgentStubs::SetPlayerEquippedItems(777u, 0u);
    GWA3_ASSERT_EQ(GetHeldOrEquippedBundleItemIdByModel(22342u), 0u);
    GWA3_ASSERT(DropHeldOrEquippedBundleItem(777u));
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);

    ItemStubs::SetBagItem(1u, 0u, 506u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::SetPlayerEquippedItems(506u, 0u);
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
    GWA3_ASSERT_EQ(AgentStubs::SignpostInteractionCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_action_interact

// --- tests/test_dungeon_bundle_autoit_acquire.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_autoit_acquire {

using namespace GWA3::DungeonBundle;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_autoit_acquire

// --- tests/test_dungeon_bundle_chest_preferred_generic_followup.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_chest_preferred_generic_followup {

using namespace GWA3::DungeonBundle;

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

GWA3_TEST(dungeon_bundle_chest_preferred_uses_action_key_after_resolved_chest_open, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddGadgetAgent(71u, 40.0f, 0.0f, 8141u);

    GWA3_ASSERT(!OpenChestAndAcquireHeldBundleByModelChestPreferred(
        0.0f,
        0.0f,
        22342u,
        1000.0f,
        1000.0f,
        2,
        1,
        0u,
        0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 71u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 2u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0x80u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_chest_preferred_generic_followup

// --- tests/test_dungeon_bundle_chest_preferred_loose_offset.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_chest_preferred_loose_offset {

using namespace GWA3::DungeonBundle;

GWA3_TEST(dungeon_bundle_chest_preferred_keeps_exact_generic_when_loose_chest_is_far, {
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
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 60u);
    GWA3_ASSERT_EQ(AgentStubs::ActionInteractCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 3501u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_chest_preferred_loose_offset

// --- tests/test_dungeon_bundle_ravens_torch_chest_precedence.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_ravens_torch_chest_precedence {

using namespace GWA3::DungeonBundle;

GWA3_TEST(dungeon_bundle_prefers_ravens_8141_chest_over_marker_and_object, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::SetPlayerAgent(-5904.0f, -15071.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 1u);
    ItemStubs::SetBagItem(1u, 0u, 4101u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::AddGadgetAgent(3u, -6191.0f, -15856.0f, 8469u);
    AgentStubs::AddGadgetAgent(181u, -6218.0f, -15617.0f, 8141u);
    AgentStubs::AddGadgetAgent(196u, -6422.0f, -15801.0f, 3u);
    AgentStubs::AddItemAgent(197u, -6218.0f, -15617.0f, 4101u, 0u);

    GWA3_ASSERT(OpenChestAndAcquireHeldBundleByModel(
        -6462.0f,
        -15819.0f,
        22342u,
        1500.0f,
        18000.0f,
        1,
        1,
        0u,
        0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 181u);
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 4101u);
})

GWA3_TEST(dungeon_bundle_chest_preferred_uses_visible_ravens_chest_over_exact_marker, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    AgentStubs::SetPlayerAgent(-5575.0f, 6115.0f, 1.0f);
    ItemStubs::SetBagCapacity(1u, 1u);
    ItemStubs::SetBagItem(1u, 0u, 4201u, 22342u, 6u, 1u, 0u, 0u, 0u);
    AgentStubs::AddGadgetAgent(9u, -5138.0f, 6553.0f, 8153u);
    AgentStubs::AddGadgetAgent(29u, -5662.0f, 5736.0f, 8178u);
    AgentStubs::AddGadgetAgent(38u, -4945.0f, 6766.0f, 8323u);
    AgentStubs::AddGadgetAgent(67u, -5115.0f, 6015.0f, 8141u);
    AgentStubs::AddGadgetAgent(243u, -5768.0f, 6120.0f, 3u);
    AgentStubs::AddItemAgent(244u, -5115.0f, 6015.0f, 4201u, 0u);

    GWA3_ASSERT(OpenChestAndAcquireHeldBundleByModelChestPreferred(
        -5643.0f,
        6112.0f,
        22342u,
        1500.0f,
        18000.0f,
        2,
        1,
        0u,
        0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 67u);
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 4201u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_ravens_torch_chest_precedence

// --- tests/test_dungeon_bundle_interact_signpost.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_interact_signpost {

using namespace GWA3::DungeonBundle;

GWA3_TEST(dungeon_bundle_interacts_with_nearest_signpost, {
    AgentStubs::ResetAgents();
    AgentStubs::AddAgent(10u, 500.0f, 0.0f, 0x200u);
    AgentStubs::AddAgent(12u, 100.0f, 0.0f, 0x200u);

    GWA3_ASSERT(InteractSignpostNearPoint(0.0f, 0.0f, 1000.0f, 2, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 12u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_interact_signpost

// --- tests/test_dungeon_bundle_legacy_generic_first.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_legacy_generic_first {

using namespace GWA3::DungeonBundle;

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
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 4001u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_legacy_generic_first

// --- tests/test_dungeon_bundle_open_chest_pick_bundle.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_chest_pick_bundle {

using namespace GWA3::DungeonBundle;

GWA3_TEST(dungeon_bundle_opens_chest_and_picks_bundle, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::AddAgent(30u, 50.0f, 0.0f, 0x200u);
    AgentStubs::AddAgent(31u, 75.0f, 0.0f, 0x400u);

    GWA3_ASSERT(OpenChestAndPickUpBundle(0.0f, 0.0f, 1000.0f, 1000.0f, 2, 3, 0u, 0u));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 30u);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 31u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_chest_pick_bundle

// --- tests/test_dungeon_bundle_open_chest_pick_model.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_chest_pick_model {

using namespace GWA3::DungeonBundle;

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
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 2001u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_chest_pick_model

// --- tests/test_dungeon_bundle_open_door_sequence.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_door_sequence {

using namespace GWA3::Tests::DungeonBundle;

GWA3_TEST(dungeon_bundle_open_door_sequence, {
    AgentStubs::ResetAgents();
    ResetMoveRecorder();
    AgentStubs::AddAgent(50u, 70.0f, 0.0f, 0x200u);

    GWA3::DungeonBundle::DoorOpenOptions options;
    options.interact_count = 6;
    options.interact_delay_ms = 0u;

    GWA3_ASSERT(GWA3::DungeonBundle::TryOpenDoorAt(0.0f, 0.0f, options));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 50u);
    GWA3_ASSERT_EQ(AgentStubs::SignpostInteractionCount(), 6u);
    GWA3_ASSERT(GWA3::DungeonBundle::ExecuteDoorOpenSequence(
        0.0f,
        0.0f,
        &RecordMoveToPoint,
        options,
        200.0f,
        0u));
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveX), 0);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveY), 0);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 200);
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_door_sequence

// --- tests/test_dungeon_bundle_open_door_sequence_returns_false_without_signpost.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_door_sequence_returns_false_without_signpost {

using namespace GWA3::Tests::DungeonBundle;

GWA3_TEST(dungeon_bundle_open_door_sequence_returns_false_without_signpost, {
    AgentStubs::ResetAgents();
    ResetMoveRecorder();

    GWA3::DungeonBundle::DoorOpenOptions options;
    options.interact_count = 6;
    options.interact_delay_ms = 0u;

    GWA3_ASSERT(!GWA3::DungeonBundle::ExecuteDoorOpenSequence(
        0.0f,
        0.0f,
        &RecordMoveToPoint,
        options,
        200.0f,
        0u));
    GWA3_ASSERT_EQ(g_moveCallCount, 0);
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_open_door_sequence_returns_false_without_signpost

// --- tests/test_dungeon_bundle_pick_nearest_item.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_pick_nearest_item {

using namespace GWA3::DungeonBundle;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_pick_nearest_item

// --- tests/test_dungeon_bundle_pick_nearest_model.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_pick_nearest_model {

using namespace GWA3::DungeonBundle;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_pick_nearest_model

// --- tests/test_dungeon_bundle_prefers_chest_signpost.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_prefers_chest_signpost {

using namespace GWA3::DungeonBundle;

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
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 3001u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_prefers_chest_signpost

// --- tests/test_dungeon_bundle_try_open_chest_at.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_bundle_try_open_chest_at {

GWA3_TEST(dungeon_bundle_try_open_chest_at, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    AgentStubs::AddAgent(30u, 40.0f, 0.0f, 0x200u);
    AgentStubs::AddAgent(31u, 55.0f, 0.0f, 0x400u);

    GWA3::DungeonInteractions::OpenedChestTracker tracker;
    GWA3_ASSERT(GWA3::DungeonBundle::TryOpenChestAt(0.0f, 0.0f, 100u, tracker));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedSignpostId(), 30u);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 31u);

    GWA3_ASSERT(!GWA3::DungeonBundle::TryOpenChestAt(0.0f, 0.0f, 100u, tracker));
    GWA3_ASSERT(GWA3::DungeonBundle::TryOpenChestAt(0.0f, 0.0f, 101u, tracker));
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_bundle_try_open_chest_at
