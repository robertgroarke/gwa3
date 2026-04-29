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

static bool IsNearSparkflyDungeonSide() {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f || !MapMgr::GetIsMapLoaded() || MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
        return false;
    }

    const float distToTekksStage =
        AgentMgr::GetDistance(me->x, me->y, SPARKFLY_TEKKS_STAGE.x, SPARKFLY_TEKKS_STAGE.y);
    const float distToDungeonStage =
        AgentMgr::GetDistance(me->x, me->y, SPARKFLY_DUNGEON_ENTRY_STAGE.x, SPARKFLY_DUNGEON_ENTRY_STAGE.y);
    return distToTekksStage <= SPARKFLY_DUNGEON_SIDE_THRESHOLD ||
           distToDungeonStage <= SPARKFLY_DUNGEON_SIDE_THRESHOLD;
}

static bool IsSparkflyTekksApproachReady(float maxDist) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f || !MapMgr::GetIsMapLoaded() ||
        MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
        return false;
    }

    return IsNearSparkflyDungeonSide() ||
           AgentMgr::GetDistance(me->x, me->y, SPARKFLY_TEKKS_STAGE.x, SPARKFLY_TEKKS_STAGE.y) <= maxDist ||
           AgentMgr::GetDistance(me->x, me->y, SPARKFLY_TEKKS_SEARCH.x, SPARKFLY_TEKKS_SEARCH.y) <= maxDist ||
           AgentMgr::GetDistance(me->x, me->y, SPARKFLY_DUNGEON_ENTRY_STAGE.x, SPARKFLY_DUNGEON_ENTRY_STAGE.y) <= maxDist;
}

static void LogSparkflyTekksApproachStatus(const char* stage) {
    auto* me = AgentMgr::GetMyAgent();
    Log::Info("Froggy: Sparkfly Tekks %s map=%u loaded=%d alive=%d hp=%.3f player=(%.0f, %.0f) nearDungeonSide=%d distStage=%.0f distSearch=%.0f distEntry=%.0f",
              stage ? stage : "status",
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              me && me->hp > 0.0f ? 1 : 0,
              me ? me->hp : 0.0f,
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              IsNearSparkflyDungeonSide() ? 1 : 0,
              me ? AgentMgr::GetDistance(me->x, me->y, SPARKFLY_TEKKS_STAGE.x, SPARKFLY_TEKKS_STAGE.y) : -1.0f,
              me ? AgentMgr::GetDistance(me->x, me->y, SPARKFLY_TEKKS_SEARCH.x, SPARKFLY_TEKKS_SEARCH.y) : -1.0f,
              me ? AgentMgr::GetDistance(me->x, me->y, SPARKFLY_DUNGEON_ENTRY_STAGE.x, SPARKFLY_DUNGEON_ENTRY_STAGE.y) : -1.0f);
}

static bool WaitForSparkflyTekksDeathRecovery(const char* context) {
    if (!IsDead()) {
        return true;
    }

    LogBot("Sparkfly Tekks approach death during %s; waiting for resurrection",
           context ? context : "route");
    const bool recovered = DungeonRuntime::WaitForCondition(120000u, []() {
        return !IsDead();
    }, 500u);
    WaitMs(1000);
    LogSparkflyTekksApproachStatus(recovered ? "death-recovered" : "death-recovery-timeout");
    return recovered &&
           MapMgr::GetIsMapLoaded() &&
           MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
}

static bool MoveToTekksFromSparkflyCurrentSide() {
    if (MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    LogSparkflyTekksApproachStatus("approach-start");

    if (IsNearSparkflyDungeonSide()) {
        LogBot("Sparkfly near-dungeon return detected; using short Tekks approach");
        const bool stageReached = MoveToAndWait(
            SPARKFLY_TEKKS_STAGE.x,
            SPARKFLY_TEKKS_STAGE.y,
            SPARKFLY_TEKKS_SHORT_MOVE_THRESHOLD);
        if (!WaitForSparkflyTekksDeathRecovery("short-stage")) {
            return false;
        }
        const bool searchReached = MoveToAndWait(
            SPARKFLY_TEKKS_SEARCH.x,
            SPARKFLY_TEKKS_SEARCH.y,
            SPARKFLY_TEKKS_SHORT_MOVE_THRESHOLD);
        if (!WaitForSparkflyTekksDeathRecovery("short-search")) {
            return false;
        }
        LogSparkflyTekksApproachStatus("short-complete");
        return stageReached || searchReached || IsSparkflyTekksApproachReady(1500.0f);
    }

    for (int attempt = 1; attempt <= 2; ++attempt) {
        LogBot("Sparkfly south-side entry detected; using full aggro route to Tekks attempt %d", attempt);
        FollowWaypoints(SPARKFLY_TO_DUNGEON, SPARKFLY_TO_DUNGEON_COUNT, true);
        LogSparkflyTekksApproachStatus("after-full-route");

        if (!WaitForSparkflyTekksDeathRecovery("full-route")) {
            return false;
        }
        if (MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP || !MapMgr::GetIsMapLoaded()) {
            return false;
        }
        if (!IsSparkflyTekksApproachReady(SPARKFLY_DUNGEON_SIDE_THRESHOLD) && attempt < 2) {
            LogBot("Sparkfly Tekks route recovered away from dungeon side; rerouting once");
            continue;
        }

        const bool stageReached = MoveToAndWait(
            SPARKFLY_TEKKS_STAGE.x,
            SPARKFLY_TEKKS_STAGE.y,
            SPARKFLY_TEKKS_FULL_MOVE_THRESHOLD);
        if (!WaitForSparkflyTekksDeathRecovery("full-stage")) {
            if (attempt < 2) {
                LogBot("Sparkfly Tekks stage move died; retrying route");
                continue;
            }
            return false;
        }
        const bool searchReached = MoveToAndWait(
            SPARKFLY_TEKKS_SEARCH.x,
            SPARKFLY_TEKKS_SEARCH.y,
            SPARKFLY_TEKKS_FULL_MOVE_THRESHOLD);
        if (!WaitForSparkflyTekksDeathRecovery("full-search")) {
            if (attempt < 2) {
                LogBot("Sparkfly Tekks search move died; retrying route");
                continue;
            }
            return false;
        }

        LogSparkflyTekksApproachStatus("full-complete");
        if (stageReached || searchReached || IsSparkflyTekksApproachReady(2500.0f)) {
            return true;
        }
    }

    LogSparkflyTekksApproachStatus("failed");
    return false;
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
