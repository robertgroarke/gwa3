static bool IsInBogrootDungeonLoopMap() {
    const uint32_t mapId = MapMgr::GetMapId();
    return mapId == MapIds::BOGROOT_GROWTHS_LVL1 || mapId == MapIds::BOGROOT_GROWTHS_LVL2;
}

static bool PrepareBogrootDungeonLoopProof() {
    if (!IsInBogrootDungeonLoopMap()) {
        FroggyFeatureSkip("Bogroot dungeon loop", "Not in Bogroot Growths");
        return false;
    }

    FroggyFeatureCheck("Froggy combat cache refresh before Bogroot loop",
                DungeonCombatRoutine::RefreshCombatSkillbarForDebug(Bot::Froggy::g_combatSession, "Froggy"));
    Bot::Froggy::ResetDungeonLoopTelemetry();
    return true;
}
