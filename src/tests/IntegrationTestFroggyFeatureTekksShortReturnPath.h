static bool RunShortTekksReturnPath() {
    auto* meBefore = AgentMgr::GetMyAgent();
    FroggyFeatureReport("  Near-dungeon Sparkfly spawn detected: player=(%.0f, %.0f) distToTekks=%.0f distToDoor=%.0f",
              meBefore ? meBefore->x : 0.0f,
              meBefore ? meBefore->y : 0.0f,
              meBefore ? AgentMgr::GetDistance(
                  meBefore->x, meBefore->y,
                  Bot::Froggy::SPARKFLY_TEKKS_STAGE.x,
                  Bot::Froggy::SPARKFLY_TEKKS_STAGE.y) : -1.0f,
              meBefore ? AgentMgr::GetDistance(
                  meBefore->x, meBefore->y,
                  Bot::Froggy::SPARKFLY_DUNGEON_ENTRY_STAGE.x,
                  Bot::Froggy::SPARKFLY_DUNGEON_ENTRY_STAGE.y) : -1.0f);
    bool reached = MovePlayerNear(
        Bot::Froggy::SPARKFLY_TEKKS_STAGE.x,
        Bot::Froggy::SPARKFLY_TEKKS_STAGE.y,
        Bot::Froggy::SPARKFLY_TEKKS_SHORT_MOVE_THRESHOLD,
        45000);
    if (!reached) {
        reached = MovePlayerNear(
            Bot::Froggy::SPARKFLY_TEKKS_SEARCH.x,
            Bot::Froggy::SPARKFLY_TEKKS_SEARCH.y,
            Bot::Froggy::SPARKFLY_TEKKS_SHORT_MOVE_THRESHOLD,
            30000);
    }
    FroggyFeatureCheck("Short Sparkfly return path to Tekks", reached);
    return reached;
}
