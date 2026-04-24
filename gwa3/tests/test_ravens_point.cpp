#include <gwa3/bot/RavensPoint.h>
#include <gwa3/testing/TestFramework.h>

namespace Ravens = GWA3::Bot::RavensPoint;

GWA3_TEST(ravens_dispatch_routes_match_source, {
    const auto& run = Ravens::GetDispatchRouteDefinition(Ravens::StageId::RunOlafsteadToVarajarFells);
    const auto& varajar = Ravens::GetDispatchRouteDefinition(Ravens::StageId::VarajarFells);
    const auto& level1 = Ravens::GetDispatchRouteDefinition(Ravens::StageId::Level1);
    const auto& level2 = Ravens::GetDispatchRouteDefinition(Ravens::StageId::Level2);
    const auto& level3 = Ravens::GetDispatchRouteDefinition(Ravens::StageId::Level3);

    GWA3_ASSERT_EQ(run.map_id, Ravens::MAP_OLAFSTEAD);
    GWA3_ASSERT_EQ(run.next_map_id, Ravens::MAP_VARAJAR_FELLS_1);
    GWA3_ASSERT_EQ(run.waypoint_count, 2);

    GWA3_ASSERT_EQ(varajar.map_id, Ravens::MAP_VARAJAR_FELLS_1);
    GWA3_ASSERT_EQ(varajar.next_map_id, Ravens::MAP_RAVENS_POINT_LVL1);
    GWA3_ASSERT_EQ(varajar.waypoint_count, 4);

    GWA3_ASSERT_EQ(level1.map_id, Ravens::MAP_RAVENS_POINT_LVL1);
    GWA3_ASSERT_EQ(level1.next_map_id, Ravens::MAP_RAVENS_POINT_LVL2);
    GWA3_ASSERT_EQ(level1.waypoint_count, 5);

    GWA3_ASSERT_EQ(level2.map_id, Ravens::MAP_RAVENS_POINT_LVL2);
    GWA3_ASSERT_EQ(level2.next_map_id, Ravens::MAP_RAVENS_POINT_LVL3);
    GWA3_ASSERT_EQ(level2.waypoint_count, 12);

    GWA3_ASSERT_EQ(level3.map_id, Ravens::MAP_RAVENS_POINT_LVL3);
    GWA3_ASSERT_EQ(level3.next_map_id, Ravens::MAP_VARAJAR_FELLS_1);
    GWA3_ASSERT_EQ(level3.waypoint_count, 8);
})

GWA3_TEST(ravens_quest_cycle_matches_autoit, {
    const auto* dispatch = Ravens::FindDispatchRouteDefinitionByMapId(Ravens::MAP_VARAJAR_FELLS_1);
    GWA3_ASSERT(dispatch != nullptr);

    const auto plan = Ravens::GetQuestCyclePlan();
    GWA3_ASSERT(GWA3::Bot::DungeonQuest::IsValidQuestCyclePlan(plan));
    GWA3_ASSERT_EQ(Ravens::QUEST_RAVENS_POINT, 0x343u);
    GWA3_ASSERT_EQ(static_cast<int>(plan.npc.x), -15526);
    GWA3_ASSERT_EQ(static_cast<int>(plan.npc.y), 8811);
    GWA3_ASSERT_EQ(plan.reward_dialog.dialog_ids[0], 0x8101u);
    GWA3_ASSERT_EQ(plan.reward_dialog.dialog_ids[1], 0x834307u);
    GWA3_ASSERT_EQ(plan.accept_dialog.dialog_ids[1], 0x834301u);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(plan.reward_dialog), 4);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(plan.accept_dialog), 6);
    GWA3_ASSERT_EQ(plan.approach_path_count, 4);
    GWA3_ASSERT_EQ(static_cast<int>(plan.approach_path[3].x), -24788);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_entry.x), -26100);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_exit.y), -14350);

    const auto& approachRoute = Ravens::GetRouteDefinition(Ravens::RouteId::QuestApproach);
    GWA3_ASSERT_EQ(approachRoute.waypoint_count, 4);
    GWA3_ASSERT_EQ(static_cast<int>(approachRoute.waypoints[0].fight_range), 900);
    GWA3_ASSERT_EQ(static_cast<int>(approachRoute.waypoints[1].fight_range), 900);

    const auto& returnRoute = Ravens::GetRouteDefinition(Ravens::RouteId::QuestReturn);
    GWA3_ASSERT_EQ(returnRoute.waypoint_count, 4);
    GWA3_ASSERT_EQ(static_cast<int>(returnRoute.waypoints[0].fight_range), 900);
})

GWA3_TEST(ravens_aggro_route_selection_matches_dungeon_stages, {
    GWA3_ASSERT(Ravens::UsesAggroTraversal(Ravens::RouteId::RunVarajarToBlessing));
    GWA3_ASSERT(Ravens::UsesAggroTraversal(Ravens::RouteId::QuestApproach));
    GWA3_ASSERT(Ravens::UsesAggroTraversal(Ravens::RouteId::QuestReturn));
    GWA3_ASSERT(Ravens::UsesAggroTraversal(Ravens::RouteId::Level1Torch1));
    GWA3_ASSERT(Ravens::UsesAggroTraversal(Ravens::RouteId::Level2Door));
    GWA3_ASSERT(Ravens::UsesAggroTraversal(Ravens::RouteId::Level3BossLoop));
})

GWA3_TEST(ravens_blessings_and_doors_match_source, {
    int count = 0;
    const auto* routeBlessing = Ravens::GetBlessingAnchors(Ravens::RouteId::RunVarajarToBlessing, count);
    GWA3_ASSERT_EQ(count, 1);
    GWA3_ASSERT_EQ(static_cast<int>(routeBlessing[0].x), -2034);
    GWA3_ASSERT_EQ(static_cast<int>(routeBlessing[0].y), -4512);

    const auto* torchBlessing = Ravens::FindBlessingAnchor(Ravens::RouteId::Level1Torch1, 0);
    GWA3_ASSERT(torchBlessing != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(torchBlessing->x), -17536);

    const auto* door = Ravens::FindDoorObjective(Ravens::RouteId::Level1DoorKey);
    GWA3_ASSERT(door != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(door->interact_point.x), -15612);
    GWA3_ASSERT_EQ(door->interact_repeats, 6);
    GWA3_ASSERT_EQ(static_cast<int>(door->resume_point.y), 8184);

    const auto* loopDoor = Ravens::FindDoorObjective(Ravens::RouteId::Level2Door);
    GWA3_ASSERT(loopDoor != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(loopDoor->interact_point.x), 3686);
    GWA3_ASSERT_EQ(loopDoor->interact_repeats, 1);
})

GWA3_TEST(ravens_torch_and_loot_objectives_match_source, {
    int count = 0;
    const auto* torches = Ravens::GetTorchObjectives(count);
    GWA3_ASSERT_EQ(count, 5);
    GWA3_ASSERT_EQ(static_cast<int>(torches[0].chest.x), -6462);
    GWA3_ASSERT_EQ(torches[0].brazier_count, 3);
    GWA3_ASSERT_EQ(static_cast<int>(torches[1].transit_points[0].x), -6862);
    GWA3_ASSERT_EQ(static_cast<int>(torches[2].resume_point.x), 11541);
    GWA3_ASSERT_EQ(torches[3].transit_point_count, 1);
    GWA3_ASSERT_EQ(static_cast<int>(torches[4].resume_point.x), -8521);

    const auto* key = Ravens::FindLootObjective(Ravens::RouteId::Level1DoorKey);
    GWA3_ASSERT(key != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(key->pickup_point.x), -6174);
    GWA3_ASSERT_EQ(key->pickup_retries, 3);

    const auto* bossKey = Ravens::FindLootObjective(Ravens::RouteId::Level2BossKey);
    GWA3_ASSERT(bossKey != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(bossKey->pickup_point.x), static_cast<int>(-129581.0f));
})

GWA3_TEST(ravens_routes_and_reward_chest_match_source, {
    const auto& lvl1Door = Ravens::GetRouteDefinition(Ravens::RouteId::Level1DoorKey);
    const auto& lvl2Door = Ravens::GetRouteDefinition(Ravens::RouteId::Level2Door);
    const auto& lvl3Loop = Ravens::GetRouteDefinition(Ravens::RouteId::Level3BossLoop);

    GWA3_ASSERT_EQ(lvl1Door.waypoint_count, 9);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::DungeonRoute::ClassifyWaypointLabel(lvl1Door.waypoints[2].label)),
                   static_cast<int>(GWA3::Bot::DungeonRoute::WaypointLabelKind::DungeonKey));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::DungeonRoute::ClassifyWaypointLabel(lvl1Door.waypoints[8].label)),
                   static_cast<int>(GWA3::Bot::DungeonRoute::WaypointLabelKind::BossLock));
    GWA3_ASSERT_EQ(lvl2Door.waypoint_count, 8);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::Bot::DungeonRoute::ClassifyWaypointLabel(lvl2Door.waypoints[7].label)),
                   static_cast<int>(GWA3::Bot::DungeonRoute::WaypointLabelKind::BossLock));
    GWA3_ASSERT_EQ(lvl3Loop.waypoint_count, 6);

    const auto reward = Ravens::GetRewardChestObjective();
    GWA3_ASSERT_EQ(static_cast<int>(reward.staging_point.x), 14876);
    GWA3_ASSERT_EQ(static_cast<int>(reward.search_point.y), 9604);
    GWA3_ASSERT_EQ(reward.interact_repeats, 2);
    GWA3_ASSERT_EQ(reward.pickup_attempts, 2);
})
