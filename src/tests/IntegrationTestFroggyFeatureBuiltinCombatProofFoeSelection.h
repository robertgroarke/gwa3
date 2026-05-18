static bool IsSeedFoeReachableForBuiltinCombatProof(uint32_t seedFoeId) {
    auto* me = GetAgentLivingRaw(AgentMgr::GetMyId());
    auto* seedFoe = GetAgentLivingRaw(seedFoeId);
    if (!me || !seedFoe || seedFoe->hp <= 0.0f) {
        return false;
    }

    const float dist = AgentMgr::GetDistance(me->x, me->y, seedFoe->x, seedFoe->y);
    FroggyFeatureReport("  Builtin combat proof seed foe=%u distance=%.0f", seedFoeId, dist);
    return dist <= 2500.0f;
}

static bool MoveToBuiltinCombatProofProbe() {
    FroggyFeatureReport("  Repositioning toward Sparkfly combat probe for builtin combat proof...");
    AgentMgr::CancelAction();
    Sleep(100);
    const bool idleBeforeProbe = WaitForPlayerCombatIdle(3000, "Player combat idle before builtin combat proof route probe");
    FroggyFeatureCheck("Player combat idle before builtin combat proof route probe", idleBeforeProbe);
    const bool movedToProbe = MovePlayerNear(-4559.0f, -14406.0f, 500.0f, 25000);
    FroggyFeatureCheck("Moved to builtin combat proof route probe", movedToProbe);
    return movedToProbe;
}

static bool SelectProbeFoeForBuiltinCombatProof(uint32_t& outFoeId, const char*& outLabel) {
    if (!MoveToBuiltinCombatProofProbe()) {
        return false;
    }

    const uint32_t probeFoeId = FindNearestFoe(5000.0f);
    if (!probeFoeId) {
        return false;
    }

    const bool targetReady = WaitForCombatTargetAcquire(probeFoeId);
    FroggyFeatureCheck("Target changed to builtin combat proof foe", targetReady);
    if (!targetReady) {
        return false;
    }

    outFoeId = probeFoeId;
    outLabel = "combat proof foe";
    return true;
}

static bool SelectReachableBuiltinCombatProofFoe(uint32_t seedFoeId,
                                                 uint32_t& outFoeId,
                                                 const char*& outLabel) {
    outFoeId = 0;
    outLabel = "combat proof foe";

    if (IsSeedFoeReachableForBuiltinCombatProof(seedFoeId)) {
        outFoeId = seedFoeId;
        outLabel = "initial foe";
        return true;
    }

    return SelectProbeFoeForBuiltinCombatProof(outFoeId, outLabel);
}
