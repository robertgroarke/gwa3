// Froggy wipe recovery and map transition helpers. Included by FroggyHM.cpp
// before waypoint traversal so checkpoint recovery types are visible there.

#include "FroggyHMWipeRecovery.h"

static DungeonEntryRecovery::TransitionPlan MakeFroggyReturnToSparkflyPlan(const char* label) {
    DungeonEntryRecovery::TransitionPlan plan;
    plan.required_start_map_id = MapIds::BOGROOT_GROWTHS_LVL1;
    plan.target_map_id = MapIds::SPARKFLY_SWAMP;
    plan.stage = {BOGROOT_RETURN_TO_SPARKFLY_STAGE.x, BOGROOT_RETURN_TO_SPARKFLY_STAGE.y};
    plan.push = {BOGROOT_RETURN_TO_SPARKFLY_PUSH.x, BOGROOT_RETURN_TO_SPARKFLY_PUSH.y};
    plan.stage_threshold = MAP_TRANSITION_STAGE_THRESHOLD;
    plan.pre_push_delay_ms = MAP_TRANSITION_RETURN_PUSH_DELAY_MS;
    plan.ready_timeout_ms = MAP_TRANSITION_READY_TIMEOUT_MS;
    plan.push_timeout_ms = MAP_TRANSITION_PUSH_TIMEOUT_MS;
    plan.push_interval_ms = MAP_TRANSITION_PUSH_INTERVAL_MS;
    plan.move_to_point = &MoveToAndWait;
    plan.log_prefix = "Froggy";
    plan.label = label;
    return plan;
}

static DungeonEntryRecovery::DungeonEntryPlan MakeFroggyBogrootEntryPlan(const char* label) {
    DungeonEntryRecovery::DungeonEntryPlan plan;
    plan.bootstrap = GetEntryBootstrapPlan();
    plan.path_threshold = DungeonNavigation::MOVE_TO_DEFAULT_THRESHOLD;
    plan.route_dwell_ms = MAP_TRANSITION_ENTRY_ROUTE_DWELL_MS;
    plan.ready_timeout_ms = MAP_TRANSITION_READY_TIMEOUT_MS;
    plan.push_timeout_ms = MAP_TRANSITION_PUSH_TIMEOUT_MS;
    plan.push_interval_ms = MAP_TRANSITION_PUSH_INTERVAL_MS;
    plan.log_prefix = "Froggy";
    plan.label = label;
    return plan;
}

static DungeonEntryRecovery::DialogResetBouncePlan MakeTekksDialogResetBouncePlan() {
    DungeonEntryRecovery::DialogResetBouncePlan plan;
    plan.required_start_map_id = MapIds::SPARKFLY_SWAMP;
    plan.enter = MakeFroggyBogrootEntryPlan("Tekks dialog reset enter");
    plan.return_to_quest_map = MakeFroggyReturnToSparkflyPlan("Tekks dialog reset return");
    plan.settle_ms = TEKKS_DIALOG_RESET_SETTLE_MS;
    plan.log_prefix = "Froggy";
    plan.label = "Tekks dialog reset bounce";
    return plan;
}

static DungeonEntryRecovery::QuestMapApproachPlan MakeFroggyTekksApproachPlan() {
    DungeonEntryRecovery::QuestMapApproachPlan plan;
    plan.quest_map_id = MapIds::SPARKFLY_SWAMP;
    plan.near_side_anchor_a = {SPARKFLY_TEKKS_STAGE.x, SPARKFLY_TEKKS_STAGE.y};
    plan.near_side_anchor_b = {SPARKFLY_DUNGEON_ENTRY_STAGE.x, SPARKFLY_DUNGEON_ENTRY_STAGE.y};
    plan.quest_stage = {SPARKFLY_TEKKS_STAGE.x, SPARKFLY_TEKKS_STAGE.y};
    plan.quest_search = {SPARKFLY_TEKKS_SEARCH.x, SPARKFLY_TEKKS_SEARCH.y};
    plan.entry_stage = {SPARKFLY_DUNGEON_ENTRY_STAGE.x, SPARKFLY_DUNGEON_ENTRY_STAGE.y};
    plan.near_side_threshold = SPARKFLY_DUNGEON_SIDE_THRESHOLD;
    plan.short_move_threshold = SPARKFLY_TEKKS_SHORT_MOVE_THRESHOLD;
    plan.full_move_threshold = SPARKFLY_TEKKS_FULL_MOVE_THRESHOLD;
    plan.short_ready_threshold = 1500.0f;
    plan.full_ready_threshold = 2500.0f;
    plan.full_route = SPARKFLY_TO_DUNGEON;
    plan.full_route_count = SPARKFLY_TO_DUNGEON_COUNT;
    plan.full_route_attempts = 2;
    plan.death_recovery_timeout_ms = 120000u;
    plan.death_recovery_poll_ms = 500u;
    plan.death_recovery_settle_ms = 1000u;
    plan.move_to_point = &MoveToAndWait;
    plan.follow_waypoints = &FollowWaypoints;
    plan.is_dead = &IsDead;
    plan.log_prefix = "Froggy";
    plan.label = "Sparkfly Tekks";
    return plan;
}

static bool IsSparkflyTekksApproachReady(float maxDist) {
    return DungeonEntryRecovery::IsQuestMapApproachReady(MakeFroggyTekksApproachPlan(), maxDist);
}

static bool MoveToTekksFromSparkflyCurrentSide() {
    return DungeonEntryRecovery::MoveToQuestGiverFromCurrentQuestMapSide(MakeFroggyTekksApproachPlan());
}

static bool EnterBogrootFromSparkfly() {
    return DungeonEntryRecovery::EnterDungeonFromQuestMap(
        MakeFroggyBogrootEntryPlan("Froggy transition"));
}

static void ResetTekksQuestEntryFailures(const char* reason) {
    DungeonEntryRecovery::ResetEntryFailureTracker(s_tekksQuestEntryFailures, reason, "Froggy");
}

static bool RecordTekksQuestEntryFailureAndMaybeResetDialog(const char* context) {
    return DungeonEntryRecovery::RecordEntryFailureAndMaybeResetDialog(
        s_tekksQuestEntryFailures,
        MakeTekksDialogResetBouncePlan(),
        context);
}
