// Consolidated test module generated from small test files.
#include "DungeonOutpostSetupTestSupport.h"
#include "DungeonQuestRuntimeTestSupport.h"
#include <gwa3/packets/Headers.h>

// --- tests/test_dungeon_quest_runtime_fails_required_dialog.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_fails_required_dialog {

GWA3_TEST(dungeon_quest_runtime_fails_when_required_dialog_never_opens, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};
    QuestRuntime::DialogExecutionOptions options;
    options.change_target_delay_ms = 0u;
    options.interact_delay_ms = 0u;
    options.post_interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;
    options.repeat_delay_ms = 0u;
    options.dialog_wait_timeout_ms = 1u;
    options.interact_count = 1;
    options.max_retries_per_dialog = 1;
    options.require_dialog_before_send = true;
    options.use_direct_npc_interact = true;

    AgentStubs::ResetAgents();
    AgentStubs::AddNpc(42u, -15520.0f, 8800.0f);
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::DialogMgr::Reset();

    GWA3_ASSERT(!QuestRuntime::InteractNearestNpcAndSendDialogPlan(npc, plan, options));
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_fails_required_dialog


// --- tests/test_dungeon_quest_runtime_fails_without_npc.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_fails_without_npc {

GWA3_TEST(dungeon_quest_runtime_fails_without_npc, {
    static const uint32_t dialogs[] = {0x8101u, 0x832A01u};
    const GWA3::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::DungeonQuest::QuestNpcAnchor npc = {1012.0f, 25505.0f, 1500.0f};
    QuestRuntime::DialogExecutionOptions options;
    options.interact_delay_ms = 0u;
    options.post_interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;
    options.repeat_delay_ms = 0u;
    options.interact_count = 1;
    options.max_retries_per_dialog = 1;

    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();

    GWA3_ASSERT(!QuestRuntime::InteractNearestNpcAndSendDialogPlan(npc, plan, options));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedNpcId(), 0u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_fails_without_npc


// --- tests/test_dungeon_quest_runtime_follow_path_zone.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_follow_path_zone {

GWA3_TEST(dungeon_quest_runtime_follows_path_and_zones, {
    static const GWA3::DungeonQuest::TravelPoint path[] = {
        {10.0f, 20.0f},
        {30.0f, 40.0f},
    };

    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f);
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(558u);

    GWA3_ASSERT(QuestRuntime::FollowTravelPath(path, 2, 558u, 10.0f, 1000u, 50u));
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 2u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 30);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 40);

    GWA3::TestStubs::MapMgr::SetMoveSetsMapId(615u);
    GWA3_ASSERT(QuestRuntime::ZoneThroughPoint(50.0f, 60.0f, 615u, 1000u, 10u));
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 50);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 60);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_follow_path_zone


// --- tests/test_dungeon_quest_runtime_interact_dialogs.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_interact_dialogs {

GWA3_TEST(dungeon_quest_runtime_interacts_and_sends_dialogs, {
    static const uint32_t dialogs[] = {0x8101u, 0x832501u};
    const GWA3::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::DungeonQuest::QuestNpcAnchor npc = {18329.0f, -18134.0f, 1500.0f};
    QuestRuntime::DialogExecutionOptions options;
    options.interact_delay_ms = 0u;
    options.post_interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;
    options.repeat_delay_ms = 0u;
    options.interact_count = 3;
    options.max_retries_per_dialog = 1;

    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::DialogMgr::Reset();
    AgentStubs::AddNpc(42u, 18320.0f, -18120.0f);

    GWA3_ASSERT(QuestRuntime::InteractNearestNpcAndSendDialogPlan(npc, plan, options));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedNpcId(), 42u);
    GWA3_ASSERT_EQ(AgentStubs::NpcInteractionCount(), 3u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 2u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(1), 0x832501u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_interact_dialogs


// --- tests/test_dungeon_quest_runtime_missing_quest_cleared.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_missing_quest_cleared {

GWA3_TEST(dungeon_quest_runtime_recognizes_missing_quest_is_already_cleared, {
    QuestRuntime::QuestVerificationOptions options;
    options.refresh_delay_ms = 0u;
    options.poll_ms = 0u;
    options.post_set_active_delay_ms = 0u;

    QuestStubs::ResetQuestState();
    GWA3_ASSERT(QuestRuntime::WaitForQuestState(0x343u, false, options));
    GWA3_ASSERT_EQ(QuestStubs::RequestQuestInfoCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_missing_quest_cleared


// --- tests/test_dungeon_quest_runtime_move_onto_npc.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_move_onto_npc {

GWA3_TEST(dungeon_quest_runtime_can_move_onto_npc_and_require_dialog_before_send, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};
    QuestRuntime::DialogExecutionOptions options;
    options.change_target_delay_ms = 0u;
    options.interact_delay_ms = 0u;
    options.post_interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;
    options.repeat_delay_ms = 0u;
    options.interact_count = 1;
    options.max_retries_per_dialog = 1;
    options.move_to_actual_npc = true;
    options.move_to_npc_tolerance = 50.0f;
    options.move_to_npc_timeout_ms = 1000u;
    options.require_dialog_before_send = true;
    options.use_direct_npc_interact = true;

    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f);
    AgentStubs::AddNpc(42u, -15520.0f, 8800.0f);
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::DialogMgr::SetAutoDialogOnNpcInteract(true, 2u);
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(553u);

    GWA3_ASSERT(QuestRuntime::InteractNearestNpcAndSendDialogPlan(npc, plan, options));
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), -15520);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 8800);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::SendPacketCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 2u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_move_onto_npc


// --- tests/test_dungeon_quest_runtime_quest_completed_guard.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_quest_completed_guard {

GWA3_TEST(dungeon_quest_runtime_requires_non_completed_quest_when_requested, {
    QuestRuntime::QuestVerificationOptions options;
    options.refresh_delay_ms = 0u;
    options.refresh_interval_ms = 0u;
    options.poll_ms = 0u;
    options.timeout_ms = 1u;
    options.post_set_active_delay_ms = 0u;
    options.require_not_completed_when_present = true;

    QuestStubs::ResetQuestState();
    QuestStubs::SetQuest(0x343u, 0x02u);

    GWA3_ASSERT(!QuestRuntime::WaitForQuestState(0x343u, true, options));

    QuestStubs::SetQuest(0x343u, 0x00u);
    GWA3_ASSERT(QuestRuntime::WaitForQuestState(0x343u, true, options));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_quest_completed_guard


// --- tests/test_dungeon_quest_runtime_send_dialog_refresh.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_send_dialog_refresh {

GWA3_TEST(dungeon_quest_runtime_send_dialog_and_refresh_reports_quest_state, {
    QuestStubs::ResetDialogs();
    QuestStubs::ResetQuestState();
    QuestStubs::SetQuestGrantedOnDialog(0x834301u, 0x343u, 1u);

    QuestRuntime::QuestDialogOptions options;
    options.post_dialog_wait_ms = 0u;
    options.refresh_delay_ms = 0u;

    const auto result = QuestRuntime::SendDialogAndRefreshQuest(0x834301u, 0x343u, options);

    GWA3_ASSERT(result.sent);
    GWA3_ASSERT(result.quest_present);
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 1u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(0), 0x834301u);
    GWA3_ASSERT_EQ(QuestStubs::RequestQuestInfoCount(), 1u);
    GWA3_ASSERT_EQ(result.last_dialog_id, 0x834301u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_send_dialog_refresh


// --- tests/test_dungeon_quest_runtime_send_plan.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_send_plan {

GWA3_TEST(dungeon_quest_runtime_sends_dialog_plan, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::DungeonQuest::DialogPlan plan = {dialogs, 2, 2};
    QuestRuntime::DialogExecutionOptions options;
    options.interact_delay_ms = 0u;
    options.post_interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;
    options.repeat_delay_ms = 0u;
    options.interact_count = 1;
    options.max_retries_per_dialog = 1;

    QuestStubs::ResetDialogs();
    GWA3_ASSERT(QuestRuntime::SendDialogPlan(plan, options));
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 4u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(0), 0x8101u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(3), 0x834301u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_send_plan


// --- tests/test_dungeon_quest_runtime_wait_accept_active.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_wait_accept_active {

GWA3_TEST(dungeon_quest_runtime_waits_for_accepted_quest_and_sets_active, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};

    QuestRuntime::DialogExecutionOptions dialogOptions;
    dialogOptions.interact_delay_ms = 0u;
    dialogOptions.post_interact_delay_ms = 0u;
    dialogOptions.dialog_delay_ms = 0u;
    dialogOptions.repeat_delay_ms = 0u;

    QuestRuntime::QuestVerificationOptions verifyOptions;
    verifyOptions.refresh_delay_ms = 0u;
    verifyOptions.poll_ms = 0u;
    verifyOptions.post_set_active_delay_ms = 0u;

    AgentStubs::ResetAgents();
    AgentStubs::AddNpc(42u, -15520.0f, 8800.0f);
    QuestStubs::ResetDialogs();
    QuestStubs::ResetQuestState();
    QuestStubs::SetQuestGrantedOnDialog(0x834301u, 0x343u, 2u);

    GWA3_ASSERT(QuestRuntime::InteractNearestNpcAndSendDialogPlan(npc, plan, dialogOptions));
    GWA3_ASSERT(QuestRuntime::WaitForQuestState(0x343u, true, verifyOptions));
    GWA3_ASSERT_EQ(GWA3::QuestMgr::GetActiveQuestId(), 0x343u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_wait_accept_active
