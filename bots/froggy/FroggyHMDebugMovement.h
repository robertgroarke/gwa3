// Froggy movement and Tekks debug entrypoints. Included by FroggyHM.cpp.

bool DebugAggroMoveTo(float x, float y, float fightRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    // Populate the player skillbar cache up front. The bot's normal
    // state machine caches in HandleOutpost, but LLM-bridge callers
    // enter Froggy via this debug function without going through that
    // path - without the cache, every TryUseSkillSlot / TryUseSkillWith
    // Role lookup returns false and the PLAYER never casts (heroes
    // still fight, and the LLM sees attack/call but no skill bumps).
    if (!s_combatSession.skills_cached) {
        DungeonCombatRoutine::RefreshSkillCacheWithDebugLog(s_combatSession, "Froggy");
    }

    LogBot("DebugAggroMoveTo request target=(%.0f, %.0f) fightRange=%.0f", x, y, fightRange);
    AggroMoveToEx(x, y, fightRange);
    return DungeonCombat::DistanceToPoint(x, y) <= DungeonCombat::AGGRO_ARRIVAL_THRESHOLD;
}

bool DebugClearAggroInPlace(float fightRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    const float nearestBefore = DungeonCombat::GetNearestLivingEnemyDistance(fightRange + 500.0f);
    LogBot("DebugClearAggroInPlace start fightRange=%.0f nearestBefore=%.0f", fightRange, nearestBefore);
    AgentMgr::CancelAction();
    WaitMs(100);
    FightEnemiesInAggro(fightRange, false, &s_sparkflyTraversalCombatStats);
    AgentMgr::CancelAction();
    WaitMs(250);
    const float nearestAfter = DungeonCombat::GetNearestLivingEnemyDistance(fightRange + 500.0f);
    LogBot("DebugClearAggroInPlace end nearestAfter=%.0f cleared=%d",
           nearestAfter,
           nearestAfter > fightRange ? 1 : 0);
    return nearestAfter > fightRange;
}

bool DebugRunSparkflyRouteToTekks() {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f || !MapMgr::GetIsMapLoaded() || MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
        return false;
    }

    static constexpr float kTekksStageThreshold = 900.0f;
    static constexpr float kTekksReadyThreshold = 2500.0f;

    LogBot("DebugRunSparkflyRouteToTekks start");
    const bool approached = MoveToTekksFromSparkflyCurrentSide();
    if (!approached || !MapMgr::GetIsMapLoaded() || MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
        LogBot("DebugRunSparkflyRouteToTekks aborted after approach: approached=%d map=%u loaded=%d",
               approached ? 1 : 0,
               MapMgr::GetMapId(),
               MapMgr::GetIsMapLoaded() ? 1 : 0);
        return false;
    }

    const bool staged = MoveToAndWait(SPARKFLY_TEKKS_STAGE.x, SPARKFLY_TEKKS_STAGE.y, kTekksStageThreshold);
    const float stageRemaining = DungeonCombat::DistanceToPoint(SPARKFLY_TEKKS_STAGE.x, SPARKFLY_TEKKS_STAGE.y);
    const float searchRemaining = DungeonCombat::DistanceToPoint(SPARKFLY_TEKKS_SEARCH.x, SPARKFLY_TEKKS_SEARCH.y);
    const bool ready = staged || IsSparkflyTekksApproachReady(kTekksReadyThreshold);
    LogBot("DebugRunSparkflyRouteToTekks end staged=%d stageRemaining=%.0f searchRemaining=%.0f ready=%d",
           staged ? 1 : 0,
           stageRemaining,
           searchRemaining,
           ready ? 1 : 0);
    return ready;
}

bool DebugPrepareTekksDungeonEntry() {
    const bool ready = DungeonRuntime::WaitForCondition(5000, []() {
        auto* meInner = AgentMgr::GetMyAgent();
        return meInner != nullptr &&
               meInner->hp > 0.0f &&
               MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP &&
               AgentMgr::GetMyId() != 0;
    }, 100);
    auto* me = AgentMgr::GetMyAgent();
    Log::Info("Froggy: DebugPrepareTekksDungeonEntry gate ready=%d map=%u loaded=%d myId=%u hp=%.3f",
              ready ? 1 : 0,
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              AgentMgr::GetMyId(),
              me ? me->hp : 0.0f);
    if (!ready) {
        return false;
    }

    Log::Info("Froggy: DebugPrepareTekksDungeonEntry start");
    LogBot("DebugPrepareTekksDungeonEntry start");
    const bool prepared = PrepareTekksDungeonEntry();
    Log::Info("Froggy: DebugPrepareTekksDungeonEntry end prepared=%d activeQuest=0x%X lastDialog=0x%X",
              prepared ? 1 : 0,
              QuestMgr::GetActiveQuestId(),
              DialogMgr::GetLastDialogId());
    LogBot("DebugPrepareTekksDungeonEntry end prepared=%d activeQuest=0x%X lastDialog=0x%X",
           prepared ? 1 : 0,
           QuestMgr::GetActiveQuestId(),
           DialogMgr::GetLastDialogId());
    return prepared;
}
