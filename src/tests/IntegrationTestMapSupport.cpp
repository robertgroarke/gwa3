#include "IntegrationTestInternal.h"

#include <gwa3/managers/MapMgr.h>

#include <Windows.h>
#include <cstdint>

namespace GWA3::SmokeTest {

bool WaitForStablePlayerState(int timeoutMs) {
    const DWORD start = GetTickCount();
    int consecutiveReady = 0;
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        if (IsPlayerRuntimeReady(true)) {
            if (++consecutiveReady >= 4) return true;
        } else {
            consecutiveReady = 0;
        }
        Sleep(500);
    }
    IntReport("  Timeout waiting for: player runtime state ready (%d ms)", timeoutMs);
    return false;
}

bool WaitForPlayerWorldReady(int timeoutMs) {
    const DWORD start = GetTickCount();
    int consecutiveReady = 0;
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        if (IsPlayerRuntimeReady(false)) {
            if (++consecutiveReady >= 4) return true;
        } else {
            consecutiveReady = 0;
        }
        Sleep(500);
    }
    IntReport("  Timeout waiting for: player world state ready (%d ms)", timeoutMs);
    return false;
}

bool WaitForSessionHydrationIfNeeded() {
    const uint32_t mapId = ReadMapId();
    if (mapId == 0) return false;

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area) {
        IntReport("  Session hydration: AreaInfo unavailable for map %u", mapId);
        return false;
    }

    IntReport("  Session hydration check: Map %u regionType=%u (%s)",
              mapId,
              area->type,
              DescribeMapRegionType(area->type));

    // Outpost-like maps are already covered well enough by bootstrap;
    // the long-tail flakiness is specific to sessions that reconnect
    // directly into an explorable instance.
    if (!IsSkillCastMapType(area->type)) {
        return true;
    }

    IntReport("  Session hydration: waiting for explorable runtime state...");
    const DWORD start = GetTickCount();
    int consecutiveReady = 0;
    bool ready = false;
    while ((GetTickCount() - start) < 45000) {
        if (IsPlayerRuntimeReady(true)) {
            if (++consecutiveReady >= 6) {
                ready = true;
                break;
            }
        } else {
            consecutiveReady = 0;
        }
        Sleep(500);
    }
    if (!ready) {
        IntReport("  Session hydration after wait: TypeMap=0x%X ModelState=%u",
                  GetPlayerTypeMap(),
                  GetPlayerModelState());
    } else {
        IntReport("  Session hydration complete: TypeMap=0x%X ModelState=%u",
                  GetPlayerTypeMap(),
                  GetPlayerModelState());
    }
    return ready;
}

const char* DescribeMapRegionType(uint32_t type) {
    switch (static_cast<MapRegionType>(type)) {
    case MapRegionType::ExplorableZone: return "ExplorableZone";
    case MapRegionType::MissionOutpost: return "MissionOutpost";
    case MapRegionType::CooperativeMission: return "CooperativeMission";
    case MapRegionType::EliteMission: return "EliteMission";
    case MapRegionType::Challenge: return "Challenge";
    case MapRegionType::Outpost: return "Outpost";
    case MapRegionType::City: return "City";
    case MapRegionType::MissionArea: return "MissionArea";
    case MapRegionType::EotnMission: return "EotnMission";
    case MapRegionType::Dungeon: return "Dungeon";
    case MapRegionType::Marketplace: return "Marketplace";
    default: return "Other";
    }
}

bool IsSkillCastMapType(uint32_t type) {
    switch (static_cast<MapRegionType>(type)) {
    case MapRegionType::ExplorableZone:
    case MapRegionType::CooperativeMission:
    case MapRegionType::CompetitiveMission:
    case MapRegionType::EliteMission:
    case MapRegionType::Challenge:
    case MapRegionType::MissionArea:
    case MapRegionType::HeroBattleArea:
    case MapRegionType::EotnMission:
    case MapRegionType::Dungeon:
        return true;
    default:
        return false;
    }
}

bool IsCurrentMapSkillCastable() {
    const uint32_t mapId = ReadMapId();
    if (mapId == 0) return false;

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    return area && IsSkillCastMapType(area->type);
}

} // namespace GWA3::SmokeTest