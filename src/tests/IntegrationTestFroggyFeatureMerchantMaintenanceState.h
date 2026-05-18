struct MerchantMaintenanceBeforeState {
    uint32_t freeSlots = 0;
    uint32_t gold = 0;
    uint32_t superiorIdKits = 0;
    uint32_t salvageKits = 0;
    uint32_t unidentifiedItems = 0;
    uint32_t salvageCandidates = 0;
    uint32_t sellCandidates = 0;
};

static MerchantMaintenanceBeforeState CaptureMerchantMaintenanceBeforeState(uint32_t openedMerchantId) {
    MerchantMaintenanceBeforeState state = {};
    state.freeSlots = MaintenanceMgr::CountFreeSlots();
    state.gold = ItemMgr::GetGoldCharacter();
    state.superiorIdKits = CountSuperiorIdKits();
    state.salvageKits = CountSalvageKitFamily();
    state.unidentifiedItems = CountUnidentifiedMaintenanceItems();
    state.salvageCandidates = CountSalvageCandidatesForMaintenance();
    state.sellCandidates = CountSellCandidatesForMaintenance();
    FroggyFeatureReport("  Before maintenance: free=%u gold=%u superiorId=%u salvage=%u unidentified=%u salvageCandidates=%u sellCandidates=%u merchantAgent=%u",
              state.freeSlots, state.gold, state.superiorIdKits, state.salvageKits,
              state.unidentifiedItems, state.salvageCandidates, state.sellCandidates, openedMerchantId);
    return state;
}

static void ReportMerchantMaintenanceAfterState(uint32_t identified, uint32_t salvaged, uint32_t sold) {
    const uint32_t freeAfter = MaintenanceMgr::CountFreeSlots();
    const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
    FroggyFeatureReport("  After maintenance: free=%u gold=%u superiorId=%u salvage=%u identified=%u salvaged=%u sold=%u",
              freeAfter, goldAfter, CountSuperiorIdKits(), CountSalvageKitFamily(),
              identified, salvaged, sold);
}

static void CleanupMerchantMaintenanceSession() {
    FroggyFeatureReport("  Final merchant cleanup: CancelAction()");
    AgentMgr::CancelAction();
    FroggyFeatureReport("  Final merchant cleanup: CancelAction complete");
    Sleep(500);
}
