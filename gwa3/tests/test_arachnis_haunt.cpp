#include <gwa3/bot/ArachnisHaunt.h>
#include <gwa3/testing/TestFramework.h>

namespace Arachnis = GWA3::Bot::ArachnisHaunt;

GWA3_TEST(arachnis_stage_and_route_definitions_match_source, {
    const auto& run = Arachnis::GetStageDefinition(Arachnis::StageId::RunRataSumToMagusStones);
    const auto& magus = Arachnis::GetStageDefinition(Arachnis::StageId::RunMagusToDungeon);
    const auto& level1 = Arachnis::GetStageDefinition(Arachnis::StageId::Level1);
    const auto& level2 = Arachnis::GetStageDefinition(Arachnis::StageId::Level2);

    GWA3_ASSERT_EQ(run.map_id, Arachnis::MAP_RATA_SUM);
    GWA3_ASSERT_EQ(run.next_map_id, Arachnis::MAP_MAGUS_STONES);
    GWA3_ASSERT_EQ(run.waypoint_count, 3);

    GWA3_ASSERT_EQ(magus.map_id, Arachnis::MAP_MAGUS_STONES);
    GWA3_ASSERT_EQ(magus.next_map_id, Arachnis::MAP_ARACHNIS_HAUNT_LVL1);
    GWA3_ASSERT_EQ(magus.waypoint_count, 16);

    GWA3_ASSERT_EQ(level1.map_id, Arachnis::MAP_ARACHNIS_HAUNT_LVL1);
    GWA3_ASSERT_EQ(level1.next_map_id, Arachnis::MAP_ARACHNIS_HAUNT_LVL2);
    GWA3_ASSERT_EQ(level1.waypoint_count, 16);

    GWA3_ASSERT_EQ(level2.map_id, Arachnis::MAP_ARACHNIS_HAUNT_LVL2);
    GWA3_ASSERT_EQ(level2.next_map_id, Arachnis::MAP_MAGUS_STONES);
    GWA3_ASSERT_EQ(level2.waypoint_count, 16);

    const auto* byMap = Arachnis::FindStageDefinitionByMapId(Arachnis::MAP_ARACHNIS_HAUNT_LVL2);
    GWA3_ASSERT(byMap != nullptr);
    GWA3_ASSERT_EQ(byMap->next_map_id, Arachnis::MAP_MAGUS_STONES);

    const auto& phase2 = Arachnis::GetRouteDefinition(Arachnis::RouteId::Level1Phase2);
    const auto& exit = Arachnis::GetRouteDefinition(Arachnis::RouteId::Level1Exit);
    const auto& boss = Arachnis::GetRouteDefinition(Arachnis::RouteId::Level2Phase3);
    GWA3_ASSERT_EQ(phase2.waypoint_count, 19);
    GWA3_ASSERT_EQ(exit.next_map_id, Arachnis::MAP_ARACHNIS_HAUNT_LVL2);
    GWA3_ASSERT_EQ(boss.waypoint_count, 29);
})

GWA3_TEST(arachnis_quest_cycle_and_dialogs_match_source, {
    const auto plan = Arachnis::GetQuestCyclePlan();
    GWA3_ASSERT(GWA3::Bot::DungeonQuest::IsValidQuestCyclePlan(plan));
    GWA3_ASSERT_EQ(plan.start_map_id, Arachnis::MAP_MAGUS_STONES);
    GWA3_ASSERT_EQ(plan.dungeon_map_id, Arachnis::MAP_ARACHNIS_HAUNT_LVL1);
    GWA3_ASSERT_EQ(static_cast<int>(plan.npc.x), -10150);
    GWA3_ASSERT_EQ(static_cast<int>(plan.npc.y), -17087);
    GWA3_ASSERT_EQ(plan.reward_dialog.dialog_ids[0], Arachnis::DIALOG_REWARD_ACCEPT);
    GWA3_ASSERT_EQ(plan.reward_dialog.dialog_ids[1], Arachnis::DIALOG_ARACHNIS_REWARD);
    GWA3_ASSERT_EQ(plan.accept_dialog.dialog_ids[1], Arachnis::DIALOG_ARACHNIS_QUEST);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(plan.reward_dialog), 4);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(plan.accept_dialog), 6);
    GWA3_ASSERT_EQ(plan.approach_path_count, 2);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_entry.x), -11570);
    GWA3_ASSERT_EQ(static_cast<int>(plan.dungeon_exit.y), 20000);

    int returnCount = 0;
    const auto* returnPath = Arachnis::GetQuestReturnPath(returnCount);
    GWA3_ASSERT_EQ(returnCount, 2);
    GWA3_ASSERT_EQ(static_cast<int>(returnPath[0].x), -11392);

    const auto completion = Arachnis::GetCompletionDialogPlan();
    GWA3_ASSERT_EQ(completion.dialog_ids[0], Arachnis::DIALOG_ACCEPT);
    GWA3_ASSERT_EQ(completion.dialog_ids[1], Arachnis::DIALOG_ARACHNIS_COMPLETION);
    GWA3_ASSERT_EQ(GWA3::Bot::DungeonQuest::GetExpandedDialogCount(completion), 4);
})

GWA3_TEST(arachnis_blessing_anchors_match_source, {
    int count = 0;
    const auto* none = Arachnis::GetBlessingAnchors(Arachnis::StageId::RunRataSumToMagusStones, count);
    GWA3_ASSERT(none == nullptr);
    GWA3_ASSERT_EQ(count, 0);

    const auto* runBlessing = Arachnis::GetBlessingAnchors(Arachnis::StageId::RunMagusToDungeon, count);
    GWA3_ASSERT_EQ(count, 1);
    GWA3_ASSERT_EQ(static_cast<int>(runBlessing[0].x), 14862);
    GWA3_ASSERT_EQ(static_cast<int>(runBlessing[0].y), 13173);

    const auto* level2Blessing = Arachnis::FindBlessingAnchor(Arachnis::StageId::Level2, 8);
    GWA3_ASSERT(level2Blessing != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(level2Blessing->x), -6755);

    const auto* phase3Blessing = Arachnis::FindBlessingAnchor(Arachnis::RouteId::Level1Phase3, 0);
    GWA3_ASSERT(phase3Blessing != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(phase3Blessing->x), -16066);

    const auto* phase2Blessing = Arachnis::FindBlessingAnchor(Arachnis::RouteId::Level2Phase2, 5);
    GWA3_ASSERT(phase2Blessing != nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(phase2Blessing->y), 15568);
})

GWA3_TEST(arachnis_all_runtime_routes_use_aggro_traversal, {
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::RunRataSumToMagusStones));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::RunMagusToDungeon));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level1Phase1));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level1Phase2));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level1Phase3));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level1Phase4));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level1Exit));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level2Phase1Approach));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level2Phase1Transit));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level2Phase2));
    GWA3_ASSERT(Arachnis::UsesAggroTraversal(Arachnis::RouteId::Level2Phase3));
})

GWA3_TEST(arachnis_objectives_match_source, {
    int flameCount = 0;
    const auto* flames = Arachnis::GetFlameStaffObjectives(flameCount);
    GWA3_ASSERT_EQ(flameCount, 4);
    GWA3_ASSERT_EQ(static_cast<int>(flames[0].pickup_point.x), 12938);
    GWA3_ASSERT_EQ(flames[1].web_clear_path_count, 3);
    GWA3_ASSERT_EQ(static_cast<int>(flames[2].drop_point.x), -930);
    GWA3_ASSERT_EQ(static_cast<int>(flames[3].pickup_point.x), -4020);

    int eggCount = 0;
    const auto* eggs = Arachnis::GetSpiderEggClusters(eggCount);
    GWA3_ASSERT_EQ(eggCount, 6);
    GWA3_ASSERT_EQ(eggs[0].egg_count, 5);
    GWA3_ASSERT_EQ(eggs[5].egg_count, 3);

    const auto key = Arachnis::GetLevel1KeyObjective();
    GWA3_ASSERT_EQ(key.pickup_attempts, 4);
    GWA3_ASSERT_EQ(static_cast<int>(key.pickup_point.x), 6);
    GWA3_ASSERT_EQ(static_cast<int>(key.drop_point.y), 16439);

    const auto door = Arachnis::GetLevel1ExitDoorObjective();
    GWA3_ASSERT_EQ(static_cast<int>(door.interact_point.x), 2065);
    GWA3_ASSERT_EQ(door.interact_repeats, 6);
    GWA3_ASSERT_EQ(static_cast<int>(door.zone_point.y), 20200);

    const auto reward = Arachnis::GetRewardChestObjective();
    GWA3_ASSERT_EQ(static_cast<int>(reward.chest_point.x), -17131);
    GWA3_ASSERT_EQ(reward.chest_interact_passes, 2);
    GWA3_ASSERT_EQ(reward.chest_loot_attempts, 1);
    GWA3_ASSERT_EQ(static_cast<int>(reward.completion_npc.search_radius), 5000);
    GWA3_ASSERT_EQ(static_cast<int>(reward.post_completion_point.x), -14021);
})
