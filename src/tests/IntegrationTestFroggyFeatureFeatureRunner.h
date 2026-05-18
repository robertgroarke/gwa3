static int RunFroggyFeatureTestImpl(bool isolatedExplorableFlaggingMode) {
    s_isolatedExplorableFlaggingMode = isolatedExplorableFlaggingMode;
    s_enableInvasiveSparkflyCombatProofs = false;
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;
    StartWatchdog();

    FroggyFeatureAbort abort = RunFroggyTravelToGaddsPhase();
    if (abort.shouldReturn) return abort.result;

    abort = RunFroggyOutpostSetupPhase();
    if (abort.shouldReturn) return abort.result;

    RunFroggyDeferredUnitTestsAndMaintenance();
    if (RunFroggySparkflyEntryPhase() != SparkflyEntryResult::InSparkfly) {
        return FinishFroggyFeatureRunner();
    }

    if (RunSparkflyEnemyProofs()) {
        return FinishFroggyFeatureRunner();
    }

    bool preserveSparkflyLoopState = false;
    RunFroggyTekksBogrootPath(preserveSparkflyLoopState);
    RunFroggySparkflyReturnOrPreserveCleanup(preserveSparkflyLoopState);
    return FinishFroggyFeatureRunner();
}
