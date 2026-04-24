#include <gwa3/bot/DungeonRoute.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot::DungeonRoute;

GWA3_TEST(dungeon_route_numeric_label_parse, {
    int ordinal = -1;
    GWA3_ASSERT(TryParseWaypointOrdinal("17", ordinal));
    GWA3_ASSERT_EQ(ordinal, 17);
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("17")), static_cast<int>(WaypointLabelKind::Numeric));
})

GWA3_TEST(dungeon_route_special_label_classification, {
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Blessing")), static_cast<int>(WaypointLabelKind::Blessing));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Lvl1 to Lvl2")), static_cast<int>(WaypointLabelKind::LevelTransition));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Dungeon Key")), static_cast<int>(WaypointLabelKind::DungeonKey));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Dungeon Door")), static_cast<int>(WaypointLabelKind::DungeonDoor));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Dungeon Door Checkpoint")), static_cast<int>(WaypointLabelKind::DungeonDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Quest Door Checkpoint")), static_cast<int>(WaypointLabelKind::QuestDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Boss 8")), static_cast<int>(WaypointLabelKind::Boss));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Boss Lock Checkpoint 2")), static_cast<int>(WaypointLabelKind::BossLockCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Chest")), static_cast<int>(WaypointLabelKind::Chest));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Signpost")), static_cast<int>(WaypointLabelKind::Signpost));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Keg")), static_cast<int>(WaypointLabelKind::Keg));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Blast Door")), static_cast<int>(WaypointLabelKind::BlastDoor));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Blast Door Checkpoint A")), static_cast<int>(WaypointLabelKind::BlastDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Asura Flame Staff")), static_cast<int>(WaypointLabelKind::AsuraFlameStaff));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Staff Check")), static_cast<int>(WaypointLabelKind::StaffCheck));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Spider Eggs 2")), static_cast<int>(WaypointLabelKind::SpiderEggs));
})

GWA3_TEST(dungeon_route_checkpoint_label_detection, {
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::DungeonDoorCheckpoint));
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::QuestDoorCheckpoint));
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::BossLockCheckpoint));
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::BlastDoorCheckpoint));
    GWA3_ASSERT(!IsCheckpointWaypointLabel(WaypointLabelKind::DungeonDoor));
})

GWA3_TEST(dungeon_route_nearest_waypoint_index, {
    const Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3_ASSERT_EQ(FindNearestWaypointIndex(waypoints, 3, 20.0f, 10.0f), 0);
    GWA3_ASSERT_EQ(FindNearestWaypointIndex(waypoints, 3, 140.0f, 0.0f), 1);
    GWA3_ASSERT_EQ(FindNearestWaypointIndex(waypoints, 3, 280.0f, 5.0f), 2);
})

GWA3_TEST(dungeon_route_generic_restart_and_stuck_backtrack, {
    GWA3_ASSERT_EQ(ComputeGenericRestartIndex(5, 2), 3);
    GWA3_ASSERT_EQ(ComputeGenericRestartIndex(1, 2), 0);
    GWA3_ASSERT_EQ(ComputeStuckBacktrackIndex(4, 1), 3);
    GWA3_ASSERT_EQ(ComputeStuckBacktrackIndex(0, 1), 0);
    GWA3_ASSERT(!ShouldTriggerStuckBacktrack(4, 5));
    GWA3_ASSERT(ShouldTriggerStuckBacktrack(5, 5));
})

GWA3_TEST(dungeon_route_restart_rules_apply_by_numeric_label, {
    const Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "2"},
        {200.0f, 0.0f, 0.0f, "3"},
        {300.0f, 0.0f, 0.0f, "4"},
        {400.0f, 0.0f, 0.0f, "5"},
        {500.0f, 0.0f, 0.0f, "6"},
    };
    const RestartRule rules[] = {
        {1, 3, 1},
        {4, 6, 3},
    };

    GWA3_ASSERT_EQ(ComputeRestartIndexFromRules(waypoints, 6, 1, rules, 2, 2), 1);
    GWA3_ASSERT_EQ(ComputeRestartIndexFromRules(waypoints, 6, 4, rules, 2, 2), 3);
})

GWA3_TEST(dungeon_route_restart_rules_fall_back_for_non_numeric_labels, {
    const Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "Keg"},
        {200.0f, 0.0f, 0.0f, "3"},
    };
    const RestartRule rules[] = {
        {1, 3, 1},
    };

    GWA3_ASSERT_EQ(ComputeRestartIndexFromRules(waypoints, 3, 1, rules, 1, 2), 0);
})
