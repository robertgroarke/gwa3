// Consolidated test module generated from small test files.
#include "RragarsMenagerieTestSupport.h"
#include <bots/common/BotFramework.h>
#include <bots/rragars_menagerie/RragarsMenagerieBot.h>
#include <gwa3/testing/TestFramework.h>

// --- tests/test_rragars_blessing_anchor_data_matches_autoit.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_blessing_anchor_data_matches_autoit {

GWA3_TEST(rragars_blessing_anchor_data_matches_autoit, {
    int count = 0;
    const auto* routeBlessings = Rragar::GetBlessingAnchors(Rragar::RouteId::RunSacnothToDungeon, count);
    GWA3_ASSERT_EQ(count, 2);
    GWA3_ASSERT_EQ(routeBlessings[0].trigger_index, 0);
    GWA3_ASSERT_EQ(static_cast<int>(routeBlessings[0].x), -16047);
    GWA3_ASSERT_EQ(static_cast<int>(routeBlessings[0].y), 16691);
    GWA3_ASSERT_EQ(routeBlessings[1].trigger_index, 3);
    GWA3_ASSERT_EQ(static_cast<int>(routeBlessings[1].x), -12558);
    GWA3_ASSERT_EQ(static_cast<int>(routeBlessings[1].y), 221);

    const auto* level3Blessings = Rragar::GetBlessingAnchors(Rragar::RouteId::Level3, count);
    GWA3_ASSERT_EQ(count, 3);
    GWA3_ASSERT_EQ(level3Blessings[1].trigger_index, 5);
    GWA3_ASSERT_EQ(static_cast<int>(level3Blessings[1].x), 4258);
    GWA3_ASSERT_EQ(static_cast<int>(level3Blessings[1].y), -751);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_blessing_anchor_data_matches_autoit

// --- tests/test_rragars_build_waypoint_execution_plan.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_build_waypoint_execution_plan {

GWA3_TEST(rragars_build_waypoint_execution_plan, {
    const auto plan = Rragar::BuildWaypointExecutionPlan(Rragar::RouteId::Level1, 9);
    GWA3_ASSERT(plan.waypoint != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(plan.behavior), static_cast<int>(Rragar::WaypointBehavior::ValidateRetryCheckpoint));
    GWA3_ASSERT(plan.checkpoint_policy != nullptr);
    GWA3_ASSERT_EQ(plan.checkpoint_policy->backtrack_steps, 4);

    const auto standard = Rragar::BuildWaypointExecutionPlan(Rragar::RouteId::Level2, 0);
    GWA3_ASSERT(standard.waypoint != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(standard.behavior), static_cast<int>(Rragar::WaypointBehavior::StandardMove));
    GWA3_ASSERT(standard.checkpoint_policy == nullptr);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_build_waypoint_execution_plan

// --- tests/test_rragars_checkpoint_retry_policy_matches_autoit.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_checkpoint_retry_policy_matches_autoit {

GWA3_TEST(rragars_checkpoint_retry_policy_matches_autoit, {
    int count = 0;
    const auto* policies = Rragar::GetCheckpointPolicies(Rragar::RouteId::Level1, count);
    GWA3_ASSERT_EQ(count, 3);

    GWA3_ASSERT_EQ(static_cast<int>(policies[0].action), static_cast<int>(CheckpointFailureAction::AbortRun));
    GWA3_ASSERT_EQ(static_cast<int>(policies[1].action), static_cast<int>(CheckpointFailureAction::BacktrackRetry));
    GWA3_ASSERT_EQ(policies[1].backtrack_steps, 4);
    GWA3_ASSERT_EQ(policies[1].retry_index, 3);
    GWA3_ASSERT_EQ(static_cast<int>(policies[2].action), static_cast<int>(CheckpointFailureAction::BacktrackRetry));
    GWA3_ASSERT_EQ(policies[2].backtrack_steps, 5);
    GWA3_ASSERT_EQ(policies[2].retry_index, 25);

    const auto* none = Rragar::GetCheckpointPolicies(Rragar::RouteId::Level2, count);
    GWA3_ASSERT_EQ(count, 0);
    GWA3_ASSERT(none == nullptr);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_checkpoint_retry_policy_matches_autoit

// --- tests/test_rragars_find_blessing_anchor_by_nearest_index.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_find_blessing_anchor_by_nearest_index {

GWA3_TEST(rragars_find_blessing_anchor_by_nearest_index, {
    const auto* first = Rragar::FindBlessingAnchor(Rragar::RouteId::Level2, 0);
    const auto* second = Rragar::FindBlessingAnchor(Rragar::RouteId::Level2, 8);
    const auto* none = Rragar::FindBlessingAnchor(Rragar::RouteId::Level2, 7);

    GWA3_ASSERT(first != nullptr);
    GWA3_ASSERT(second != nullptr);
    GWA3_ASSERT(none == nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(first->x), 18111);
    GWA3_ASSERT_EQ(static_cast<int>(second->x), -6826);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_find_blessing_anchor_by_nearest_index

// --- tests/test_rragars_loot_and_reward_objectives_match_autoit.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_loot_and_reward_objectives_match_autoit {

GWA3_TEST(rragars_loot_and_reward_objectives_match_autoit, {
    int count = 0;
    const auto* loot = Rragar::GetLootObjectives(count);
    GWA3_ASSERT_EQ(count, 3);
    GWA3_ASSERT_EQ(static_cast<int>(loot[0].pickup_x), 10292);
    GWA3_ASSERT_EQ(static_cast<int>(loot[1].pickup_x), -11969);
    GWA3_ASSERT_EQ(static_cast<int>(loot[2].pickup_y), -9648);
    GWA3_ASSERT_EQ(loot[0].pickup_retries, 3);

    const auto* level3Key = Rragar::FindLootObjective(Rragar::RouteId::Level3);
    GWA3_ASSERT(level3Key != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(level3Key->pickup_x), 1294);

    const auto reward = Rragar::GetRewardChestObjective();
    GWA3_ASSERT_EQ(static_cast<int>(reward.staging_x), -364);
    GWA3_ASSERT_EQ(static_cast<int>(reward.staging_y), 17572);
    GWA3_ASSERT_EQ(static_cast<int>(reward.chest_x), 130);
    GWA3_ASSERT_EQ(static_cast<int>(reward.chest_y), 17437);
    GWA3_ASSERT_EQ(reward.interact_repeats, 2);
    GWA3_ASSERT_EQ(reward.pickup_attempts, 2);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_loot_and_reward_objectives_match_autoit

// --- tests/test_rragars_menagerie_bot.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_menagerie_bot {

using namespace GWA3::Bot;

GWA3_TEST(rragars_bot_register_sets_config, {
    auto& cfg = GetConfig();
    cfg = {};

    GWA3::Bot::RragarsMenagerieBot::Register();

    GWA3_ASSERT(cfg.hard_mode);
    GWA3_ASSERT_EQ(cfg.target_map_id, 573u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 648u);
    GWA3_ASSERT(cfg.bot_module_name == "RragarsMenagerie");
})
} // namespace GWA3::Tests::Consolidated::test_rragars_menagerie_bot

// --- tests/test_rragars_route_definition_counts_and_maps.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_route_definition_counts_and_maps {

GWA3_TEST(rragars_route_definition_counts_and_maps, {
    const auto& run1 = Rragar::GetRouteDefinition(Rragar::RouteId::RunDaladaToGrothmar);
    const auto& run2 = Rragar::GetRouteDefinition(Rragar::RouteId::RunGrothmarToSacnoth);
    const auto& run3 = Rragar::GetRouteDefinition(Rragar::RouteId::RunSacnothToDungeon);
    const auto& level1 = Rragar::GetRouteDefinition(Rragar::RouteId::Level1);
    const auto& level2 = Rragar::GetRouteDefinition(Rragar::RouteId::Level2);
    const auto& level3 = Rragar::GetRouteDefinition(Rragar::RouteId::Level3);

    GWA3_ASSERT_EQ(run1.map_id, GWA3::MapIds::DALADA_UPLANDS);
    GWA3_ASSERT_EQ(run1.next_map_id, GWA3::MapIds::GROTHMAR_WARDOWNS);
    GWA3_ASSERT_EQ(run1.waypoint_count, 5);

    GWA3_ASSERT_EQ(run2.map_id, GWA3::MapIds::GROTHMAR_WARDOWNS);
    GWA3_ASSERT_EQ(run2.next_map_id, GWA3::MapIds::SACNOTH_VALLEY);
    GWA3_ASSERT_EQ(run2.waypoint_count, 7);

    GWA3_ASSERT_EQ(run3.map_id, GWA3::MapIds::SACNOTH_VALLEY);
    GWA3_ASSERT_EQ(run3.next_map_id, GWA3::MapIds::RRAGARS_MENAGERIE_LVL1);
    GWA3_ASSERT_EQ(run3.waypoint_count, 8);

    GWA3_ASSERT_EQ(level1.map_id, GWA3::MapIds::RRAGARS_MENAGERIE_LVL1);
    GWA3_ASSERT_EQ(level1.next_map_id, GWA3::MapIds::RRAGARS_MENAGERIE_LVL2);
    GWA3_ASSERT_EQ(level1.waypoint_count, 35);

    GWA3_ASSERT_EQ(level2.map_id, GWA3::MapIds::RRAGARS_MENAGERIE_LVL2);
    GWA3_ASSERT_EQ(level2.next_map_id, GWA3::MapIds::RRAGARS_MENAGERIE_LVL3);
    GWA3_ASSERT_EQ(level2.waypoint_count, 25);

    GWA3_ASSERT_EQ(level3.map_id, GWA3::MapIds::RRAGARS_MENAGERIE_LVL3);
    GWA3_ASSERT_EQ(level3.next_map_id, GWA3::MapIds::DOOMLORE_SHRINE);
    GWA3_ASSERT_EQ(level3.waypoint_count, 22);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_route_definition_counts_and_maps

// --- tests/test_rragars_route_key_labels_match_autoit_port.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_route_key_labels_match_autoit_port {

GWA3_TEST(rragars_route_key_labels_match_autoit_port, {
    const auto& level1 = Rragar::GetRouteDefinition(Rragar::RouteId::Level1);
    const auto& level2 = Rragar::GetRouteDefinition(Rragar::RouteId::Level2);
    const auto& level3 = Rragar::GetRouteDefinition(Rragar::RouteId::Level3);

    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level1.waypoints[2].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::QuestCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level1.waypoints[9].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::BlastDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level1.waypoints[34].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::BossLock));

    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level2.waypoints[12].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::DungeonKey));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level2.waypoints[22].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::BossLock));

    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level3.waypoints[6].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::Keg));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level3.waypoints[7].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::BlastDoor));
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::DungeonRoute::ClassifyWaypointLabel(level3.waypoints[19].label)),
                   static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::BossLock));
})

} // namespace GWA3::Tests::Consolidated::test_rragars_route_key_labels_match_autoit_port

// --- tests/test_rragars_route_lookup_by_map_id.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_route_lookup_by_map_id {

GWA3_TEST(rragars_route_lookup_by_map_id, {
    const auto* grothmar = Rragar::FindRouteDefinitionByMapId(GWA3::MapIds::GROTHMAR_WARDOWNS);
    const auto* sacnoth = Rragar::FindRouteDefinitionByMapId(GWA3::MapIds::SACNOTH_VALLEY);
    const auto* none = Rragar::FindRouteDefinitionByMapId(GWA3::MapIds::DOOMLORE_SHRINE);

    GWA3_ASSERT(grothmar != nullptr);
    GWA3_ASSERT(sacnoth != nullptr);
    GWA3_ASSERT(none == nullptr);
    GWA3_ASSERT_EQ(grothmar->next_map_id, GWA3::MapIds::SACNOTH_VALLEY);
    GWA3_ASSERT_EQ(sacnoth->next_map_id, GWA3::MapIds::RRAGARS_MENAGERIE_LVL1);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_route_lookup_by_map_id

// --- tests/test_rragars_waypoint_behavior_matches_autoit_switch.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_waypoint_behavior_matches_autoit_switch {

GWA3_TEST(rragars_waypoint_behavior_matches_autoit_switch, {
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("Keg")),
                   static_cast<int>(Rragar::WaypointBehavior::PickUpKeg));
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("Blast Door")),
                   static_cast<int>(Rragar::WaypointBehavior::DropKegAtBlastDoor));
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("Quest Checkpoint")),
                   static_cast<int>(Rragar::WaypointBehavior::ValidateQuestCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("Blast Door Checkpoint 1")),
                   static_cast<int>(Rragar::WaypointBehavior::ValidateRetryCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("Dungeon Key")),
                   static_cast<int>(Rragar::WaypointBehavior::PickUpDungeonKey));
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("Boss lock")),
                   static_cast<int>(Rragar::WaypointBehavior::DoubleInteract));
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("Chest")),
                   static_cast<int>(Rragar::WaypointBehavior::DoubleInteract));
    GWA3_ASSERT_EQ(static_cast<int>(Rragar::ResolveWaypointBehavior("17")),
                   static_cast<int>(Rragar::WaypointBehavior::StandardMove));
})

} // namespace GWA3::Tests::Consolidated::test_rragars_waypoint_behavior_matches_autoit_switch

// --- tests/test_rragars_zone_transition_points_match_autoit.cpp ---
namespace GWA3::Tests::Consolidated::test_rragars_zone_transition_points_match_autoit {

GWA3_TEST(rragars_zone_transition_points_match_autoit, {
    const auto* dalada = Rragar::FindZoneTransitionPoint(Rragar::RouteId::RunDaladaToGrothmar);
    const auto* grothmar = Rragar::FindZoneTransitionPoint(Rragar::RouteId::RunGrothmarToSacnoth);
    const auto* sacnoth = Rragar::FindZoneTransitionPoint(Rragar::RouteId::RunSacnothToDungeon);
    const auto* level1 = Rragar::FindZoneTransitionPoint(Rragar::RouteId::Level1);
    const auto* level2 = Rragar::FindZoneTransitionPoint(Rragar::RouteId::Level2);
    const auto* none = Rragar::FindZoneTransitionPoint(Rragar::RouteId::Level3);

    GWA3_ASSERT(dalada != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(dalada->x), -20600);
    GWA3_ASSERT_EQ(static_cast<int>(dalada->y), 430);

    GWA3_ASSERT(grothmar != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(grothmar->x), 23320);
    GWA3_ASSERT_EQ(static_cast<int>(grothmar->y), -13476);

    GWA3_ASSERT(sacnoth != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(sacnoth->x), -19600);
    GWA3_ASSERT_EQ(static_cast<int>(sacnoth->y), -15945);

    GWA3_ASSERT(level1 != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(level1->x), -17934);
    GWA3_ASSERT_EQ(static_cast<int>(level1->y), -439);

    GWA3_ASSERT(level2 != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(level2->x), 11560);
    GWA3_ASSERT_EQ(static_cast<int>(level2->y), -19000);

    GWA3_ASSERT(none == nullptr);
})

} // namespace GWA3::Tests::Consolidated::test_rragars_zone_transition_points_match_autoit
