static void ResetSparkflyRouteTestState() {
    s_isolatedExplorableFlaggingMode = false;
    s_enableInvasiveSparkflyCombatProofs = false;
    s_preferDirectTekksStagingForDebug = false;
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;
}

static int FinishSparkflyRouteTest() {
    s_preferDirectTekksStagingForDebug = false;
    FroggyFeatureReport("Stopping watchdog (non-blocking)...");
    StopWatchdog(false);
    FroggyFeatureReport("Watchdog stop requested");
    FroggyFeatureReport("=== FROGGY SPARKFLY TEST COMPLETE ===");
    FroggyFeatureReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return s_failed;
}
