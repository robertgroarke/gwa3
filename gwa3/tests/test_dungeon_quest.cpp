#include <gwa3/bot/DungeonQuest.h>
#include <gwa3/testing/TestFramework.h>

namespace Quest = GWA3::Bot::DungeonQuest;

GWA3_TEST(dungeon_quest_dialog_plan_expands, {
    static const uint32_t dialogs[] = {0x8101u, 0x834301u};
    const Quest::DialogPlan plan = {dialogs, 2, 3};

    GWA3_ASSERT(Quest::IsValidDialogPlan(plan));
    GWA3_ASSERT_EQ(Quest::GetExpandedDialogCount(plan), 6);

    uint32_t expanded[6] = {};
    GWA3_ASSERT_EQ(Quest::ExpandDialogSequence(plan, expanded, 6), 6);
    GWA3_ASSERT_EQ(expanded[0], 0x8101u);
    GWA3_ASSERT_EQ(expanded[1], 0x834301u);
    GWA3_ASSERT_EQ(expanded[4], 0x8101u);
    GWA3_ASSERT_EQ(expanded[5], 0x834301u);
})

GWA3_TEST(dungeon_quest_expands_dialogs, {
    static const uint32_t dialogs[] = {0x8101u, 0x832A01u};
    static const Quest::TravelPoint entry[] = {{821.0f, 25300.0f}, {1760.0f, 25332.0f}};
    static const Quest::TravelPoint reset[] = {{-16549.0f, 18104.0f}, {-17300.0f, 18500.0f}};

    Quest::BootstrapPlan plan;
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

    GWA3_ASSERT(Quest::IsValidBootstrapPlan(plan));
    GWA3_ASSERT_EQ(Quest::GetExpandedDialogCount(plan), 6);

    uint32_t expanded[6] = {};
    GWA3_ASSERT_EQ(Quest::ExpandDialogSequence(plan, expanded, 6), 6);
    GWA3_ASSERT_EQ(expanded[0], 0x8101u);
    GWA3_ASSERT_EQ(expanded[1], 0x832A01u);
    GWA3_ASSERT_EQ(expanded[4], 0x8101u);
    GWA3_ASSERT_EQ(expanded[5], 0x832A01u);

    const auto* lastEntry = Quest::GetLastTravelPoint(plan.entry_path, plan.entry_path_count);
    const auto* lastReset = Quest::GetLastTravelPoint(plan.reset_path, plan.reset_path_count);
    const auto zonePoint = Quest::ResolveBootstrapZonePoint(plan);
    GWA3_ASSERT(lastEntry != nullptr);
    GWA3_ASSERT(lastReset != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(lastEntry->x), 1760);
    GWA3_ASSERT_EQ(static_cast<int>(lastReset->y), 18500);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.x), 1760);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.y), 25332);
})

GWA3_TEST(dungeon_quest_cycle_validates_two_phase_bootstrap, {
    static const uint32_t rewardDialogs[] = {0x8101u, 0x834307u};
    static const uint32_t acceptDialogs[] = {0x8101u, 0x834301u};
    static const Quest::TravelPoint approach[] = {
        {-16687.0f, 10730.0f},
        {-19787.0f, 10647.0f},
        {-21885.0f, 14297.0f},
        {-24788.0f, 15648.0f},
    };

    Quest::QuestCyclePlan plan;
    plan.npc = {-15526.0f, 8811.0f, 1500.0f};
    plan.reward_dialog = {rewardDialogs, 2, 2};
    plan.accept_dialog = {acceptDialogs, 2, 3};
    plan.approach_path = approach;
    plan.approach_path_count = 4;
    plan.dungeon_entry = {-26100.0f, 16100.0f};
    plan.dungeon_exit = {-18700.0f, -14350.0f};
    plan.start_map_id = 553u;
    plan.dungeon_map_id = 617u;

    GWA3_ASSERT(Quest::IsValidQuestCyclePlan(plan));
    GWA3_ASSERT_EQ(Quest::GetExpandedDialogCount(plan.reward_dialog), 4);
    GWA3_ASSERT_EQ(Quest::GetExpandedDialogCount(plan.accept_dialog), 6);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_entry.x), -26100);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_exit.y), -14350);
})

GWA3_TEST(dungeon_quest_rejects_invalid_plan, {
    Quest::BootstrapPlan plan;
    GWA3_ASSERT(!Quest::IsValidBootstrapPlan(plan));
    GWA3_ASSERT_EQ(Quest::GetExpandedDialogCount(plan), 0);

    uint32_t expanded[2] = {};
    GWA3_ASSERT_EQ(Quest::ExpandDialogSequence(plan, expanded, 2), 0);
    GWA3_ASSERT(Quest::GetLastTravelPoint(nullptr, 0) == nullptr);
    const auto zonePoint = Quest::ResolveBootstrapZonePoint(plan);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.x), 0);
    GWA3_ASSERT_EQ(static_cast<int>(zonePoint.y), 0);
    GWA3_ASSERT(!Quest::IsValidDialogPlan({nullptr, 0, 0}));
    GWA3_ASSERT(!Quest::IsValidQuestCyclePlan({}));
})
