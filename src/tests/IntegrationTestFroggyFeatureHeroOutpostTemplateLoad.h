static bool LoadHeroTemplateConfigForOutpost(
    const char* filename,
    const char* label,
    HeroTemplateConfig* heroCfg,
    size_t heroCfgCapacity,
    size_t* outHeroCfgCount) {
    if (!filename || !*filename || !label || !*label || !heroCfg || !outHeroCfgCount) return false;

    *outHeroCfgCount = LoadHeroTemplates(filename, heroCfg, heroCfgCapacity);

    char checkName[128] = {};
    snprintf(checkName, sizeof(checkName), "Loaded %s hero config", label);
    FroggyFeatureCheck(checkName, *outHeroCfgCount == kHeroTemplateCount);
    return *outHeroCfgCount == kHeroTemplateCount;
}
