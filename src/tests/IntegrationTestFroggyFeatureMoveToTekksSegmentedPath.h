static bool RunSegmentedTekksPath() {
    for (size_t stepIndex = 0; stepIndex < _countof(kSparkflyToTekksPath); ++stepIndex) {
        const auto& step = kSparkflyToTekksPath[stepIndex];
        if (s_preferDirectTekksStagingForDebug && stepIndex == 6) {
            FroggyFeatureReport("  Direct Tekks debug staging: cutting over after waypoint 6 and replacing the late Sparkfly tail");
            return RunDirectTekksDebugTail("Sparkfly waypoint 7");
        }

        const bool stepReached = MoveTekksSegmentStep(step, stepIndex);
        FroggyFeatureCheck(step.label, stepReached);
        if (!stepReached) {
            return RecoverFromTekksSegmentFailure(step);
        }
    }
    return true;
}
