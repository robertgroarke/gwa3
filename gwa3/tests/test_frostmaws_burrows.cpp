#include <gwa3/bot/FrostmawsBurrows.h>
#include <gwa3/testing/TestFramework.h>

namespace Frost = GWA3::Bot::FrostmawsBurrows;

GWA3_TEST(frostmaws_route_definition_counts_and_maps, {
    const auto& run1 = Frost::GetRouteDefinition(Frost::RouteId::RunSifhallaToJagaMoraine);
    const auto& run2 = Frost::GetRouteDefinition(Frost::RouteId::RunJagaMoraineToDungeon);
    const auto& level1 = Frost::GetRouteDefinition(Frost::RouteId::Level1);
    const auto& level2 = Frost::GetRouteDefinition(Frost::RouteId::Level2);
    const auto& level3 = Frost::GetRouteDefinition(Frost::RouteId::Level3);
    const auto& level4 = Frost::GetRouteDefinition(Frost::RouteId::Level4);

    GWA3_ASSERT_EQ(run1.map_id, Frost::MAP_SIFHALLA);
    GWA3_ASSERT_EQ(run1.next_map_id, Frost::MAP_JAGA_MORAINE);
    GWA3_ASSERT_EQ(run1.waypoint_count, 3);

    GWA3_ASSERT_EQ(run2.map_id, Frost::MAP_JAGA_MORAINE);
    GWA3_ASSERT_EQ(run2.next_map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL1);
    GWA3_ASSERT_EQ(run2.waypoint_count, 9);

    GWA3_ASSERT_EQ(level1.map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL1);
    GWA3_ASSERT_EQ(level1.next_map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL2);
    GWA3_ASSERT_EQ(level1.waypoint_count, 5);

    GWA3_ASSERT_EQ(level2.map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL2);
    GWA3_ASSERT_EQ(level2.next_map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL3);
    GWA3_ASSERT_EQ(level2.waypoint_count, 11);

    GWA3_ASSERT_EQ(level3.map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL3);
    GWA3_ASSERT_EQ(level3.next_map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL4);
    GWA3_ASSERT_EQ(level3.waypoint_count, 21);

    GWA3_ASSERT_EQ(level4.map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL4);
    GWA3_ASSERT_EQ(level4.next_map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL5);
    GWA3_ASSERT_EQ(level4.waypoint_count, 21);
})

GWA3_TEST(frostmaws_route_lookup_and_missing_level5_source_are_explicit, {
    const auto* jaga = Frost::FindRouteDefinitionByMapId(Frost::MAP_JAGA_MORAINE);
    const auto* level4 = Frost::FindRouteDefinitionByMapId(Frost::MAP_FROSTMAWS_BURROWS_LVL4);
    const auto* missing = Frost::FindRouteDefinitionByMapId(Frost::MAP_FROSTMAWS_BURROWS_LVL5);

    GWA3_ASSERT(jaga != nullptr);
    GWA3_ASSERT(level4 != nullptr);
    GWA3_ASSERT(missing == nullptr);
    GWA3_ASSERT_EQ(Frost::DIALOG_ACCEPT, 0x8101u);
    GWA3_ASSERT_EQ(Frost::DIALOG_FROSTMAW_QUEST, 0x832A01u);
    GWA3_ASSERT_EQ(Frost::QUEST_DIALOG_REPEAT_COUNT, 3);
    GWA3_ASSERT(!Frost::HAS_LEVEL5_ROUTE_SOURCE);
})

GWA3_TEST(frostmaws_blessing_anchor_data_matches_autoit, {
    int count = 0;
    const auto* none = Frost::GetBlessingAnchors(Frost::RouteId::RunSifhallaToJagaMoraine, count);
    GWA3_ASSERT(none == nullptr);
    GWA3_ASSERT_EQ(count, 0);

    const auto* runBlessing = Frost::GetBlessingAnchors(Frost::RouteId::RunJagaMoraineToDungeon, count);
    GWA3_ASSERT_EQ(count, 1);
    GWA3_ASSERT_EQ(runBlessing[0].trigger_index, 0);
    GWA3_ASSERT_EQ(static_cast<int>(runBlessing[0].x), -9068);
    GWA3_ASSERT_EQ(static_cast<int>(runBlessing[0].y), -22735);

    const auto* level3Blessing = Frost::FindBlessingAnchor(Frost::RouteId::Level3, 0);
    const auto* noneAtIndex = Frost::FindBlessingAnchor(Frost::RouteId::Level3, 1);
    GWA3_ASSERT(level3Blessing != nullptr);
    GWA3_ASSERT(noneAtIndex == nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(level3Blessing->x), 18540);
    GWA3_ASSERT_EQ(static_cast<int>(level3Blessing->y), 9962);
})

GWA3_TEST(frostmaws_quest_bootstrap_constants, {
    int dialogCount = 0;
    const auto* dialogs = Frost::GetQuestDialogSequence(dialogCount);
    GWA3_ASSERT_EQ(dialogCount, 2);
    GWA3_ASSERT_EQ(dialogs[0], 0x8101u);
    GWA3_ASSERT_EQ(dialogs[1], 0x832A01u);

    const auto npc = Frost::GetQuestNpcAnchor();
    GWA3_ASSERT_EQ(static_cast<int>(npc.x), 1012);
    GWA3_ASSERT_EQ(static_cast<int>(npc.y), 25505);
    GWA3_ASSERT_EQ(static_cast<int>(npc.search_radius), 1500);

    int entryCount = 0;
    const auto* entry = Frost::GetQuestEntryPath(entryCount);
    GWA3_ASSERT_EQ(entryCount, 3);
    GWA3_ASSERT_EQ(static_cast<int>(entry[0].x), 821);
    GWA3_ASSERT_EQ(static_cast<int>(entry[1].x), 1760);
    GWA3_ASSERT_EQ(static_cast<int>(entry[2].y), 25700);

    int resetCount = 0;
    const auto* reset = Frost::GetQuestResetPath(resetCount);
    GWA3_ASSERT_EQ(resetCount, 2);
    GWA3_ASSERT_EQ(static_cast<int>(reset[0].x), -16549);
    GWA3_ASSERT_EQ(static_cast<int>(reset[1].y), 18500);

    const auto plan = Frost::GetQuestBootstrapPlan();
    GWA3_ASSERT(GWA3::Bot::DungeonQuest::IsValidBootstrapPlan(plan));
    GWA3_ASSERT_EQ(plan.entry_map_id, Frost::MAP_JAGA_MORAINE);
    GWA3_ASSERT_EQ(plan.reset_map_id, Frost::MAP_SIFHALLA);
    GWA3_ASSERT_EQ(plan.target_map_id, Frost::MAP_FROSTMAWS_BURROWS_LVL1);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(plan), 6);
})

GWA3_TEST(frostmaws_waypoint_execution_plan_defaults_to_standard_move, {
    GWA3_ASSERT_EQ(static_cast<int>(Frost::ResolveWaypointBehavior("1")),
                   static_cast<int>(Frost::WaypointBehavior::StandardMove));

    const auto plan = Frost::BuildWaypointExecutionPlan(Frost::RouteId::Level4, 13);
    GWA3_ASSERT(plan.waypoint != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(plan.behavior), static_cast<int>(Frost::WaypointBehavior::StandardMove));
    GWA3_ASSERT_EQ(static_cast<int>(plan.waypoint->x), -13724);

    const auto invalid = Frost::BuildWaypointExecutionPlan(Frost::RouteId::Level4, 99);
    GWA3_ASSERT(invalid.waypoint == nullptr);
})
