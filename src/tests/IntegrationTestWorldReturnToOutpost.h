bool TestReturnToOutpost() {
    IntReport("=== Return to Outpost ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        IntSkip("Return to outpost", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area || !IsSkillCastMapType(area->type)) {
        IntSkip("Return to outpost", "Not in explorable instance");
        IntReport("");
        return false;
    }

    IntReport("  Traveling to Gadd's Encampment (map %u) from explorable map %u...",
              MapIds::GADDS_ENCAMPMENT, mapId);
    GameThread::Enqueue([]() {
        MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
    });

    const bool returned = WaitFor("MapID changes after ReturnToOutpost", 60000, [mapId]() {
        const uint32_t newMap = ReadMapId();
        return newMap != 0 && newMap != mapId;
    });
    IntCheck("Left explorable instance", returned);

    if (returned) {
        const bool myIdReady = WaitFor("MyID valid after return to outpost", 30000, []() {
            return ReadMyId() > 0;
        });
        IntCheck("MyID valid after return to outpost", myIdReady);

        const uint32_t newMapId = ReadMapId();
        const AreaInfo* newArea = MapMgr::GetAreaInfo(newMapId);
        IntReport("  Returned to map %u type=%u (%s)",
                  newMapId,
                  newArea ? newArea->type : 0xFFFFFFFFu,
                  newArea ? DescribeMapRegionType(newArea->type) : "unknown");

        if (newArea) {
            IntCheck("Returned to outpost-type map", !IsSkillCastMapType(newArea->type));
        }
    }

    IntReport("");
    return returned;
}
