static bool RunDirectTekksDebugPath(const char* reasonLabel) {
    static constexpr TekksDebugPathStep kDebugPath[] = {
        {-928.0f,  -8699.0f,  1000.0f, 35000, "Direct debug path waypoint 3", false},
        {4200.0f,  -4897.0f,   900.0f, 45000, "Direct debug path waypoint 4", false},
        {6114.0f,   819.0f,    900.0f, 45000, "Direct debug path waypoint 5", false},
        {9500.0f,  2281.0f,    900.0f, 45000, "Direct debug path waypoint 6", false},
        {11570.0f, 6120.0f,    900.0f, 45000, "Direct debug path waypoint 7", false},
        {11025.0f, 11710.0f,   900.0f, 45000, "Direct debug path waypoint 8", false},
        {14624.0f, 19314.0f,   700.0f, 60000, "Direct debug path waypoint 9", false},
        {Bot::Froggy::SPARKFLY_TEKKS_STAGE.x, Bot::Froggy::SPARKFLY_TEKKS_STAGE.y, 500.0f, 90000, "Direct Tekks staging tail", true},
    };

    FroggyFeatureReport("  Direct Tekks debug path starting from %s", reasonLabel);
    bool requiredReached = true;
    for (const auto& pathStep : kDebugPath) {
        auto* meBefore = AgentMgr::GetMyAgent();
        FroggyFeatureReport("    %s: player=(%.0f, %.0f) target=(%.0f, %.0f) threshold=%.0f timeout=%lu required=%d",
                  pathStep.label,
                  meBefore ? meBefore->x : 0.0f,
                  meBefore ? meBefore->y : 0.0f,
                  pathStep.x,
                  pathStep.y,
                  pathStep.threshold,
                  static_cast<unsigned long>(pathStep.timeoutMs),
                  pathStep.required ? 1 : 0);
        bool pathReached = MovePlayerNear(pathStep.x, pathStep.y, pathStep.threshold, pathStep.timeoutMs);
        if (!pathReached) {
            FroggyFeatureReport("    Retrying %s once with relaxed threshold...", pathStep.label);
            pathReached = MovePlayerNear(pathStep.x, pathStep.y, pathStep.threshold + 200.0f, 30000);
        }
        auto* meAfter = AgentMgr::GetMyAgent();
        const float distAfter = meAfter
            ? AgentMgr::GetDistance(meAfter->x, meAfter->y, pathStep.x, pathStep.y)
            : -1.0f;
        FroggyFeatureReport("    %s result: reached=%d player=(%.0f, %.0f) distAfter=%.0f",
                  pathStep.label,
                  pathReached ? 1 : 0,
                  meAfter ? meAfter->x : 0.0f,
                  meAfter ? meAfter->y : 0.0f,
                  distAfter);
        if (pathStep.required) {
            FroggyFeatureCheck(pathStep.label, pathReached);
            requiredReached &= pathReached;
        }
    }
    return requiredReached;
}
