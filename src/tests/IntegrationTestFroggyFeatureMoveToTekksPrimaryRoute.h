static bool ShouldTryPrimaryFroggyTekksRoute() {
    return !s_enableInvasiveSparkflyCombatProofs &&
           !s_preferDirectTekksStagingForDebug &&
           MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
}

static bool TryPrimaryFroggyTekksRoute(bool* outReachedTekks) {
    if (outReachedTekks) *outReachedTekks = false;
    if (!ShouldTryPrimaryFroggyTekksRoute()) {
        if (s_preferDirectTekksStagingForDebug && MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP) {
            FroggyFeatureReport("  Bypassing Froggy's native Sparkfly route for direct Tekks debug staging");
        }
        return false;
    }

    FroggyFeatureReport("  Using Froggy's native Sparkfly route to Tekks before segmented fallbacks...");
    const bool froggyPrimaryRouteReached = Bot::Froggy::DebugRunSparkflyRouteToTekks();
    FroggyFeatureCheck("Primary Froggy Sparkfly route", froggyPrimaryRouteReached);
    if (froggyPrimaryRouteReached) {
        if (outReachedTekks) *outReachedTekks = true;
        return true;
    }

    FroggyFeatureReport("  Primary Froggy Sparkfly route did not settle at Tekks; falling back to segmented probes");
    AgentMgr::CancelAction();
    Sleep(250);
    return false;
}
