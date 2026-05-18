static bool AddTemplateHeroesToOutpostParty(
    const char* label,
    const HeroTemplateConfig* heroCfg,
    size_t heroCfgCount) {
    char checkName[128] = {};
    FroggyFeatureReport("  Adding %s heroes...", label);
    for (size_t i = 0; i < heroCfgCount; ++i) {
        FroggyFeatureReport("  Adding hero %u (%zu/%zu)...", heroCfg[i].heroId, i + 1, heroCfgCount);
        PartyMgr::AddHero(heroCfg[i].heroId);
        const DWORD addStart = GetTickCount();
        bool observed = false;
        while ((GetTickCount() - addStart) < 5000) {
            uint32_t current[16] = {};
            const size_t currentCount = PartyMgr::GetPartyHeroIds(current, _countof(current));
            if (currentCount > i && current[i] == heroCfg[i].heroId) {
                observed = true;
                break;
            }
            Sleep(200);
        }
        snprintf(checkName, sizeof(checkName), "%s hero %u joined slot %zu", label, heroCfg[i].heroId, i + 1);
        FroggyFeatureCheck(checkName, observed);
        ReportPartyHeroIds("Party after add");
        if (!observed) {
            return false;
        }
    }
    return true;
}

static void SetTemplateHeroBehaviors(size_t heroCfgCount) {
    for (uint32_t heroIndex = 1; heroIndex <= heroCfgCount; ++heroIndex) {
        PartyMgr::SetHeroBehavior(heroIndex, 1);
        Sleep(300);
    }
    Sleep(1000);
}

static bool VerifyTemplateHeroParty(
    const char* label,
    const HeroTemplateConfig* heroCfg,
    size_t heroCfgCount) {
    char checkName[128] = {};
    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    FroggyFeatureReport("  Party heroes after %s setup: %u", label, heroesAfterAdd);
    ReportPartyHeroIds("Party after setup");
    snprintf(checkName, sizeof(checkName), "%s hero party has seven heroes", label);
    FroggyFeatureCheck(checkName, heroesAfterAdd == kHeroTemplateCount);
    if (heroesAfterAdd != kHeroTemplateCount) {
        return false;
    }

    const bool orderedHeroesReady = WaitForOrderedPartyHeroes(heroCfg, heroCfgCount, 3000);
    snprintf(checkName, sizeof(checkName), "%s hero order matches template", label);
    FroggyFeatureCheck(checkName, orderedHeroesReady);
    return orderedHeroesReady;
}
