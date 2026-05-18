static bool EnsureOutpostHardModeEnabled(const char* label) {
    if (!label || !*label) label = "Hard mode enabled in outpost";

    const uint32_t mapId = MapMgr::GetMapId();
    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area) {
        FroggyFeatureSkip(label, "AreaInfo unavailable");
        return false;
    }
    if (IsSkillCastMapType(area->type)) {
        FroggyFeatureSkip(label, "Cannot toggle hard mode in explorable instance");
        return false;
    }

    if (PartyMgr::GetIsHardMode()) {
        FroggyFeatureCheck(label, true);
        return true;
    }

    FroggyFeatureReport("  Enabling hard mode in outpost before zoning...");
    MapMgr::SetHardMode(true);
    const bool enabled = WaitFor("Hard mode flag enabled", 5000, []() {
        return PartyMgr::GetIsHardMode();
    });
    FroggyFeatureCheck(label, enabled);
    return enabled;
}
