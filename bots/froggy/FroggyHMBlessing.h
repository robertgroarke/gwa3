// Froggy dungeon blessing adapter. Included by FroggyHM.cpp.

static bool WaitForBlessingPositionSettle(uint32_t timeoutMs, float maxDeltaPerSample) {
    return WaitForLocalPositionSettle(timeoutMs, maxDeltaPerSample);
}

static void GrabDungeonBlessing(float shrineX, float shrineY) {
    DungeonEffects::DungeonBlessingAcquireOptions options;
    options.primary_search_radius = BLESSING_PRIMARY_SEARCH_RADIUS;
    options.fallback_search_radius = BLESSING_FALLBACK_SEARCH_RADIUS;
    options.signpost_move_threshold = BLESSING_SIGNPOST_MOVE_THRESHOLD;
    options.npc_move_threshold = BLESSING_NPC_MOVE_THRESHOLD;
    options.required_title_id = BLESSING_TITLE_ID;
    options.title_settle_delay_ms = BLESSING_TITLE_SETTLE_MS;
    options.accept_dialog_id = BLESSING_ACCEPT_DIALOG_ID;
    options.settle_timeout_ms = BLESSING_SETTLE_TIMEOUT_MS;
    options.settle_distance = BLESSING_SETTLE_DISTANCE;
    options.log_prefix = "Froggy: Blessing";
    options.move_to_point = &MoveToAndWait;
    options.wait_for_position_settle = &WaitForBlessingPositionSettle;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.signpost_scan_log = &LogNearbySignposts;
    options.agent_log = &LogAgentIdentity;
    (void)DungeonEffects::AcquireDungeonBlessingAt(shrineX, shrineY, options);
}
