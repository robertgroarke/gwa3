// Consolidated test module generated from small test files.
#include "DungeonInteractionsTestSupport.h"
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/testing/TestFramework.h>
#include <gwa3/packets/Headers.h>

// --- tests/test_dungeon_interactions_candidate_dialog_npc.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_candidate_dialog_npc {


using namespace GWA3::DungeonInteractions;
using namespace GWA3::Tests::DungeonInteractionsSupport;

GWA3_TEST(dungeon_interactions_candidate_dialog_interacts_npc_and_sends_dialog, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    GWA3::TestStubs::AgentMgr::AddNpc(42u, 100.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::DialogMgr::SetAutoDialogOnNpcInteract(true, 1u);

    InteractCandidate candidate;
    candidate.agent_id = 42u;
    candidate.use_signpost = false;
    CandidateDialogOptions options;
    options.dialog_id = 0x84u;
    options.interact_attempts = 1;
    options.prepare_wait_ms = 0u;
    options.post_dialog_wait_ms = 0u;
    options.wait_ms = &NoInteractionWait;

    const auto result = InteractCandidateAndSendDialog(candidate, options);

    GWA3_ASSERT(result.interacted);
    GWA3_ASSERT(result.dialog_sent);
    GWA3_ASSERT_EQ(result.interact_attempts, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedNpcId(), 42u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogCount(), static_cast<std::size_t>(1));
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x84u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_candidate_dialog_npc

// --- tests/test_dungeon_interactions_candidate_dialog_signpost.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_candidate_dialog_signpost {


using namespace GWA3::DungeonInteractions;
using namespace GWA3::Tests::DungeonInteractionsSupport;

GWA3_TEST(dungeon_interactions_candidate_dialog_interacts_signpost_when_no_dialog_opens, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    GWA3::TestStubs::AgentMgr::AddAgent(77u, 100.0f, 0.0f, 0x200u);
    GWA3::TestStubs::DialogMgr::SetDialogState(true, 0u, 1u);

    InteractCandidate candidate;
    candidate.agent_id = 77u;
    candidate.use_signpost = true;
    CandidateDialogOptions options;
    options.dialog_id = 0x84u;
    options.interact_attempts = 1;
    options.prepare_wait_ms = 0u;
    options.post_dialog_wait_ms = 0u;
    options.wait_ms = &NoInteractionWait;

    const auto result = InteractCandidateAndSendDialog(candidate, options);

    GWA3_ASSERT(result.interacted);
    GWA3_ASSERT(!result.dialog_sent);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedSignpostId(), 77u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogCount(), static_cast<std::size_t>(0));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_candidate_dialog_signpost

// --- tests/test_dungeon_interactions_collect_nearest_candidates.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_collect_nearest_candidates {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_collect_nearest_interact_candidates_sorts_unique_targets, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::AddAgent(4u, 600.0f, 0.0f, 0x200u);
    GWA3::TestStubs::AgentMgr::AddAgent(7u, 200.0f, 0.0f, 0x200u);
    GWA3::TestStubs::AgentMgr::AddNpc(8u, 250.0f, 0.0f, 6u, 1.0f);

    InteractCandidate candidates[2] = {};
    const std::size_t count = CollectNearestInteractCandidates(
        0.0f,
        0.0f,
        1000.0f,
        1000.0f,
        candidates,
        2u);

    GWA3_ASSERT_EQ(static_cast<unsigned>(count), 2u);
    GWA3_ASSERT_EQ(candidates[0].agent_id, 7u);
    GWA3_ASSERT(candidates[0].use_signpost);
    GWA3_ASSERT_EQ(static_cast<int>(candidates[0].dist_to_anchor), 200);
    GWA3_ASSERT_EQ(candidates[1].agent_id, 8u);
    GWA3_ASSERT(!candidates[1].use_signpost);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_collect_nearest_candidates

// --- tests/test_dungeon_interactions_collect_nearest_npcs.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_collect_nearest_npcs {


using namespace GWA3::DungeonInteractions;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_collect_nearest_npcs

// --- tests/test_dungeon_interactions_drop_bundle_action_key.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_action_key {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_drop_held_bundle_uses_action_key_press_when_available, {
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
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_action_key

// --- tests/test_dungeon_interactions_drop_bundle_disable_inventory_fallback.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_disable_inventory_fallback {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_drop_held_bundle_can_disable_inventory_fallback, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(4242u);

    GWA3_ASSERT(!DropHeldBundle(false, false));
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 4242u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_disable_inventory_fallback

// --- tests/test_dungeon_interactions_drop_bundle_inventory_fallback.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_inventory_fallback {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_drop_held_bundle_falls_back_to_inventory_drop_when_ui_action_unavailable, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(4242u);

    GWA3_ASSERT(DropHeldBundle());
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 4242u);
    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_inventory_fallback

// --- tests/test_dungeon_interactions_drop_bundle_ui_action.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_ui_action {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_drop_held_bundle_falls_back_to_inventory_when_action_key_unavailable, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(4242u);
    GWA3::TestStubs::UIMgr::SetPerformUiActionResult(true);

    GWA3_ASSERT(DropHeldBundle());
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 4242u);
    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_ui_action

// --- tests/test_dungeon_interactions_drop_bundle_without_bundle.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_without_bundle {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_drop_held_bundle_returns_false_without_bundle, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();

    GWA3_ASSERT(!DropHeldBundle());
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_bundle_without_bundle

// --- tests/test_dungeon_interactions_drop_effect_bundle.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_effect_bundle {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_drop_held_bundle_rejects_effect_only_bundle_without_action_key, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::UIMgr::SetPerformUiActionResult(true);

    GWA3_ASSERT(!DropHeldBundle(true));
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::ActionKeyDownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::LastActionKeyDown(), 0xCDu);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionDirectCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::UIMgr::PerformUiActionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_effect_bundle

// --- tests/test_dungeon_interactions_drop_effect_bundle_action_key.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_effect_bundle_action_key {


using namespace GWA3::DungeonInteractions;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_drop_effect_bundle_action_key

// --- tests/test_dungeon_interactions_find_nearest_item_by_model.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_find_nearest_item_by_model {


using namespace GWA3::DungeonInteractions;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_find_nearest_item_by_model

// --- tests/test_dungeon_interactions_find_nearest_npc.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_find_nearest_npc {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_find_nearest_npc_filters_non_npcs_and_dead_npcs, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::AddNpc(3u, 700.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(5u, 150.0f, 0.0f, 5u, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(6u, 100.0f, 0.0f, 6u, 0.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(8u, 250.0f, 0.0f, 6u, 1.0f);

    GWA3_ASSERT_EQ(FindNearestNpc(0.0f, 0.0f, 1000.0f), 8u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_find_nearest_npc

// --- tests/test_dungeon_interactions_find_nearest_signpost_item.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_find_nearest_signpost_item {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_find_nearest_signpost_and_item, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::AddAgent(4u, 600.0f, 0.0f, 0x200u);
    GWA3::TestStubs::AgentMgr::AddAgent(7u, 200.0f, 0.0f, 0x200u);
    GWA3::TestStubs::AgentMgr::AddAgent(9u, 500.0f, 0.0f, 0x400u);
    GWA3::TestStubs::AgentMgr::AddAgent(11u, 150.0f, 0.0f, 0x400u);

    GWA3_ASSERT_EQ(FindNearestSignpost(0.0f, 0.0f, 1000.0f), 7u);
    GWA3_ASSERT_EQ(FindNearestItem(0.0f, 0.0f, 1000.0f), 11u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_find_nearest_signpost_item

// --- tests/test_dungeon_interactions_held_bundle_empty.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_held_bundle_empty {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_get_held_bundle_item_id_returns_zero_when_empty, {
    GWA3::TestStubs::ItemMgr::Reset();

    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_held_bundle_empty

// --- tests/test_dungeon_interactions_held_bundle_item.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_held_bundle_item {


using namespace GWA3::DungeonInteractions;

GWA3_TEST(dungeon_interactions_get_held_bundle_item_id_returns_bundle_item_id, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(1337u);

    GWA3_ASSERT_EQ(GetHeldBundleItemId(), 1337u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_held_bundle_item

// --- tests/test_dungeon_interactions_known_chest_ids.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_known_chest_ids {

using namespace GWA3::DungeonInteractions;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_known_chest_ids

// --- tests/test_dungeon_interactions_opened_chest_tracker.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_opened_chest_tracker {

using namespace GWA3::DungeonInteractions;

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
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_opened_chest_tracker

// --- tests/test_dungeon_interactions_pulse_direct_npc.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_interactions_pulse_direct_npc {


using namespace GWA3::DungeonInteractions;
using namespace GWA3::Tests::DungeonInteractionsSupport;

GWA3_TEST(dungeon_interactions_pulse_direct_npc_interact_sends_raw_go_npc_until_stopped, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::AgentMgr::AddNpc(42u, 100.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::DialogMgr::SetAutoDialogOnNpcInteract(true, 1u);

    DirectNpcInteractOptions options;
    options.target_wait_ms = 0u;
    options.pass_wait_ms = 0u;
    options.passes = 3;
    options.wait_ms = &NoInteractionWait;
    options.stop_condition = &StopWhenNpcDialogOpen;

    const auto result = PulseDirectNpcInteract(42u, options);

    GWA3_ASSERT(result.interacted);
    GWA3_ASSERT(result.stopped);
    GWA3_ASSERT_EQ(result.passes, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::SendPacketCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketHeader(), GWA3::Packets::INTERACT_NPC);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketArg1(), 42u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketArg2(), 0u);
    GWA3_ASSERT(result.dialog_open);
    GWA3_ASSERT_EQ(result.dialog_sender, 42u);

    GWA3::TestStubs::DialogMgr::Reset();
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_interactions_pulse_direct_npc
