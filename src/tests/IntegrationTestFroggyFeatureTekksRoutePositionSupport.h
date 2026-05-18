static bool IsNearSparkflyDungeonSide() {
    if (MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) return false;
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f) return false;
    const float distToTekksStage = AgentMgr::GetDistance(
        me->x, me->y,
        Bot::Froggy::SPARKFLY_TEKKS_STAGE.x,
        Bot::Froggy::SPARKFLY_TEKKS_STAGE.y);
    const float distToDungeonStage = AgentMgr::GetDistance(
        me->x, me->y,
        Bot::Froggy::SPARKFLY_DUNGEON_ENTRY_STAGE.x,
        Bot::Froggy::SPARKFLY_DUNGEON_ENTRY_STAGE.y);
    return distToTekksStage <= Bot::Froggy::SPARKFLY_DUNGEON_SIDE_THRESHOLD ||
           distToDungeonStage <= Bot::Froggy::SPARKFLY_DUNGEON_SIDE_THRESHOLD;
}

static float GetFightRangeForTekksStep(size_t index) {
    return index < 4 ? 1350.0f : 1250.0f;
}
