#pragma once

#include <gwa3/game/Map.h>
#include <cstdint>

namespace GWA3::MapMgr {

    bool Initialize();

    // Travel
    void Travel(uint32_t mapId, uint32_t region = 0, uint32_t district = 0, uint32_t language = 0);
    void TravelGuildHall();
    void LeaveGuildHall();
    void ReturnToOutpost();
    void EnterMission();
    void EnterChallenge();
    void CancelEnterChallenge();

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

    // Area info
    const AreaInfo* GetAreaInfo(uint32_t mapId);

} // namespace GWA3::MapMgr
