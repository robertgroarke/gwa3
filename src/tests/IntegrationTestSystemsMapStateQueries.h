bool TestMapStateQueries() {
    IntReport("=== Map State Queries ===");

    if (ReadMyId() == 0) {
        IntSkip("Map state queries", "Not in game");
        IntReport("");
        return false;
    }

    const bool observing = MapMgr::GetIsObserving();
    IntReport("  IsObserving: %d", observing);
    IntCheck("Not in observer mode (expected for bot account)", !observing);

    const bool cinematic = MapMgr::GetIsInCinematic();
    IntReport("  IsInCinematic: %d", cinematic);
    IntCheck("Not in cinematic (expected during test)", !cinematic);

    const bool mapLoaded = MapMgr::GetIsMapLoaded();
    IntReport("  IsMapLoaded: %d", mapLoaded);
    IntCheck("Map is loaded", mapLoaded);

    const uint32_t instanceTime = MapMgr::GetInstanceTime();
    IntReport("  InstanceTime: %u ms", instanceTime);
    IntCheck("Instance time > 0", instanceTime > 0);

    IntReport("");
    return true;
}
