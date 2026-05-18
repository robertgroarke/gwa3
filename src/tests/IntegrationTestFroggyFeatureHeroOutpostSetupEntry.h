static bool SetupHeroesFromTemplateForOutpost(const char* filename, const char* label) {
    if (!filename || !*filename || !label || !*label) return false;

    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    FroggyFeatureReport("  Party heroes before %s setup: %u", label, heroesBefore);

    HeroTemplateConfig heroCfg[kHeroTemplateCount] = {};
    size_t heroCfgCount = 0;
    if (!LoadHeroTemplateConfigForOutpost(filename, label, heroCfg, _countof(heroCfg), &heroCfgCount)) {
        return false;
    }

    if (!ClearExistingHeroesForOutpostSetup(label, heroesBefore)) {
        return false;
    }

    if (!AddTemplateHeroesToOutpostParty(label, heroCfg, heroCfgCount)) {
        return false;
    }

    SetTemplateHeroBehaviors(heroCfgCount);
    if (!VerifyTemplateHeroParty(label, heroCfg, heroCfgCount)) {
        return false;
    }

    return LoadTemplateHeroSkillbars(filename, label, heroCfg);
}
