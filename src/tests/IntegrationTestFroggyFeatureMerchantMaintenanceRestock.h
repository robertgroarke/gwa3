static bool WaitForRestockTargets(const MaintenanceMgr::Config& cfg, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (CountSuperiorIdKits() >= cfg.targetIdKits && CountSalvageKitFamily() >= cfg.targetSalvageKits) {
            return true;
        }
        Sleep(250);
    }
    return CountSuperiorIdKits() >= cfg.targetIdKits && CountSalvageKitFamily() >= cfg.targetSalvageKits;
}

static void RestockMerchantMaintenanceKits() {
    MaintenanceMgr::Config cfg = {};
    cfg.targetIdKits = 3;
    cfg.targetSalvageKits = 10;
    MaintenanceMgr::BuyKitsToTarget(cfg);
    const bool restocked = WaitForRestockTargets(cfg, 6000);
    FroggyFeatureCheck("Maintenance reaches superior ID kit target", CountSuperiorIdKits() >= cfg.targetIdKits);
    FroggyFeatureCheck("Maintenance reaches salvage kit target", CountSalvageKitFamily() >= cfg.targetSalvageKits);
    FroggyFeatureCheck("Maintenance restock settles", restocked);
}
