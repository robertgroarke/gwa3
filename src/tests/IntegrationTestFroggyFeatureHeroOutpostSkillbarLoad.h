static bool LoadTemplateHeroSkillbars(
    const char* filename,
    const char* label,
    const HeroTemplateConfig* heroCfg) {
    char checkName[128] = {};
    FroggyFeatureReport("  Loading hero skillbars from %s...", filename);
    for (uint32_t heroIndex = 1; heroIndex <= kHeroTemplateCount; ++heroIndex) {
        uint32_t before[8] = {};
        const bool copiedBefore = WaitForHeroSkillbarAvailable(heroIndex, before, 4000);
        snprintf(checkName, sizeof(checkName), "%s hero skillbar available before template load", label);
        FroggyFeatureCheck(checkName, copiedBefore);
        if (!copiedBefore) return false;

        SkillMgr::LoadSkillbar(heroCfg[heroIndex - 1].skills, heroIndex);
        const bool restoredMatches = WaitForHeroSkillbarMatch(heroIndex, heroCfg[heroIndex - 1].skills, 5000);
        snprintf(checkName, sizeof(checkName), "%s hero skillbar matches template after load", label);
        FroggyFeatureCheck(checkName, restoredMatches);
        if (!restoredMatches) return false;

        ReportHeroSkillbarState("After template load", heroIndex, heroCfg[heroIndex - 1].skills);
        Sleep(500);
    }

    return true;
}
