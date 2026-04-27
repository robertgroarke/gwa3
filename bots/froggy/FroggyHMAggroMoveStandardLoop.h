// Froggy aggro movement loop implementations. Included by FroggyHMAggroMoveRuntime.h.

static void AggroMoveToStandard(float x, float y, float fightRange, bool sparkflyMap, bool bogrootMap) {
    const float localClearRange = DungeonCombat::ComputeLocalClearRange(fightRange);
    DungeonNavigation::AggroMoveState moveState;
    moveState.moveTargetX = x;
    moveState.moveTargetY = y;
    if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE)) {
        DungeonNavigation::IssueAggroMove(moveState, x, y, sparkflyMap, true);
    }
    DWORD start = GetTickCount();
    const float arrivalThreshold = DungeonCombat::AGGRO_ARRIVAL_THRESHOLD;
    while (DungeonCombat::DistanceToPoint(x, y) > arrivalThreshold &&
           (GetTickCount() - start) < DungeonCombat::AGGRO_MOVE_BUDGET_MS) {
        if (IsDead()) return;
        if (!IsMapLoaded()) return;

        auto* meLoop = AgentMgr::GetMyAgent();
        const float oldX = meLoop ? meLoop->x : 0.0f;
        const float oldY = meLoop ? meLoop->y : 0.0f;
        const float nearestDistance = DungeonCombat::GetNearestLivingEnemyDistance();

        // Check for enemies in fight range. Mirror AutoIt's AggroMoveToEX by
        // entering the fight loop as soon as a foe is inside aggro range.
        {
            if (nearestDistance < localClearRange) {
                if (bogrootMap && DungeonNavigation::ShouldContinueLocalClearCooldown(moveState)) {
                    DungeonNavigation::IssueAggroMove(moveState, x, y, sparkflyMap, true);
                    WaitMs(DungeonCombat::AGGRO_STANDARD_COOLDOWN_MOVE_DELAY_MS);
                    continue;
                }

                moveState.blockedCount = 0;
                const uint32_t bestId = DungeonSkill::GetBestBalledEnemy(localClearRange);
                if (!bestId) {
                    WaitMs(DungeonCombat::AGGRO_STANDARD_NO_BALL_DELAY_MS);
                    continue;
                }
                if (sparkflyMap) {
                    LogBot("AggroMoveToEx Sparkfly holding movement for local clear: foe=%u waypoint=(%.0f, %.0f) dist=%.0f",
                           bestId, x, y, nearestDistance);
                    HoldSparkflyForLocalClear(x, y, fightRange, bestId, &s_sparkflyTraversalCombatStats);
                    WaitForLocalPositionSettle(
                        DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_TIMEOUT_MS,
                        DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_DISTANCE);
                    DungeonNavigation::IssueAggroMove(moveState, x, y, sparkflyMap, true);
                    WaitMs(DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_RESUME_DELAY_MS);
                    continue;
                }

                LogBot("AggroMoveToEx holding movement for local clear: foe=%u waypoint=(%.0f, %.0f) dist=%.0f clearRange=%.0f",
                       bestId, x, y, nearestDistance, localClearRange);
                HoldForLocalClear("Route", x, y, fightRange, bestId, nullptr);
                if (bogrootMap) {
                    DungeonNavigation::ArmLocalClearCooldown(moveState, x, y);
                }
                WaitForLocalPositionSettle(
                    DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_TIMEOUT_MS,
                    DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_DISTANCE);
                DungeonNavigation::IssueAggroMove(moveState, x, y, sparkflyMap, true);
                WaitMs(DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_RESUME_DELAY_MS);
                continue;
            }
        }

        if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE) ||
            (GetTickCount() - start) > DungeonCombat::AGGRO_FORCE_MOVE_AFTER_MS) {
            DungeonNavigation::IssueAggroMove(
                moveState,
                x,
                y,
                sparkflyMap,
                (GetTickCount() - start) > DungeonCombat::AGGRO_FORCE_MOVE_AFTER_MS);
            WaitMs(DungeonCombat::AGGRO_LOOP_POLL_MS);

            // Match AutoIt's immediate blocked handling: if the reissued move
            // did not produce visible positional progress, sidestep and retry.
            DungeonNavigation::HandleBlockedMoveProgress(moveState, x, y, oldX, oldY, sparkflyMap);
        }

        if (moveState.blockedCount > DungeonCombat::AGGRO_BLOCKED_LIMIT) {
            LogBot("AggroMoveToEx blocked limit reached (%d) target=(%.0f, %.0f) remaining=%.0f",
                   moveState.blockedCount, x, y, DungeonCombat::DistanceToPoint(x, y));
            return;
        }
        WaitMs(DungeonCombat::AGGRO_LOOP_POLL_MS);
    }
    LogBot("AggroMoveToEx end target=(%.0f, %.0f) remaining=%.0f threshold=%.0f",
           x, y, DungeonCombat::DistanceToPoint(x, y), arrivalThreshold);
}
