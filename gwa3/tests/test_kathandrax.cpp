#include <gwa3/bot/Kathandrax.h>
#include <gwa3/testing/TestFramework.h>

namespace Kath = GWA3::Bot::Kathandrax;

GWA3_TEST(kathandrax_route_definition_counts_and_maps, {
    const auto& run0 = Kath::GetRouteDefinition(Kath::RouteId::RunDoomloreToDalada);
    const auto& run1 = Kath::GetRouteDefinition(Kath::RouteId::RunDaladaToSacnoth);
    const auto& run2 = Kath::GetRouteDefinition(Kath::RouteId::RunSacnothToDungeon);
    const auto& level1 = Kath::GetRouteDefinition(Kath::RouteId::Level1);
    const auto& level2 = Kath::GetRouteDefinition(Kath::RouteId::Level2);
    const auto& level3 = Kath::GetRouteDefinition(Kath::RouteId::Level3);

    GWA3_ASSERT_EQ(run0.map_id, Kath::MAP_DOOMLORE_SHRINE);
    GWA3_ASSERT_EQ(run0.next_map_id, Kath::MAP_DALADA_UPLANDS);
    GWA3_ASSERT_EQ(run0.waypoint_count, 3);

    GWA3_ASSERT_EQ(run1.map_id, Kath::MAP_DALADA_UPLANDS);
    GWA3_ASSERT_EQ(run1.next_map_id, Kath::MAP_SACNOTH_VALLEY);
    GWA3_ASSERT_EQ(run1.waypoint_count, 9);

    GWA3_ASSERT_EQ(run2.map_id, Kath::MAP_SACNOTH_VALLEY);
    GWA3_ASSERT_EQ(run2.next_map_id, Kath::MAP_CATACOMBS_OF_KATHANDRAX_LVL1);
    GWA3_ASSERT_EQ(run2.waypoint_count, 11);

    GWA3_ASSERT_EQ(level1.map_id, Kath::MAP_CATACOMBS_OF_KATHANDRAX_LVL1);
    GWA3_ASSERT_EQ(level1.next_map_id, Kath::MAP_CATACOMBS_OF_KATHANDRAX_LVL2);
    GWA3_ASSERT_EQ(level1.waypoint_count, 20);

    GWA3_ASSERT_EQ(level2.map_id, Kath::MAP_CATACOMBS_OF_KATHANDRAX_LVL2);
    GWA3_ASSERT_EQ(level2.next_map_id, Kath::MAP_CATACOMBS_OF_KATHANDRAX_LVL3);
    GWA3_ASSERT_EQ(level2.waypoint_count, 19);

    GWA3_ASSERT_EQ(level3.map_id, Kath::MAP_CATACOMBS_OF_KATHANDRAX_LVL3);
    GWA3_ASSERT_EQ(level3.next_map_id, Kath::MAP_SACNOTH_VALLEY);
    GWA3_ASSERT_EQ(level3.waypoint_count, 21);
    GWA3_ASSERT_EQ(Kath::QUEST_KATHANDRAXS_CRUSHER, 805u);
    GWA3_ASSERT_EQ(Kath::DIALOG_ACCEPT, 0x8101u);
    GWA3_ASSERT_EQ(Kath::DIALOG_KATHANDRAX_QUEST, 0x832501u);
})

GWA3_TEST(kathandrax_route_lookup_and_blessings_match_autoit, {
    const auto* sacnoth = Kath::FindRouteDefinitionByMapId(Kath::MAP_SACNOTH_VALLEY);
    const auto* none = Kath::FindRouteDefinitionByMapId(Kath::MAP_DOOMLORE_SHRINE);
    GWA3_ASSERT(sacnoth != nullptr);
    GWA3_ASSERT(none != nullptr);

    int count = 0;
    const auto* noBlessing = Kath::GetBlessingAnchors(Kath::RouteId::RunDoomloreToDalada, count);
    GWA3_ASSERT(noBlessing == nullptr);
    GWA3_ASSERT_EQ(count, 0);

    const auto* runBlessing = Kath::GetBlessingAnchors(Kath::RouteId::RunSacnothToDungeon, count);
    GWA3_ASSERT_EQ(count, 1);
    GWA3_ASSERT_EQ(runBlessing[0].trigger_index, 0);
    GWA3_ASSERT_EQ(static_cast<int>(runBlessing[0].x), -10503);
    GWA3_ASSERT_EQ(static_cast<int>(runBlessing[0].y), 19666);

    const auto* level3Blessing = Kath::FindBlessingAnchor(Kath::RouteId::Level3, 0);
    GWA3_ASSERT(level3Blessing != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(level3Blessing->x), -17643);
    GWA3_ASSERT_EQ(static_cast<int>(level3Blessing->y), 9567);
})

GWA3_TEST(kathandrax_waypoint_behavior_matches_port, {
    GWA3_ASSERT_EQ(static_cast<int>(Kath::ResolveWaypointBehavior("Dungeon Key")),
                   static_cast<int>(Kath::WaypointBehavior::PickUpDungeonKey));
    GWA3_ASSERT_EQ(static_cast<int>(Kath::ResolveWaypointBehavior("Boss lock")),
                   static_cast<int>(Kath::WaypointBehavior::DoubleInteract));
    GWA3_ASSERT_EQ(static_cast<int>(Kath::ResolveWaypointBehavior("Boss Lock Checkpoint 2")),
                   static_cast<int>(Kath::WaypointBehavior::StandardMove));

    const auto plan = Kath::BuildWaypointExecutionPlan(Kath::RouteId::Level2, 11);
    GWA3_ASSERT(plan.waypoint != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(plan.behavior), static_cast<int>(Kath::WaypointBehavior::DoubleInteract));

    const auto keyPlan = Kath::BuildWaypointExecutionPlan(Kath::RouteId::Level3, 4);
    GWA3_ASSERT(keyPlan.waypoint != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(keyPlan.behavior), static_cast<int>(Kath::WaypointBehavior::PickUpDungeonKey));
})

GWA3_TEST(kathandrax_quest_bootstrap_matches_autoit, {
    int dialogCount = 0;
    const auto* dialogs = Kath::GetQuestDialogSequence(dialogCount);
    GWA3_ASSERT_EQ(dialogCount, 2);
    GWA3_ASSERT_EQ(dialogs[0], 0x8101u);
    GWA3_ASSERT_EQ(dialogs[1], 0x832501u);

    const auto npc = Kath::GetQuestNpcAnchor();
    GWA3_ASSERT_EQ(static_cast<int>(npc.x), 18329);
    GWA3_ASSERT_EQ(static_cast<int>(npc.y), -18134);
    GWA3_ASSERT_EQ(static_cast<int>(npc.search_radius), 1500);

    int entryCount = 0;
    const auto* entry = Kath::GetQuestEntryPath(entryCount);
    GWA3_ASSERT_EQ(entryCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(entry[0].x), 19324);
    GWA3_ASSERT_EQ(static_cast<int>(entry[0].y), -16150);

    const auto plan = Kath::GetQuestBootstrapPlan();
    GWA3_ASSERT(GWA3::Bot::DungeonQuest::IsValidBootstrapPlan(plan));
    GWA3_ASSERT_EQ(plan.entry_map_id, Kath::MAP_SACNOTH_VALLEY);
    GWA3_ASSERT_EQ(plan.target_map_id, Kath::MAP_CATACOMBS_OF_KATHANDRAX_LVL1);
    GWA3_ASSERT_EQ(plan.reset_map_id, 0u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(plan), 2);

    const auto reward = Kath::GetRewardChestObjective();
    GWA3_ASSERT_EQ(static_cast<int>(reward.staging_point.x), -18);
    GWA3_ASSERT_EQ(static_cast<int>(reward.search_point.y), -583);
    GWA3_ASSERT_EQ(reward.interact_repeats, 2);
    GWA3_ASSERT_EQ(reward.pickup_attempts, 2);
})
