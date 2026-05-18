static void ReportPartyHeroIds(const char* label) {
    uint32_t heroIds[16] = {};
    const size_t heroCount = PartyMgr::GetPartyHeroIds(heroIds, _countof(heroIds));
    char buf[256] = {};
    size_t used = 0;
    for (size_t i = 0; i < heroCount && used + 16 < sizeof(buf); ++i) {
        used += snprintf(buf + used, sizeof(buf) - used, "%s%u", i == 0 ? "" : ", ", heroIds[i]);
    }
    FroggyFeatureReport("  %s hero ids (%zu): [%s]", label, heroCount, buf);
}

static bool PartyHeroIdsMatchOrdered(const HeroTemplateConfig* heroCfg, size_t heroCfgCount) {
    if (!heroCfg || heroCfgCount == 0) return false;
    uint32_t current[16] = {};
    const size_t currentCount = PartyMgr::GetPartyHeroIds(current, _countof(current));
    if (currentCount != heroCfgCount) return false;
    for (size_t i = 0; i < heroCfgCount; ++i) {
        if (current[i] != heroCfg[i].heroId) return false;
    }
    return true;
}

static bool WaitForOrderedPartyHeroes(const HeroTemplateConfig* heroCfg, size_t heroCfgCount, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (PartyHeroIdsMatchOrdered(heroCfg, heroCfgCount)) {
            return true;
        }
        Sleep(200);
    }
    return false;
}
