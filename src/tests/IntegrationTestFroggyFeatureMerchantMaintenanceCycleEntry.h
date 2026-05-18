static void RunMerchantMaintenanceCycle(const char* label, bool includeSalvage) {
    FroggyFeatureReport("=== %s ===", label);
    uint32_t openedMerchantId = 0;
    const bool merchantOpen = EnsureGaddsMerchantOpen(&openedMerchantId);
    if (!merchantOpen) {
        FroggyFeatureSkip(label, "Merchant NPC not found near target coords");
        return;
    }

    const MerchantMaintenanceBeforeState before =
        CaptureMerchantMaintenanceBeforeState(openedMerchantId);
    const MerchantMaintenanceActionCounts counts =
        RunMerchantMaintenanceItemActions(before, includeSalvage);
    RestockMerchantMaintenanceKits();
    ReportMerchantMaintenanceAfterState(counts.identified, counts.salvaged, counts.sold);
    CleanupMerchantMaintenanceSession();
}
