bool WaitForConsumableTravelState(const char* waitStage, const char* targetLabel,
                                  uint32_t expectedMapId, uint32_t expectedRegion, uint32_t expectedDistrict,
                                  uint32_t timeoutMs) {
    const DWORD start = GetTickCount();
    DWORD lastStatusTick = 0;
    DWORD loadedOtherDistrictSince = 0;
    while ((GetTickCount() - start) < timeoutMs) {
        const uint32_t mapId = ReadMapId();
        const uint32_t myId = ReadMyId();
        const uint32_t region = MapMgr::GetRegion();
        const uint32_t district = MapMgr::GetDistrict();
        const uint32_t loading = MapMgr::GetLoadingState();
        const bool matched =
            mapId == expectedMapId &&
            region == expectedRegion &&
            district == expectedDistrict &&
            myId > 0;
        if (matched) return true;

        const bool loadedTargetOtherDistrict =
            mapId == expectedMapId &&
            region == expectedRegion &&
            district != expectedDistrict &&
            myId > 0 &&
            loading == 1;
        if (loadedTargetOtherDistrict) {
            if (loadedOtherDistrictSince == 0) {
                loadedOtherDistrictSince = GetTickCount();
            } else if ((GetTickCount() - loadedOtherDistrictSince) >= 3000) {
                return false;
            }
        } else {
            loadedOtherDistrictSince = 0;
        }

        const DWORD now = GetTickCount();
        if (lastStatusTick == 0 || (now - lastStatusTick) >= 1000) {
            char detail[192] = {};
            sprintf_s(detail,
                      "waiting map=%u region=%u district=%u myId=%u loading=%u expectedMap=%u expectedRegion=%u expectedDistrict=%u elapsedMs=%lu",
                      mapId,
                      region,
                      district,
                      myId,
                      loading,
                      expectedMapId,
                      expectedRegion,
                      expectedDistrict,
                      static_cast<unsigned long>(now - start));
            WriteConsumableHarnessStatus(waitStage, targetLabel, mapId, 0, 0, 0, 0, 0, 0, 0, detail);
            lastStatusTick = now;
        }
        Sleep(250);
    }
    return false;
}
