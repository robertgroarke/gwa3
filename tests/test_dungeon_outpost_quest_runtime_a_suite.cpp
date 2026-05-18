// Consolidated test module generated from small test files.
#include "DungeonOutpostSetupTestSupport.h"
#include "DungeonQuestRuntimeTestSupport.h"
#include <gwa3/packets/Headers.h>

// --- tests/test_dungeon_outpost_setup_sample_one_party.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_outpost_setup_sample_one_party {

GWA3_TEST(dungeon_outpost_setup_applies_party_setup_for_sample_one_config, {
    GWA3::TestStubs::MapMgr::SetHardModeEnabled(false);
    GWA3::TestStubs::PartyMgr::ResetFlags();
    GWA3::TestStubs::PartyMgr::ResetHeroes();
    GWA3::TestStubs::PlayerMgr::Reset();
    GWA3::TestStubs::PlayerMgr::SetPlayerName(L"GWA3 SAMPLE ONE");

    auto& cfg = GWA3::Bot::GetConfig();
    cfg = {};
    cfg.hard_mode = true;
    cfg.hero_config_file = "Mercs";
    cfg.hero_ids[0] = 30u;
    cfg.hero_ids[1] = 16u;
    cfg.hero_ids[2] = 23u;
    cfg.hero_ids[3] = 24u;
    cfg.hero_ids[4] = 37u;
    cfg.hero_ids[5] = 6u;
    cfg.hero_ids[6] = 29u;

    GWA3_ASSERT(OutpostSetup::ApplyOutpostSetup(cfg));
    GWA3_ASSERT(cfg.hero_config_file == "Mercs.txt");
    GWA3_ASSERT_EQ(GWA3::PartyMgr::CountPartyHeroes(), 7u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroIdAt(0), 30u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroIdAt(6), 29u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroBehaviorAt(0), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroBehaviorAt(6), 1u);
    GWA3_ASSERT(GWA3::TestStubs::MapMgr::HardModeEnabled());
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_outpost_setup_sample_one_party


// --- tests/test_dungeon_outpost_setup_config_json.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_outpost_setup_config_json {

GWA3_TEST(dungeon_outpost_setup_resolves_preferred_hero_config_from_json, {
    char file[64] = {};

    GWA3_ASSERT(OutpostSetup::ResolvePreferredHeroConfigFromJson(
        "{\"GWA3 SAMPLE ONE\":{\"hero_config\":\"Mercs\"},\"GWA3 SAMPLE TWO\":{\"hero_config\":\"Standard\"}}",
        "GWA3 SAMPLE ONE",
        file,
        sizeof(file)));
    GWA3_ASSERT(std::string(file) == "Mercs.txt");

    GWA3_ASSERT(OutpostSetup::ResolvePreferredHeroConfigFromJson(
        "{\"GWA3 SAMPLE TWO\":{\"hero_config\":\"Standard\"}}",
        "Unknown",
        file,
        sizeof(file)));
    GWA3_ASSERT(std::string(file) == "Standard.txt");
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_outpost_setup_config_json


// --- tests/test_dungeon_outpost_setup_skill_templates.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_outpost_setup_skill_templates {

GWA3_TEST(dungeon_outpost_setup_decodes_skill_templates, {
    uint32_t skills[8] = {};
    GWA3_ASSERT(OutpostSetup::DecodeSkillTemplate("OAOiAyk8gNtePuwJ00ZaNbJA", skills));
    GWA3_ASSERT(skills[0] != 0u || skills[1] != 0u);
    GWA3_ASSERT(!OutpostSetup::DecodeSkillTemplate("", skills));
})

GWA3_TEST(dungeon_outpost_setup_loads_arachnis_template_professions, {
    OutpostSetup::HeroTemplate templates[7] = {};
    const std::size_t count =
        OutpostSetup::LoadHeroTemplatesFromFile("ArachnisStandard.txt", templates, 7u);
    GWA3_ASSERT_EQ(count, 7u);
    GWA3_ASSERT_EQ(templates[0].primary_profession, 8u);
    GWA3_ASSERT_EQ(templates[0].secondary_profession, 1u);
    GWA3_ASSERT_EQ(templates[3].secondary_profession, 1u);
    GWA3_ASSERT_EQ(templates[6].secondary_profession, 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_outpost_setup_skill_templates


// --- tests/test_dungeon_quest_runtime_bootstrap_plan.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_bootstrap_plan {

GWA3_TEST(dungeon_quest_runtime_executes_bootstrap_plan, {
    static const uint32_t dialogs[] = {0x2AE6u, 0x833901u};
    static const GWA3::DungeonQuest::TravelPoint path[] = {
        {12228.0f, 22677.0f},
        {12470.0f, 25036.0f},
        {12968.0f, 26219.0f},
    };

    const GWA3::DungeonQuest::BootstrapPlan plan = {
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
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_bootstrap_plan


// --- tests/test_dungeon_quest_runtime_boss_reward_sequence.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_boss_reward_sequence {

namespace ItemStubs = GWA3::TestStubs::ItemMgr;

namespace {

constexpr uint32_t kRewardDialogs[] = {0x833907u};

GWA3::DungeonQuest::QuestNpcAnchor RewardNpc() {
    return {14618.0f, -17828.0f, 1500.0f};
}

GWA3::DungeonQuest::DialogPlan RewardDialog() {
    return {kRewardDialogs, 1, 3};
}

QuestRuntime::RewardClaimOptions RewardOptions() {
    QuestRuntime::RewardClaimOptions options;
    options.execution.interact_delay_ms = 0u;
    options.execution.post_interact_delay_ms = 0u;
    options.execution.dialog_delay_ms = 0u;
    options.execution.repeat_delay_ms = 0u;
    options.execution.interact_count = 3;
    options.execution.max_retries_per_dialog = 1;
    return options;
}

} // namespace

GWA3_TEST(dungeon_quest_runtime_boss_reward_sequence, {
    AgentStubs::ResetAgents();
    ItemStubs::Reset();
    QuestStubs::ResetDialogs();
    AgentStubs::AddAgent(30u, 14880.0f, -19020.0f, 0x200u);
    AgentStubs::AddAgent(31u, 14895.0f, -19010.0f, 0x400u);
    AgentStubs::AddNpc(91u, 14610.0f, -17820.0f, 6u, 1.0f);

    GWA3::DungeonInteractions::OpenedChestTracker tracker;
    QuestRuntime::BossRewardOptions bossOptions;
    bossOptions.current_map_id = 615u;
    bossOptions.chest_x = 14876.0f;
    bossOptions.chest_y = -19033.0f;
    bossOptions.chest.signpost_search_radius = 1500.0f;
    bossOptions.chest.item_search_radius = 1500.0f;
    bossOptions.chest.interact_count = 1;
    bossOptions.chest.pickup_attempts = 1;
    bossOptions.chest.interact_delay_ms = 0u;
    bossOptions.chest.pickup_delay_ms = 0u;
    bossOptions.reward = RewardOptions();

    const auto bossResult = QuestRuntime::ExecuteBossRewardSequence(
        tracker,
        RewardNpc(),
        RewardDialog(),
        bossOptions);
    GWA3_ASSERT(bossResult.chest_opened);
    GWA3_ASSERT(bossResult.reward_dialog_sent);
    GWA3_ASSERT(bossResult.reward_npc_found);
    GWA3_ASSERT_EQ(ItemStubs::LastPickedItemAgentId(), 31u);
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(3));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_boss_reward_sequence


// --- tests/test_dungeon_quest_runtime_claim_reward.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_claim_reward {

namespace {

constexpr uint32_t kRewardDialogs[] = {0x833907u};

GWA3::DungeonQuest::QuestNpcAnchor RewardNpc() {
    return {14618.0f, -17828.0f, 1500.0f};
}

GWA3::DungeonQuest::DialogPlan RewardDialog() {
    return {kRewardDialogs, 1, 3};
}

QuestRuntime::RewardClaimOptions RewardOptions() {
    QuestRuntime::RewardClaimOptions options;
    options.execution.interact_delay_ms = 0u;
    options.execution.post_interact_delay_ms = 0u;
    options.execution.dialog_delay_ms = 0u;
    options.execution.repeat_delay_ms = 0u;
    options.execution.interact_count = 3;
    options.execution.max_retries_per_dialog = 1;
    return options;
}

} // namespace

GWA3_TEST(dungeon_quest_runtime_claim_reward, {
    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    const auto options = RewardOptions();

    AgentStubs::AddNpc(77u, 14610.0f, -17820.0f, 6u, 1.0f);
    const auto rewardWithNpc = QuestRuntime::TryClaimReward(RewardNpc(), RewardDialog(), options);

    GWA3_ASSERT(rewardWithNpc.npc_found);
    GWA3_ASSERT(rewardWithNpc.dialog_sent);
    GWA3_ASSERT_EQ(rewardWithNpc.npc_id, 77u);
    GWA3_ASSERT_EQ(AgentStubs::LastInteractedNpcId(), 77u);
    GWA3_ASSERT_EQ(AgentStubs::NpcInteractionCount(), 3u);
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(3));
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(0), 0x833907u);
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(2), 0x833907u);

    AgentStubs::ResetAgents();
    QuestStubs::ResetDialogs();
    const auto rewardWithoutNpc = QuestRuntime::TryClaimReward(RewardNpc(), RewardDialog(), options);

    GWA3_ASSERT(!rewardWithoutNpc.npc_found);
    GWA3_ASSERT(rewardWithoutNpc.dialog_sent);
    GWA3_ASSERT_EQ(rewardWithoutNpc.npc_id, 0u);
    GWA3_ASSERT_EQ(QuestStubs::DialogCount(), static_cast<std::size_t>(3));
    GWA3_ASSERT_EQ(QuestStubs::DialogAt(1), 0x833907u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_claim_reward


// --- tests/test_dungeon_quest_runtime_direct_npc_interact.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_direct_npc_interact {


GWA3_TEST(dungeon_quest_runtime_can_use_direct_npc_interact, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const GWA3::DungeonQuest::DialogPlan plan = {dialogs, 2, 1};
    const GWA3::DungeonQuest::QuestNpcAnchor npc = {-15526.0f, 8811.0f, 1500.0f};
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
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_direct_npc_interact


// --- tests/test_dungeon_quest_runtime_entry_ready.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_entry_ready {

GWA3_TEST(dungeon_quest_runtime_wait_for_dungeon_entry_ready_accepts_visible_entry_button, {
    QuestStubs::ResetQuestState();
    QuestStubs::ResetDialogs();
    GWA3::TestStubs::DialogMgr::Reset();
    GWA3::TestStubs::DialogMgr::SetDialogState(true, 42u, 1u);
    GWA3::TestStubs::DialogMgr::SetDialogButton(0u, 0x833905u, 0u);

    QuestRuntime::DungeonEntryReadyOptions options;
    options.quest_id = 0x339u;
    options.entry_dialog_id = 0x833905u;
    options.npc_id = 42u;
    options.timeout_ms = 100u;
    options.poll_ms = 0u;

    const auto result = QuestRuntime::WaitForDungeonEntryReady(options);

    GWA3_ASSERT(result.ready);
    GWA3_ASSERT(result.entry_button_visible);
    GWA3_ASSERT(!result.quest_present);
    GWA3_ASSERT_EQ(result.sender_agent_id, 42u);
    GWA3::TestStubs::DialogMgr::Reset();
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_quest_runtime_entry_ready
