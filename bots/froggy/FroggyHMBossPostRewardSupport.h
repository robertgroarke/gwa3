// Froggy boss post-reward cleanup and return helpers. Included by FroggyHMBossWaypointHandler.h.

static void SalvageBossRewardGoldIfReady(bool questRewardAccepted) {
    if (questRewardAccepted || s_dungeonLoopTelemetry.reward_dialog_latched) {
        const uint32_t salvaged = MaintenanceMgr::IdentifyAndSalvageGoldItems();
        Log::Info("Froggy: Boss post-reward gold salvage result salvaged=%u questAccepted=%d rewardLatched=%d",
                  salvaged,
                  questRewardAccepted ? 1 : 0,
                  s_dungeonLoopTelemetry.reward_dialog_latched ? 1 : 0);
    } else {
        Log::Info("Froggy: Boss reward not accepted and dialog not latched; skipping post-reward gold salvage");
    }
}

static bool WaitForBossPostRewardReturn(bool questRewardAccepted) {
    const bool usePostRewardLongWait =
        questRewardAccepted || s_dungeonLoopTelemetry.reward_dialog_latched;
    bool returnedToSparkfly =
        DungeonRuntime::WaitForPostDungeonReturn(
            MapIds::SPARKFLY_SWAMP,
            usePostRewardLongWait ? BOSS_POST_REWARD_LONG_TOTAL_WAIT_MS : BOSS_POST_REWARD_SHORT_TOTAL_WAIT_MS,
            usePostRewardLongWait ? BOSS_POST_REWARD_LONG_BOGROOT_WAIT_MS : BOSS_POST_REWARD_SHORT_BOGROOT_WAIT_MS,
            BOGROOT_DUNGEON_MAPS,
            BOGROOT_DUNGEON_MAP_COUNT,
            "Froggy: Boss post-reward");
    if (!returnedToSparkfly) {
        const uint32_t mapAfterReward = MapMgr::GetMapId();
        const bool mapLoadedAfterReward = MapMgr::GetIsMapLoaded();
        const uint32_t myIdAfterReward = AgentMgr::GetMyId();
        if (IsBogrootMapId(mapAfterReward)) {
            if (!mapLoadedAfterReward || myIdAfterReward == 0) {
                Log::Info("Froggy: Boss post-reward map stuck in ghost state map=%u loaded=%d myId=%u; skipping explicit reverse to avoid crash",
                          mapAfterReward,
                          mapLoadedAfterReward ? 1 : 0,
                          myIdAfterReward);
            } else {
                Log::Info("Froggy: Boss post-reward still in Bogroot after wait questRewardAccepted=%d map=%u loaded=%d myId=%u; attempting explicit reverse",
                          questRewardAccepted ? 1 : 0,
                          mapAfterReward,
                          mapLoadedAfterReward ? 1 : 0,
                          myIdAfterReward);
                returnedToSparkfly = ReverseToSparkflySwamp();
                Log::Info("Froggy: Boss explicit reverse to Sparkfly result=%d finalMap=%u loaded=%d myId=%u",
                          returnedToSparkfly ? 1 : 0,
                          MapMgr::GetMapId(),
                          MapMgr::GetIsMapLoaded() ? 1 : 0,
                          AgentMgr::GetMyId());
            }
        } else {
            Log::Info("Froggy: Boss post-reward wait ended off-Bogroot map questRewardAccepted=%d map=%u loaded=%d myId=%u",
                      questRewardAccepted ? 1 : 0,
                      mapAfterReward,
                      mapLoadedAfterReward ? 1 : 0,
                      myIdAfterReward);
        }
    }
    return returnedToSparkfly;
}
