static void RunFroggySparkflyReturnOrPreserveCleanup(bool preserveSparkflyLoopState) {
    if (preserveSparkflyLoopState) {
        FroggyFeatureReport("=== PHASE 7: End State / Cleanup ===");
        FroggyFeatureReport("  Preserving Sparkfly state after reward/reaccept; skipping outpost cleanup");
        FroggyFeatureCheck("Loop state preserved in Sparkfly after reward/reaccept",
                    MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP &&
                    MapMgr::GetIsMapLoaded() &&
                    AgentMgr::GetMyId() > 0);
        FroggyFeatureSkip("Returned to Gadd's Encampment",
                   "Preserving Sparkfly state for real Froggy loop continuation");
        FroggyFeatureSkip("PHASE 7B: Post-Run Identify Salvage Sell Restock",
                   "Preserving Sparkfly state for real Froggy loop continuation");
        return;
    }

    FroggyFeatureReport("=== PHASE 7: Return to Outpost ===");
    const uint32_t currentMap = MapMgr::GetMapId();
    if (currentMap == MapIds::BOGROOT_GROWTHS_LVL1) {
        FroggyFeatureReport("  In Bogroot dungeon (map=%u), using Travel to return", currentMap);
        MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
    } else {
        MapMgr::ReturnToOutpost();
    }
    const bool returned = WaitFor("MapID == Gadd's after return", 120000, []() {
        return MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT &&
               MapMgr::GetIsMapLoaded() &&
               AgentMgr::GetMyId() > 0;
    });
    FroggyFeatureCheck("Returned to Gadd's Encampment", returned);
    if (returned) {
        RunMerchantMaintenanceCycle("PHASE 7B: Post-Run Identify Salvage Sell Restock", true);
    } else {
        FroggyFeatureSkip("PHASE 7B: Post-Run Identify Salvage Sell Restock", "Did not reach Gadd's after run");
    }
}
