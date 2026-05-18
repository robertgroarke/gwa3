bool TestHardModeToggle() {
    IntReport("=== Hard Mode Toggle ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        IntSkip("Hard mode toggle", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area) {
        IntSkip("Hard mode toggle", "AreaInfo unavailable");
        IntReport("");
        return false;
    }

    if (IsSkillCastMapType(area->type)) {
        IntSkip("Hard mode toggle", "Cannot toggle HM in explorable instance");
        IntReport("");
        return false;
    }

    IntReport("  Setting Hard Mode ON...");
    GameThread::Enqueue([]() {
        MapMgr::SetHardMode(true);
    });
    Sleep(1000);
    IntCheck("SetHardMode(true) sent (no crash)", true);

    IntReport("  Setting Hard Mode OFF...");
    GameThread::Enqueue([]() {
        MapMgr::SetHardMode(false);
    });
    Sleep(1000);
    IntCheck("SetHardMode(false) sent (no crash)", true);

    IntReport("");
    return true;
}
