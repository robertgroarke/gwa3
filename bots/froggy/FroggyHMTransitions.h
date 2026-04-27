// Froggy wipe recovery and map transition helpers. Included by FroggyHM.cpp
// before waypoint traversal so checkpoint recovery types are visible there.

#include "FroggyHMWipeRecovery.h"

static bool ReverseToSparkflySwamp() {
    return DungeonRuntime::StageAndPushUntilMapReady(
        MapIds::SPARKFLY_SWAMP,
        BOGROOT_REVERSE_TO_SPARKFLY_STAGE.x,
        BOGROOT_REVERSE_TO_SPARKFLY_STAGE.y,
        MAP_TRANSITION_STAGE_THRESHOLD,
        BOGROOT_REVERSE_TO_SPARKFLY_STAGE.x,
        BOGROOT_REVERSE_TO_SPARKFLY_STAGE.y,
        &MoveToAndWait,
        MAP_TRANSITION_REVERSE_PUSH_DELAY_MS,
        MAP_TRANSITION_READY_TIMEOUT_MS,
        MAP_TRANSITION_PUSH_TIMEOUT_MS,
        MAP_TRANSITION_PUSH_INTERVAL_MS,
        "Froggy reverse to Sparkfly");
}

static bool ReturnToSparkflyFromBogroot() {
    if (MapMgr::GetMapId() != MapIds::BOGROOT_GROWTHS_LVL1) {
        return false;
    }

    return DungeonRuntime::StageAndPushUntilMapReady(
        MapIds::SPARKFLY_SWAMP,
        BOGROOT_RETURN_TO_SPARKFLY_STAGE.x,
        BOGROOT_RETURN_TO_SPARKFLY_STAGE.y,
        MAP_TRANSITION_STAGE_THRESHOLD,
        BOGROOT_RETURN_TO_SPARKFLY_PUSH.x,
        BOGROOT_RETURN_TO_SPARKFLY_PUSH.y,
        &MoveToAndWait,
        MAP_TRANSITION_RETURN_PUSH_DELAY_MS,
        MAP_TRANSITION_READY_TIMEOUT_MS,
        MAP_TRANSITION_PUSH_TIMEOUT_MS,
        MAP_TRANSITION_PUSH_INTERVAL_MS,
        "Froggy transition");
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
    if (MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
        return false;
    }

    const auto entryPlan = GetEntryBootstrapPlan();
    for (int i = 0; i < entryPlan.entry_path_count; ++i) {
        const auto& point = entryPlan.entry_path[i];
        if (i + 1 < entryPlan.entry_path_count) {
            MoveToAndWait(point.x, point.y);
        } else {
            AgentMgr::Move(point.x, point.y);
        }
    }
    WaitMs(MAP_TRANSITION_ENTRY_ROUTE_DWELL_MS);

    return DungeonRuntime::PushUntilMapReady(
        entryPlan.target_map_id,
        entryPlan.zone_point.x,
        entryPlan.zone_point.y,
        MAP_TRANSITION_READY_TIMEOUT_MS,
        MAP_TRANSITION_PUSH_TIMEOUT_MS,
        MAP_TRANSITION_PUSH_INTERVAL_MS,
        "Froggy transition");
}

static void ResetTekksQuestEntryFailures(const char* reason) {
    if (s_tekksQuestEntryFailureCount > 0) {
        Log::Info("Froggy: Resetting Tekks quest-entry failure count from %d reason=%s",
                  s_tekksQuestEntryFailureCount,
                  reason ? reason : "unspecified");
    }
    s_tekksQuestEntryFailureCount = 0;
}

static bool BounceThroughBogrootToResetTekksDialog() {
    if (MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
        Log::Info("Froggy: Tekks dialog reset bounce skipped because map=%u is not Sparkfly",
                  MapMgr::GetMapId());
        return false;
    }

    LogBot("Tekks quest dialog failed repeatedly; zoning into Bogroot and back to reset dialog");
    const bool entered = EnterBogrootFromSparkfly();
    if (!entered) {
        Log::Info("Froggy: Tekks dialog reset bounce failed to enter Bogroot finalMap=%u",
                  MapMgr::GetMapId());
        WaitMs(TEKKS_DIALOG_RESET_SETTLE_MS);
        return false;
    }

    WaitMs(TEKKS_DIALOG_RESET_SETTLE_MS);
    const bool returned = ReturnToSparkflyFromBogroot();
    WaitMs(TEKKS_DIALOG_RESET_SETTLE_MS);
    Log::Info("Froggy: Tekks dialog reset bounce entered=%d returned=%d finalMap=%u",
              entered ? 1 : 0,
              returned ? 1 : 0,
              MapMgr::GetMapId());
    return returned && MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
}

static bool RecordTekksQuestEntryFailureAndMaybeResetDialog(const char* context) {
    ++s_tekksQuestEntryFailureCount;
    Log::Info("Froggy: Tekks quest-entry failure count=%d threshold=%d context=%s map=%u",
              s_tekksQuestEntryFailureCount,
              TEKKS_DIALOG_RESET_FAILURE_THRESHOLD,
              context ? context : "unspecified",
              MapMgr::GetMapId());

    if (!ShouldResetTekksDialogAfterQuestFailures(s_tekksQuestEntryFailureCount)) {
        return false;
    }

    const bool bounced = BounceThroughBogrootToResetTekksDialog();
    ResetTekksQuestEntryFailures(bounced ? "dialog-reset-bounce" : "dialog-reset-bounce-failed");
    return bounced;
}
