#include <gwa3/bot/DungeonQuestRuntime.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/testing/TestFramework.h>

#include <cstddef>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace ItemStubs = GWA3::TestStubs::ItemMgr;
namespace QuestStubs = GWA3::TestStubs::QuestMgr;
namespace QuestRuntime = GWA3::Bot::DungeonQuestRuntime;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance = 6u, float hp = 1.0f);
void SetPlayerAgent(float x, float y, float hp = 1.0f);
uint32_t ChangeTargetCount();
uint32_t LastChangedTargetId();
uint32_t LastInteractedNpcId();
uint32_t NpcInteractionCount();
uint32_t MoveCount();
float LastMoveX();
float LastMoveY();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs();
void ResetQuestState();
void SetQuest(uint32_t questId, uint32_t logState = 1u);
void SetQuestGrantedOnDialog(uint32_t dialogId, uint32_t questId, uint32_t logState = 1u);
std::size_t DialogCount();
uint32_t DialogAt(std::size_t index);
uint32_t RequestQuestInfoCount();

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::TestStubs::CtoS {

void Reset();
uint32_t SendPacketCount();
uint32_t LastSendPacketHeader();
uint32_t LastSendPacketArg1();

} // namespace GWA3::TestStubs::CtoS

namespace GWA3::TestStubs::MapMgr {

void Reset();
void SetMapId(uint32_t mapId);
void SetLoadingState(uint32_t loadingState);
void SetMoveSetsMapId(uint32_t mapId);
void SetMoveSetsMapIdOnPoint(float x, float y, uint32_t mapId);

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset();
void SetDialogState(bool open, uint32_t senderAgentId, uint32_t buttonCount);
void SetAutoDialogOnNpcInteract(bool enabled, uint32_t buttonCount = 1u);

} // namespace GWA3::TestStubs::DialogMgr

GWA3_TEST(dungeon_quest_runtime_sends_dialog_plan, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::Bot::DungeonQuest::DialogPlan plan = {dialogs, 2, 2};
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

GWA3_TEST(dungeon_quest_runtime_interacts_and_sends_dialogs, {
    static const uint32_t dialogs[] = {0x8101u, 0x832501u};
    const GWA3::Bot::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::Bot::DungeonQuest::QuestNpcAnchor npc = {18329.0f, -18134.0f, 1500.0f};
    QuestRuntime::DialogExecutionOptions options;
    options.interact_delay_ms = 0u;
    options.post_interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;
    options.repeat_delay_ms = 0u;
    options.interact_count = 3;
    options.max_retries_per_dialog = 1;

    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    AgentStubs::AddNpc(42u, 18320.0f, -18120.0f);

    GWA3_ASSERT(QuestRuntime::InteractNearestNpcAndSendDialogPlan(npc, plan, options));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedNpcId(), 42u);
    GWA3_ASSERT_EQ(AgentStubs::NpcInteractionCount(), 3u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 2u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(1), 0x832501u);
})

GWA3_TEST(dungeon_quest_runtime_can_use_direct_npc_interact, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::Bot::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::Bot::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};
    QuestRuntime::DialogExecutionOptions options;
    options.change_target_delay_ms = 0u;
    options.interact_delay_ms = 0u;
    options.post_interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;
    options.repeat_delay_ms = 0u;
    options.interact_count = 2;
    options.max_retries_per_dialog = 1;
    options.use_direct_npc_interact = true;

    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::CtoS::Reset();
    AgentStubs::AddNpc(42u, -15520.0f, 8800.0f);

    GWA3_ASSERT(QuestRuntime::InteractNearestNpcAndSendDialogPlan(npc, plan, options));
    GWA3_ASSERT_EQ(AgentStubs::ChangeTargetCount(), 2u);
    GWA3_ASSERT_EQ(AgentStubs::LastChangedTargetId(), 42u);
    GWA3_ASSERT_EQ(AgentStubs::NpcInteractionCount(), 0u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::SendPacketCount(), 2u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketHeader(), GWA3::Packets::INTERACT_NPC);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketArg1(), 42u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 2u);
})

GWA3_TEST(dungeon_quest_runtime_can_move_onto_npc_and_require_dialog_before_send, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::Bot::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::Bot::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};
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

GWA3_TEST(dungeon_quest_runtime_fails_when_required_dialog_never_opens, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::Bot::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::Bot::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};
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

GWA3_TEST(dungeon_quest_runtime_fails_without_npc, {
    static const uint32_t dialogs[] = {0x8101u, 0x832A01u};
    const GWA3::Bot::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::Bot::DungeonQuest::QuestNpcAnchor npc = {1012.0f, 25505.0f, 1500.0f};
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

GWA3_TEST(dungeon_quest_runtime_waits_for_accepted_quest_and_sets_active, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::Bot::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::Bot::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};

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

GWA3_TEST(dungeon_quest_runtime_recognizes_missing_quest_is_already_cleared, {
    QuestRuntime::QuestVerificationOptions options;
    options.refresh_delay_ms = 0u;
    options.poll_ms = 0u;
    options.post_set_active_delay_ms = 0u;

    QuestStubs::ResetQuestState();
    GWA3_ASSERT(QuestRuntime::WaitForQuestState(0x343u, false, options));
    GWA3_ASSERT_EQ(QuestStubs::RequestQuestInfoCount(), 0u);
})

GWA3_TEST(dungeon_quest_runtime_follows_path_and_zones, {
    static const GWA3::Bot::DungeonQuest::TravelPoint path[] = {
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

GWA3_TEST(dungeon_quest_runtime_executes_bootstrap_plan, {
    static const uint32_t dialogs[] = {0x2AE6u, 0x833901u};
    static const GWA3::Bot::DungeonQuest::TravelPoint path[] = {
        {12228.0f, 22677.0f},
        {12470.0f, 25036.0f},
        {12968.0f, 26219.0f},
    };

    const GWA3::Bot::DungeonQuest::BootstrapPlan plan = {
        {12396.0f, 22407.0f, 1500.0f},
        dialogs,
        2,
        1,
        path,
        3,
        {13097.0f, 26393.0f},
        nullptr,
        0,
        558u,
        0u,
        615u,
    };

    QuestRuntime::DialogExecutionOptions dialogOptions;
    dialogOptions.interact_delay_ms = 0u;
    dialogOptions.post_interact_delay_ms = 0u;
    dialogOptions.dialog_delay_ms = 0u;
    dialogOptions.repeat_delay_ms = 0u;
    dialogOptions.interact_count = 2;
    dialogOptions.max_retries_per_dialog = 2;

    QuestRuntime::BootstrapExecutionOptions bootstrapOptions;
    bootstrapOptions.move_timeout_ms = 1000u;
    bootstrapOptions.move_reissue_ms = 10u;
    bootstrapOptions.zone_timeout_ms = 1000u;
    bootstrapOptions.zone_poll_ms = 10u;

    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f);
    AgentStubs::AddNpc(42u, 12390.0f, 22400.0f);
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(558u);
    GWA3::TestStubs::MapMgr::SetMoveSetsMapIdOnPoint(13097.0f, 26393.0f, 615u);

    GWA3_ASSERT(QuestRuntime::ExecuteBootstrapPlan(plan, dialogOptions, bootstrapOptions));
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedNpcId(), 42u);
    GWA3_ASSERT_EQ(AgentStubs::NpcInteractionCount(), 2u);
    GWA3_ASSERT_EQ(static_cast<unsigned>(QuestStubs::DialogCount()), 4u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(0), 0x2AE6u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(3), 0x833901u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 13097);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 26393);
})
