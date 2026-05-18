static bool ClearExistingHeroesForOutpostSetup(const char* label, uint32_t heroesBefore) {
    if (heroesBefore == 0) {
        return true;
    }

    char checkName[128] = {};
    FroggyFeatureReport("  Clearing existing heroes before %s setup...", label);
    const bool cleared = KickAllHeroesWithObservation(4000);
    const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
    FroggyFeatureReport("  Party heroes after clear: %u", heroesAfterKick);
    ReportPartyHeroIds("Party after clear");
    snprintf(checkName, sizeof(checkName), "Existing heroes cleared before %s setup", label);
    FroggyFeatureCheck(checkName, cleared && heroesAfterKick == 0);
    return cleared && heroesAfterKick == 0;
}
