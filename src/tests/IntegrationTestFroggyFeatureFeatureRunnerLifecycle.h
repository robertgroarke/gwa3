struct FroggyFeatureAbort {
    bool shouldReturn = false;
    int result = 0;
};

static int FinishFroggyFeatureRunner() {
    FroggyFeatureReport("Entering froggy_done");
    FroggyFeatureReport("Stopping watchdog (non-blocking)...");
    StopWatchdog(false);
    FroggyFeatureReport("Watchdog stop requested");
    FroggyFeatureReport("=== FROGGY FEATURE TESTS COMPLETE ===");
    FroggyFeatureReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return s_failed;
}

static FroggyFeatureAbort AbortFroggyOutpostSetup() {
    StopWatchdog(false);
    FroggyFeatureReport("Watchdog stop requested");
    FroggyFeatureReport("=== FROGGY FEATURE TESTS COMPLETE ===");
    FroggyFeatureReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return {true, s_failed};
}
