static bool MoveToTekksForQuestDialog() {
    FroggyFeatureReport("=== PHASE 5L: Path to Tekks ===");
    ResetCombatStateBeforeTekksPath();

    if (IsNearSparkflyDungeonSide()) {
        return RunShortTekksReturnPath();
    }

    bool primaryRouteReached = false;
    if (TryPrimaryFroggyTekksRoute(&primaryRouteReached)) {
        return primaryRouteReached;
    }

    if (s_preferDirectTekksStagingForDebug && MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP) {
        return RunDirectTekksDebugPath("Sparkfly spawn");
    }

    return RunSegmentedTekksPath();
}
