// Froggy wipe recovery helpers. Included by FroggyHMTransitions.h.

enum class WipeRecoveryContext {
    Standard,
    DungeonDoorCheckpoint,
    QuestDoorCheckpoint,
};

static void LogDetectedWaypointWipe(
    const Waypoint* wps,
    int currentIndex,
    WipeRecoveryContext context,
    uint32_t detectedWipeCount) {
    switch (context) {
    case WipeRecoveryContext::DungeonDoorCheckpoint:
        LogBot("Dungeon Door Checkpoint: WIPE detected at wp %d - wipe #%u", currentIndex, detectedWipeCount);
        break;
    case WipeRecoveryContext::QuestDoorCheckpoint:
        LogBot("Quest Door Checkpoint: WIPE detected after AggroMoveToEx at wp %d - wipe #%u", currentIndex, detectedWipeCount);
        break;
    default:
        LogBot("WIPE detected at waypoint %d (%s) - wipe #%u",
               currentIndex,
               wps[currentIndex].label,
               detectedWipeCount);
        break;
    }
}

static DungeonCheckpoint::WaypointWipeRecoveryOptions MakeFroggyWipeRecoveryOptions(
    const Waypoint* wps,
    int count) {
    DungeonCheckpoint::WaypointWipeRecoveryOptions options;
    options.nearest_index = GetNearestWaypointIndex(wps, count);
    options.waypoint_count = count;
    options.backtrack_steps = 2;
    options.wipe_count = &s_wipeCount;
    options.is_dead = &IsDead;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.return_to_outpost = &MapMgr::ReturnToOutpost;
    options.use_dp_removal = &UseDpRemovalIfNeeded;
    return options;
}

static void LogWaypointWipeOutpostReturn(WipeRecoveryContext context) {
    switch (context) {
    case WipeRecoveryContext::DungeonDoorCheckpoint:
        LogBot("Dungeon Door Checkpoint: party defeated - returning to outpost");
        break;
    case WipeRecoveryContext::QuestDoorCheckpoint:
        LogBot("Quest Door Checkpoint: party defeated - returning to outpost");
        break;
    default:
        LogBot("Party defeated - returning to outpost");
        break;
    }
}

static void LogWaypointWipeDpRemoval(
    WipeRecoveryContext context,
    const DungeonCheckpoint::WaypointWipeRecoveryResult& recovery) {
    if (!recovery.used_dp_removal) {
        return;
    }
    if (context == WipeRecoveryContext::Standard) {
        LogBot("Multiple wipes (%u) - using DP removal", recovery.wipe_count);
    } else if (context == WipeRecoveryContext::QuestDoorCheckpoint) {
        LogBot("Quest Door Checkpoint: multiple wipes (%u) - using DP removal", recovery.wipe_count);
    }
}

static void LogWaypointWipeResume(WipeRecoveryContext context, int restartIndex, int currentIndex) {
    switch (context) {
    case WipeRecoveryContext::DungeonDoorCheckpoint:
        LogBot("Dungeon Door Checkpoint: resuming from checkpoint %d after wipe (was at %d)",
               restartIndex, currentIndex);
        break;
    case WipeRecoveryContext::QuestDoorCheckpoint:
        LogBot("Quest Door Checkpoint: resuming from checkpoint %d after wipe (was at %d)",
               restartIndex, currentIndex);
        break;
    default:
        LogBot("Resuming from checkpoint %d after wipe (was at %d)", restartIndex, currentIndex);
        break;
    }
}

static bool RecoverFromWaypointWipe(const Waypoint* wps,
                                    int count,
                                    int currentIndex,
                                    WipeRecoveryContext context,
                                    int& outRestartIndex) {
    LogDetectedWaypointWipe(wps, currentIndex, context, s_wipeCount + 1u);

    const auto options = MakeFroggyWipeRecoveryOptions(wps, count);
    const auto recovery = DungeonCheckpoint::RecoverWaypointWipe(options);

    if (recovery.returned_to_outpost) {
        LogWaypointWipeOutpostReturn(context);
        return false;
    }

    LogWaypointWipeDpRemoval(context, recovery);
    outRestartIndex = recovery.restart_index;
    LogWaypointWipeResume(context, outRestartIndex, currentIndex);
    return true;
}
