static FroggyFeatureAbort RunFroggyOutpostSetupPhase() {
    FroggyFeatureReport("=== PHASE 2: Outpost Tests ===");

    char heroTemplateFile[64] = {};
    char heroTemplateLabel[64] = {};
    ResolvePreferredHeroTemplate(heroTemplateFile, sizeof(heroTemplateFile),
                                 heroTemplateLabel, sizeof(heroTemplateLabel));
    FroggyFeatureReport("  Preferred hero config for current character: %s (%s)", heroTemplateLabel, heroTemplateFile);
    const bool outpostHeroesReady = SetupHeroesFromTemplateForOutpost(heroTemplateFile, heroTemplateLabel);
    char heroSetupCheck[128] = {};
    snprintf(heroSetupCheck, sizeof(heroSetupCheck), "Configured %s heroes in Gadd's", heroTemplateLabel);
    FroggyFeatureCheck(heroSetupCheck, outpostHeroesReady);
    if (!outpostHeroesReady) {
        FroggyFeatureReport("  ABORT: Hero setup failed before leaving Gadd's");
        return AbortFroggyOutpostSetup();
    }

    const bool hardModeReady = EnsureOutpostHardModeEnabled("Hard mode enabled before leaving Gadd's");
    if (!hardModeReady) {
        FroggyFeatureReport("  ABORT: Hard mode was not enabled before leaving Gadd's");
        return AbortFroggyOutpostSetup();
    }

    RunFroggyOutpostSanityChecks();
    return {};
}
