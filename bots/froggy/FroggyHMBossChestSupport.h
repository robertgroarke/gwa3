// Froggy boss chest staging and reward NPC discovery helpers. Included by FroggyHMBossWaypointHandler.h.

static bool HandleBossChestLoot() {
    const bool chestStageReached = MoveToAndWait(BOSS_CHEST_X, BOSS_CHEST_Y);
    if (!chestStageReached) {
        Log::Warn("Froggy: Boss chest staging failed to reach target before chest interaction");
        return false;
    }
    ++s_dungeonLoopTelemetry.chest_attempts;
    if (OpenChestAt(BOSS_CHEST_X, BOSS_CHEST_Y, BOSS_CHEST_OPEN_RADIUS)) {
        ++s_dungeonLoopTelemetry.chest_successes;
    }
    WaitMs(BOSS_CHEST_FIRST_LOOT_DELAY_MS);
    PickupNearbyLoot(BOSS_CHEST_LOOT_RADIUS);

    ++s_dungeonLoopTelemetry.chest_attempts;
    if (OpenChestAt(BOSS_CHEST_X, BOSS_CHEST_Y, BOSS_CHEST_OPEN_RADIUS)) {
        ++s_dungeonLoopTelemetry.chest_successes;
    }
    WaitMs(BOSS_CHEST_RETRY_LOOT_DELAY_MS);
    PickupNearbyLoot(BOSS_CHEST_LOOT_RADIUS);
    return true;
}

static bool StageBossRewardInteraction() {
    const auto rewardNpc = GetRewardNpcAnchor();
    const bool rewardStageReached = MoveToAndWait(rewardNpc.x, rewardNpc.y);
    if (!rewardStageReached) {
        Log::Warn("Froggy: Boss reward staging failed to reach target before Tekks reward interaction");
        return false;
    }
    WaitForLocalPositionSettle(BOSS_REWARD_SETTLE_TIMEOUT_MS, BOSS_REWARD_SETTLE_DISTANCE);
    s_dungeonLoopTelemetry.reward_attempted = true;
    auto* me = AgentMgr::GetMyAgent();
    Log::Info("Froggy: Boss reward staging player=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f map=%u loaded=%d",
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              rewardNpc.x,
              rewardNpc.y,
              me ? AgentMgr::GetDistance(me->x, me->y, rewardNpc.x, rewardNpc.y) : -1.0f,
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0);
    return true;
}

static uint32_t FindBossRewardNpc() {
    const auto rewardNpc = GetRewardNpcAnchor();
    NearbyNpcCandidate rewardCandidates[8] = {};
    size_t rewardCandidateCount = CollectNearbyNpcCandidates(
        rewardNpc.x,
        rewardNpc.y,
        BOSS_REWARD_NPC_SEARCH_RADIUS,
        rewardCandidates,
        _countof(rewardCandidates));
    LogNearbyNpcCandidates(
        "Boss reward",
        rewardNpc.x,
        rewardNpc.y,
        BOSS_REWARD_NPC_SEARCH_RADIUS,
        rewardCandidates,
        rewardCandidateCount);

    uint32_t tekksId = rewardCandidateCount > 0
        ? rewardCandidates[0].agentId
        : DungeonInteractions::FindNearestNpc(rewardNpc.x, rewardNpc.y, BOSS_REWARD_NPC_SEARCH_RADIUS);
    if (!tekksId) {
        auto* me = AgentMgr::GetMyAgent();
        if (me) {
            NearbyNpcCandidate localCandidates[8] = {};
            const size_t localCandidateCount = CollectNearbyNpcCandidates(
                me->x,
                me->y,
                BOSS_REWARD_LOCAL_NPC_SEARCH_RADIUS,
                localCandidates,
                _countof(localCandidates));
            LogNearbyNpcCandidates(
                "Boss reward local",
                me->x,
                me->y,
                BOSS_REWARD_LOCAL_NPC_SEARCH_RADIUS,
                localCandidates,
                localCandidateCount);
            if (localCandidateCount > 0) {
                tekksId = localCandidates[0].agentId;
            }
        }
    }
    return tekksId;
}
