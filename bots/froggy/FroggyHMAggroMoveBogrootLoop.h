// Froggy aggro movement loop implementations. Included by FroggyHMAggroMoveRuntime.h.

static void AggroMoveToBogroot(float x, float y, float fightRange) {
    // Match AutoIt's Bogroot traversal more closely: keep advancing toward
    // the waypoint, fight opportunistically, and loot between movement
    // bursts rather than holding a full local-clear on every nearby pack.
    const float arrivalThreshold = DungeonCombat::AGGRO_ARRIVAL_THRESHOLD;
    const float moveRandomRadius = DungeonCombat::AGGRO_MOVE_RANDOM_RADIUS;
    DWORD start = GetTickCount();
    int blockedCount = 0;
    float moveTargetX = x;
    float moveTargetY = y;

    auto issueBogrootMove = [&]() {
        moveTargetX = DungeonNavigation::RandomizedCoordinate(x, moveRandomRadius);
        moveTargetY = DungeonNavigation::RandomizedCoordinate(y, moveRandomRadius);
        AgentMgr::Move(moveTargetX, moveTargetY);
    };
    auto issueBogrootSidestep = [&]() {
        auto* me = AgentMgr::GetMyAgent();
        if (!me) return;
        AgentMgr::Move(DungeonNavigation::RandomizedCoordinate(me->x, AGGRO_BOGROOT_SIDESTEP_RANDOM_RADIUS),
                       DungeonNavigation::RandomizedCoordinate(me->y, AGGRO_BOGROOT_SIDESTEP_RANDOM_RADIUS));
    };

    if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE)) {
        issueBogrootMove();
    }

    while (DungeonCombat::DistanceToPoint(x, y) > arrivalThreshold &&
           (GetTickCount() - start) < DungeonCombat::AGGRO_MOVE_BUDGET_MS) {
        if (IsDead() || !IsMapLoaded()) return;

        auto* meBefore = AgentMgr::GetMyAgent();
        const float oldX = meBefore ? meBefore->x : 0.0f;
        const float oldY = meBefore ? meBefore->y : 0.0f;

        if (DungeonCombat::GetNearestLivingEnemyDistance() < fightRange) {
            FightEnemiesInAggro(fightRange, false, nullptr, true, AGGRO_BOGROOT_FIGHT_BUDGET_MS);
        }

        if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE) ||
            (GetTickCount() - start) > DungeonCombat::AGGRO_FORCE_MOVE_AFTER_MS) {
            issueBogrootMove();
            PickupNearbyLoot(AGGRO_BOGROOT_LOOT_RADIUS);
            WaitMs(DungeonCombat::AGGRO_LOOP_POLL_MS);

            auto* meAfter = AgentMgr::GetMyAgent();
            if (meAfter && meAfter->x == oldX && meAfter->y == oldY) {
                ++blockedCount;
                issueBogrootSidestep();
                WaitMs(DungeonCombat::AGGRO_BLOCKED_SIDESTEP_WAIT_MS);
                issueBogrootMove();
            } else {
                blockedCount = 0;
            }
        }

        if (blockedCount > DungeonCombat::AGGRO_BLOCKED_LIMIT) {
            LogBot("AggroMoveToEx Bogroot blocked limit reached (%d) target=(%.0f, %.0f) remaining=%.0f",
                   blockedCount, x, y, DungeonCombat::DistanceToPoint(x, y));
            return;
        }

        WaitMs(DungeonCombat::AGGRO_LOOP_POLL_MS);
    }

    LogBot("AggroMoveToEx Bogroot end target=(%.0f, %.0f) remaining=%.0f threshold=%.0f",
           x, y, DungeonCombat::DistanceToPoint(x, y), arrivalThreshold);
    return;
}

