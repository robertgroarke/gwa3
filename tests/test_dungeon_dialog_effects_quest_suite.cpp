// Consolidated test module generated from small test files.
// --- merged from tests/test_dungeon_dialog_effects_suite.cpp ---
#include "DungeonDialogTestSupport.h"
#include "DungeonEffectsTestSupport.h"

// --- tests/test_dungeon_dialog_advance_sends_first.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_dialog_advance_sends_first {

using namespace GWA3::DungeonDialog;

GWA3_TEST(dungeon_dialog_advance_to_button_sends_first_non_target_button, {
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    GWA3::TestStubs::DialogMgr::SetDialogState(true, 42u, 0u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(0u, 0x80u, 1u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(1u, 0x81u, 1u);

    DialogAdvanceOptions options;
    options.sender_agent_id = 42u;
    options.max_passes = 1;
    options.post_dialog_wait_ms = 0u;

    GWA3_ASSERT(!AdvanceDialogToButton(0x833901u, options));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x80u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_dialog_advance_sends_first

// --- tests/test_dungeon_dialog_advance_visible.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_dialog_advance_visible {

using namespace GWA3::DungeonDialog;

GWA3_TEST(dungeon_dialog_advance_to_button_returns_true_when_target_is_visible, {
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    GWA3::TestStubs::DialogMgr::SetDialogState(true, 42u, 0u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(0u, 0x833901u, 2u);

    DialogAdvanceOptions options;
    options.sender_agent_id = 42u;
    options.post_dialog_wait_ms = 0u;

    GWA3_ASSERT(AdvanceDialogToButton(0x833901u, options));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_dialog_advance_visible

// --- tests/test_dungeon_dialog_has_button.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_dialog_has_button {

using namespace GWA3::DungeonDialog;

GWA3_TEST(dungeon_dialog_has_dialog_button_scans_current_buttons, {
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::DialogMgr::SetDialogState(true, 42u, 0u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(0u, 0x80u, 1u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(1u, 0x833901u, 2u);

    GWA3_ASSERT(HasDialogButton(0x833901u));
    GWA3_ASSERT(!HasDialogButton(0x833905u));
    GWA3_ASSERT(!HasDialogButton(0u));
})

GWA3_TEST(dungeon_dialog_snapshot_reports_sender_and_buttons, {
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::DialogMgr::SetDialogState(true, 42u, 0u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(0u, 0x80u, 1u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(1u, 0x833901u, 2u);

    const DialogSnapshot snapshot = CaptureDialogSnapshot();
    GWA3_ASSERT(snapshot.dialog_open);
    GWA3_ASSERT_EQ(snapshot.sender_agent_id, 42u);
    GWA3_ASSERT_EQ(snapshot.button_count, 2u);
    GWA3_ASSERT(IsDialogOpenFromSenderWithButton(42u, 0x833901u));
    GWA3_ASSERT(!IsDialogOpenFromSenderWithButton(7u, 0x833901u));
    GWA3_ASSERT(!IsDialogOpenFromSenderWithButton(42u, 0x833905u));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_dialog_has_button

// --- tests/test_dungeon_dialog_repeated_sequence.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_dialog_repeated_sequence {

using namespace GWA3::DungeonDialog;

GWA3_TEST(dungeon_dialog_repeated_sequence_replays_full_loop, {
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    const uint32_t dialogs[] = {0x8101u, 0x832A01u};

    GWA3_ASSERT(SendDialogSequenceRepeated(dialogs, 2, 3, 0u, 0u, 1));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 6u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(1), 0x832A01u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(2), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(3), 0x832A01u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(4), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(5), 0x832A01u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_dialog_repeated_sequence

// --- tests/test_dungeon_dialog_retry_single.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_dialog_retry_single {

using namespace GWA3::DungeonDialog;

GWA3_TEST(dungeon_dialog_retry_repeats_single_dialog, {
    GWA3::TestStubs::QuestMgr::ResetDialogs();

    GWA3_ASSERT(SendDialogWithRetry(0x8101u, 3, 0u));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 3u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(2), 0x8101u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_dialog_retry_single

// --- tests/test_dungeon_dialog_sequence_order.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_dialog_sequence_order {

using namespace GWA3::DungeonDialog;

GWA3_TEST(dungeon_dialog_sequence_preserves_order, {
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    const uint32_t dialogs[] = {0x8101u, 0x832303u, 0x8101u, 0x832301u};

    GWA3_ASSERT(SendDialogSequence(dialogs, 4, 0u, 1));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 4u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(1), 0x832303u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(2), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(3), 0x832301u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_dialog_sequence_order

// --- tests/test_dungeon_effects_apply_title.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_effects_apply_title {

using namespace GWA3::DungeonEffects;
using namespace GWA3::Tests::DungeonEffects;

GWA3_TEST(dungeon_effects_ensure_active_title_applies_and_waits, {
    GWA3::TestStubs::PlayerMgr::Reset();
    g_effectWaitMs = 0u;

    const auto result = EnsureActiveTitle(0x27u, 750u, &RecordEffectWait);

    GWA3_ASSERT(!result.already_active);
    GWA3_ASSERT(result.applied);
    GWA3_ASSERT_EQ(result.previous_title_id, 0u);
    GWA3_ASSERT_EQ(result.final_title_id, 0x27u);
    GWA3_ASSERT_EQ(GWA3::PlayerMgr::GetActiveTitleId(), 0x27u);
    GWA3_ASSERT_EQ(g_effectWaitMs, 750u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_effects_apply_title

// --- tests/test_dungeon_effects_blessing_acquire.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_effects_blessing_acquire {

GWA3_TEST(dungeon_effects_blessing_acquire, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    GWA3::TestStubs::EffectMgr::Reset();
    GWA3::TestStubs::PlayerMgr::Reset();
    GWA3::TestStubs::DialogMgr::Reset();

    GWA3::TestStubs::AgentMgr::AddNpc(90u, 100.0f, 50.0f, 6u, 1.0f);
    GWA3::TestStubs::QuestMgr::SetBlessingDialogEffect(
        GWA3::Bot::Froggy::BLESSING_ACCEPT_DIALOG_ID,
        GWA3::SkillIds::VETERAN_DWARVEN_RAIDER);

    GWA3::DungeonEffects::BlessingAcquireOptions options;
    options.interact_count = 3;
    options.dialog_retries = 1;
    options.required_title_id = GWA3::Bot::Froggy::BLESSING_TITLE_ID;
    options.accept_dialog_id = GWA3::Bot::Froggy::BLESSING_ACCEPT_DIALOG_ID;
    options.title_settle_delay_ms = 0u;
    options.interact_delay_ms = 0u;
    options.dialog_delay_ms = 0u;

    GWA3_ASSERT(!GWA3::DungeonEffects::HasBlessing());
    const auto result = GWA3::DungeonEffects::TryAcquireBlessingAt(0.0f, 0.0f, options);

    GWA3_ASSERT(!result.already_active);
    GWA3_ASSERT(result.npc_found);
    GWA3_ASSERT(result.interacted);
    GWA3_ASSERT(result.dialog_sent);
    GWA3_ASSERT(result.confirmed);
    GWA3_ASSERT(result.title_applied);
    GWA3_ASSERT(result.dialog_hooks_toggled);
    GWA3_ASSERT_EQ(result.npc_id, 90u);
    GWA3_ASSERT_EQ(result.final_title_id, GWA3::Bot::Froggy::BLESSING_TITLE_ID);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedNpcId(), 90u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::NpcInteractionCount(), 3u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogCount(), static_cast<std::size_t>(1));
    GWA3_ASSERT_EQ(
        GWA3::TestStubs::QuestMgr::DialogAt(0),
        GWA3::Bot::Froggy::BLESSING_ACCEPT_DIALOG_ID);
    GWA3_ASSERT_EQ(GWA3::TestStubs::DialogMgr::ShutdownCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::DialogMgr::InitializeCount(), 1u);
    GWA3_ASSERT(GWA3::DungeonEffects::HasBlessing());
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_effects_blessing_acquire

// --- tests/test_dungeon_effects_specific_blessing.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_effects_specific_blessing {

namespace {

void NoWait(uint32_t) {}

bool SettleImmediately(uint32_t, float) {
    return true;
}

} // namespace

GWA3_TEST(dungeon_effects_specific_blessing_does_not_treat_wrong_title_as_active, {
    GWA3::TestStubs::EffectMgr::Reset();

    GWA3::TestStubs::EffectMgr::AddEffect(GWA3::SkillIds::VETERAN_DWARVEN_RAIDER);

    GWA3_ASSERT(GWA3::DungeonEffects::HasAnyDungeonBlessing());
    GWA3_ASSERT(GWA3::DungeonEffects::HasDungeonBlessingForTitle(GWA3::TitleID::Deldrimor));
    GWA3_ASSERT(!GWA3::DungeonEffects::HasDungeonBlessingForTitle(GWA3::TitleID::Norn));
})

GWA3_TEST(dungeon_effects_specific_blessing_acquire_ignores_existing_other_blessing, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    GWA3::TestStubs::EffectMgr::Reset();
    GWA3::TestStubs::PlayerMgr::Reset();
    GWA3::TestStubs::DialogMgr::Reset();

    GWA3::TestStubs::EffectMgr::AddEffect(GWA3::SkillIds::VETERAN_DWARVEN_RAIDER);
    GWA3::TestStubs::AgentMgr::AddNpc(90u, 100.0f, 50.0f, 6u, 1.0f);
    GWA3::TestStubs::QuestMgr::SetBlessingDialogEffect(
        0x84u,
        GWA3::SkillIds::NORN_HUNTING_PARTY);

    GWA3::DungeonEffects::BlessingInteractionOptions options;
    options.required_title_id = GWA3::TitleID::Norn;
    options.accept_dialog_id = 0x84u;
    options.title_settle_delay_ms = 0u;
    options.settle_timeout_ms = 0u;
    options.wait_for_position_settle = &SettleImmediately;
    options.wait_ms = &NoWait;
    options.require_specific_blessing = true;

    const auto result = GWA3::DungeonEffects::AcquireDungeonBlessingAt(0.0f, 0.0f, options);

    GWA3_ASSERT(!result.already_active);
    GWA3_ASSERT(result.npc_found);
    GWA3_ASSERT(result.interacted);
    GWA3_ASSERT(result.dialog_sent);
    GWA3_ASSERT(result.confirmed);
    GWA3_ASSERT(GWA3::DungeonEffects::HasDungeonBlessingForTitle(GWA3::TitleID::Norn));
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_effects_specific_blessing

// --- tests/test_dungeon_effects_skip_current_title.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_effects_skip_current_title {

using namespace GWA3::DungeonEffects;
using namespace GWA3::Tests::DungeonEffects;

GWA3_TEST(dungeon_effects_ensure_active_title_skips_current_title, {
    GWA3::TestStubs::PlayerMgr::Reset();
    GWA3::PlayerMgr::SetActiveTitle(0x27u);
    g_effectWaitMs = 0u;

    const auto result = EnsureActiveTitle(0x27u, 750u, &RecordEffectWait);

    GWA3_ASSERT(result.already_active);
    GWA3_ASSERT(!result.applied);
    GWA3_ASSERT_EQ(result.previous_title_id, 0x27u);
    GWA3_ASSERT_EQ(result.final_title_id, 0x27u);
    GWA3_ASSERT_EQ(g_effectWaitMs, 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_effects_skip_current_title

// --- merged from tests/test_dungeon_quest_suite.cpp ---
#include "DungeonQuestTestSupport.h"

// --- tests/test_dungeon_quest_bootstrap_expands.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_bootstrap_expands {

GWA3_TEST(dungeon_quest_expands_dialogs, {
    static const uint32_t dialogs[] = {0x8101u, 0x832A01u};
    static const ::Quest::TravelPoint entry[] = {{821.0f, 25300.0f}, {1760.0f, 25332.0f}};
    static const ::Quest::TravelPoint reset[] = {{-16549.0f, 18104.0f}, {-17300.0f, 18500.0f}};

    ::Quest::BootstrapPlan plan;
    plan.npc = {1012.0f, 25505.0f, 1500.0f};
    plan.dialog_ids = dialogs;
    plan.dialog_count = 2;
    plan.dialog_repeats = 3;
    plan.entry_path = entry;
    plan.entry_path_count = 2;
    plan.reset_path = reset;
    plan.reset_path_count = 2;
    plan.entry_map_id = 546u;
    plan.reset_map_id = 643u;
    plan.target_map_id = 630u;

    GWA3_ASSERT(::Quest::IsValidBootstrapPlan(plan));
    GWA3_ASSERT_EQ(::Quest::GetExpandedDialogCount(plan), 6);

    uint32_t expanded[6] = {};
    GWA3_ASSERT_EQ(::Quest::ExpandDialogSequence(plan, expanded, 6), 6);
    GWA3_ASSERT_EQ(expanded[0], 0x8101u);
    GWA3_ASSERT_EQ(expanded[1], 0x832A01u);
    GWA3_ASSERT_EQ(expanded[4], 0x8101u);
    GWA3_ASSERT_EQ(expanded[5], 0x832A01u);

    const auto* lastEntry = ::Quest::GetLastTravelPoint(plan.entry_path, plan.entry_path_count);
    const auto* lastReset = ::Quest::GetLastTravelPoint(plan.reset_path, plan.reset_path_count);
    const auto zonePoint = ::Quest::ResolveBootstrapZonePoint(plan);
    GWA3_ASSERT(lastEntry != nullptr);
    GWA3_ASSERT(lastReset != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(lastEntry->x), 1760);
    GWA3_ASSERT_EQ(static_cast<int>(lastReset->y), 18500);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.x), 1760);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.y), 25332);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_bootstrap_expands

// --- tests/test_dungeon_quest_cycle_plan.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_cycle_plan {

GWA3_TEST(dungeon_quest_cycle_validates_two_phase_bootstrap, {
    static const uint32_t rewardDialogs[] = {0x8101u, 0x834307u};
    static const uint32_t acceptDialogs[] = {0x8101u, 0x834301u};
    static const ::Quest::TravelPoint approach[] = {
        {-16687.0f, 10730.0f},
        {-19787.0f, 10647.0f},
        {-21885.0f, 14297.0f},
        {-24788.0f, 15648.0f},
    };

    ::Quest::QuestCyclePlan plan;
    plan.npc = {-15526.0f, 8811.0f, 1500.0f};
    plan.reward_dialog = {rewardDialogs, 2, 2};
    plan.accept_dialog = {acceptDialogs, 2, 3};
    plan.approach_path = approach;
    plan.approach_path_count = 4;
    plan.dungeon_entry = {-26100.0f, 16100.0f};
    plan.dungeon_exit = {-18700.0f, -14350.0f};
    plan.start_map_id = 553u;
    plan.dungeon_map_id = 617u;

    GWA3_ASSERT(::Quest::IsValidQuestCyclePlan(plan));
    GWA3_ASSERT_EQ(::Quest::GetExpandedDialogCount(plan.reward_dialog), 4);
    GWA3_ASSERT_EQ(::Quest::GetExpandedDialogCount(plan.accept_dialog), 6);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_entry.x), -26100);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_exit.y), -14350);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_cycle_plan

// --- tests/test_dungeon_quest_dialog_plan.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_dialog_plan {

GWA3_TEST(dungeon_quest_dialog_plan_expands, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const ::Quest::DialogPlan plan = {dialogs, 2, 3};

    GWA3_ASSERT(::Quest::IsValidDialogPlan(plan));
    GWA3_ASSERT_EQ(::Quest::GetExpandedDialogCount(plan), 6);

    uint32_t expanded[6] = {};
    GWA3_ASSERT_EQ(::Quest::ExpandDialogSequence(plan, expanded, 6), 6);
    GWA3_ASSERT_EQ(expanded[0], 0x8101u);
    GWA3_ASSERT_EQ(expanded[1], 0x834301u);
    GWA3_ASSERT_EQ(expanded[4], 0x8101u);
    GWA3_ASSERT_EQ(expanded[5], 0x834301u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_dialog_plan

// --- tests/test_dungeon_quest_invalid_plan.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_invalid_plan {

GWA3_TEST(dungeon_quest_rejects_invalid_plan, {
    ::Quest::BootstrapPlan plan;
    GWA3_ASSERT(!::Quest::IsValidBootstrapPlan(plan));
    GWA3_ASSERT_EQ(::Quest::GetExpandedDialogCount(plan), 0);

    uint32_t expanded[2] = {};
    GWA3_ASSERT_EQ(::Quest::ExpandDialogSequence(plan, expanded, 2), 0);
    GWA3_ASSERT(::Quest::GetLastTravelPoint(nullptr, 0) == nullptr);
    const auto zonePoint = ::Quest::ResolveBootstrapZonePoint(plan);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.x), 0);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.y), 0);
    GWA3_ASSERT(!::Quest::IsValidDialogPlan({nullptr, 0, 0}));
    GWA3_ASSERT(!::Quest::IsValidQuestCyclePlan({}));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_invalid_plan
