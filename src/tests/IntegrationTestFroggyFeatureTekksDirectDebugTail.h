static bool RunDirectTekksDebugTail(const char* reasonLabel) {
    static constexpr TekksDebugTailStep kDebugTail[] = {
        {11025.0f, 11710.0f, 650.0f, 45000, "Direct debug tail waypoint 8"},
        {14624.0f, 19314.0f, 650.0f, 60000, "Direct debug tail waypoint 9"},
        {Bot::Froggy::SPARKFLY_TEKKS_STAGE.x, Bot::Froggy::SPARKFLY_TEKKS_STAGE.y, 500.0f, 90000, "Direct Tekks staging tail"},
    };

    FroggyFeatureReport("  Direct Tekks debug tail starting from %s", reasonLabel);
    for (const auto& tailStep : kDebugTail) {
        auto* meBefore = AgentMgr::GetMyAgent();
        FroggyFeatureReport("    %s: player=(%.0f, %.0f) target=(%.0f, %.0f) threshold=%.0f timeout=%lu",
                  tailStep.label,
                  meBefore ? meBefore->x : 0.0f,
                  meBefore ? meBefore->y : 0.0f,
                  tailStep.x,
                  tailStep.y,
                  tailStep.threshold,
                  static_cast<unsigned long>(tailStep.timeoutMs));
        bool tailReached = MovePlayerNear(tailStep.x, tailStep.y, tailStep.threshold, tailStep.timeoutMs);
        if (!tailReached) {
            FroggyFeatureReport("    Retrying %s once with relaxed threshold...", tailStep.label);
            tailReached = MovePlayerNear(tailStep.x, tailStep.y, tailStep.threshold + 150.0f, 30000);
        }
        FroggyFeatureCheck(tailStep.label, tailReached);
        if (!tailReached) {
            auto* meAfter = AgentMgr::GetMyAgent();
            FroggyFeatureReport("    %s failed with player ending at (%.0f, %.0f)",
                      tailStep.label,
                      meAfter ? meAfter->x : 0.0f,
                      meAfter ? meAfter->y : 0.0f);
            return false;
        }
    }
    return true;
}
