static bool ValidateExplorableLootPickupContext() {
    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        FroggyFeatureSkip("Explorable loot pickup", "Not in game");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area || !IsSkillCastMapType(area->type)) {
        FroggyFeatureSkip("Explorable loot pickup", "Current map is not explorable-like");
        return false;
    }

    if (!WaitForStablePlayerState(5000)) {
        FroggyFeatureSkip("Explorable loot pickup", "Player state not stable enough for loot scan");
        return false;
    }

    return true;
}
