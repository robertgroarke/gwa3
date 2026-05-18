static bool MoveTekksSegmentStep(const MoveStep& step, size_t stepIndex) {
    const float fightRange = GetFightRangeForTekksStep(stepIndex);
    const int moveTimeoutMs = (s_preferDirectTekksStagingForDebug && stepIndex >= 4)
        ? max(step.timeoutMs, 45000)
        : step.timeoutMs;
    FroggyFeatureReport("  Moving to %s (%.0f, %.0f) fightRange=%.0f...", step.label, step.x, step.y, fightRange);
    const bool reached = s_preferDirectTekksStagingForDebug
        ? MovePlayerNear(step.x, step.y, step.threshold, moveTimeoutMs)
        : (MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP
            ? Bot::Froggy::DebugAggroMoveTo(step.x, step.y, fightRange)
            : MovePlayerNear(step.x, step.y, step.threshold, step.timeoutMs));
    if (reached || !s_preferDirectTekksStagingForDebug) {
        return reached;
    }

    FroggyFeatureReport("  Retrying %s once with relaxed debug tolerance...", step.label);
    return MovePlayerNear(step.x, step.y, step.threshold + 150.0f, 30000);
}

static bool RecoverFromTekksSegmentFailure(const MoveStep& step) {
    FroggyFeatureReport("  Aggro route failed at %s; evaluating Tekks fallback path", step.label);
    AgentMgr::CancelAction();
    Sleep(250);
    if (!s_preferDirectTekksStagingForDebug) {
        const bool froggyFallbackReached = Bot::Froggy::DebugRunSparkflyRouteToTekks();
        FroggyFeatureCheck("Fallback Froggy Sparkfly route", froggyFallbackReached);
        return froggyFallbackReached;
    }

    FroggyFeatureReport("  Direct Tekks debug staging is enabled; skipping Froggy route recovery");
    const bool directFallbackReached = RunDirectTekksDebugTail(step.label);
    FroggyFeatureCheck("Fallback direct Tekks staging", directFallbackReached);
    return directFallbackReached;
}
