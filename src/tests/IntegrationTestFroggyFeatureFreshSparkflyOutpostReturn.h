static bool EnsureAtGaddsForFreshSparkflyReset() {
    if (MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP) {
        FroggyFeatureReport("  Returning to Gadd's to start the real dungeon path from a clean Sparkfly instance...");
        MapMgr::ReturnToOutpost();
        const bool returned = WaitFor("MapID == Gadd's after Sparkfly reset", 60000, []() {
            return MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT;
        });
        FroggyFeatureCheck("Returned to Gadd's for fresh Sparkfly run", returned);
        return returned;
    }

    if (MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT) {
        return true;
    }

    FroggyFeatureReport("  Traveling to Gadd's from map %u before fresh Sparkfly reset...", MapMgr::GetMapId());
    MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
    const bool arrived = WaitFor("MapID == Gadd's for Sparkfly reset", 60000, []() {
        return MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT;
    });
    FroggyFeatureCheck("Traveled to Gadd's for fresh Sparkfly run", arrived);
    return arrived;
}
