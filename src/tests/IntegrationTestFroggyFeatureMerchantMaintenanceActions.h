struct MerchantMaintenanceActionCounts {
    uint32_t identified = 0;
    uint32_t salvaged = 0;
    uint32_t sold = 0;
};

static uint32_t RunMaintenanceIdentifyPass(const MerchantMaintenanceBeforeState& before) {
    const uint32_t identified = MaintenanceMgr::IdentifyAllItems();
    if (before.unidentifiedItems > 0) {
        FroggyFeatureCheck("Maintenance identifies pending items", identified > 0);
    } else {
        FroggyFeatureCheck("Maintenance identify pass stays safe with no pending items", identified == 0);
    }
    return identified;
}

static uint32_t RunMaintenanceSalvagePass(bool includeSalvage) {
    if (!includeSalvage) {
        return 0;
    }

    const uint32_t salvageCandidatesBeforeSalvage = CountSalvageCandidatesForMaintenance();
    const uint32_t salvaged = MaintenanceMgr::SalvageJunkItems();
    if (salvageCandidatesBeforeSalvage > 0) {
        FroggyFeatureCheck("Maintenance salvages pending junk", salvaged > 0);
    } else {
        FroggyFeatureCheck("Maintenance salvage pass stays safe with no candidates", salvaged == 0);
    }
    return salvaged;
}

static uint32_t RunMaintenanceSellPass(const MerchantMaintenanceBeforeState& before) {
    const uint32_t sold = MaintenanceMgr::SellJunkItems();
    if (before.sellCandidates > 0) {
        FroggyFeatureCheck("Maintenance sells pending junk", sold > 0);
    } else {
        FroggyFeatureCheck("Maintenance sell pass stays safe with no candidates", sold == 0);
    }
    return sold;
}

static MerchantMaintenanceActionCounts RunMerchantMaintenanceItemActions(
    const MerchantMaintenanceBeforeState& before,
    bool includeSalvage) {
    MerchantMaintenanceActionCounts counts = {};
    counts.identified = RunMaintenanceIdentifyPass(before);
    counts.salvaged = RunMaintenanceSalvagePass(includeSalvage);
    counts.sold = RunMaintenanceSellPass(before);
    return counts;
}
