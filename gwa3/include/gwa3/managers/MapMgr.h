#pragma once

#include <gwa3/game/Map.h>
#include <cstdint>

namespace GWA3::MapMgr {

    bool Initialize();

    // Travel
    bool Travel(uint32_t mapId, uint32_t region = 0, uint32_t district = 0, uint32_t language = 0);
    void ReturnToOutpost();
    void EnterMission();

    // Difficulty
    void SetHardMode(bool enabled);

    // Cinematic
    void SkipCinematic();

    // Instance data
    uint32_t GetMapId();
    uint32_t GetRegion();
    uint32_t GetDistrict();
    uint32_t GetInstanceTime();
    bool GetIsMapLoaded();
    bool GetIsObserving();
    bool GetIsInCinematic();

    // 3-state loading: 0=loading/not loaded, 1=loaded, 2=disconnected/no map
    uint32_t GetLoadingState();

    // Area info
    const AreaInfo* GetAreaInfo(uint32_t mapId);

} // namespace GWA3::MapMgr
