static void RunFroggyDeferredUnitTestsAndMaintenance() {
    FroggyFeatureReport("=== PHASE 0: Unit Tests (deferred until after outpost setup) ===");
    const int unitFailures = Bot::Froggy::RunFroggyUnitTests();
    FroggyFeatureReport("Unit tests: %d failures", unitFailures);
    s_failed += unitFailures;
    RunMerchantMaintenanceCycle("PHASE 3: Merchant Maintenance", false);
}
