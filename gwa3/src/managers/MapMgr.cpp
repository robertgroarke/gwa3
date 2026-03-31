#include <gwa3/managers/MapMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

namespace GWA3::MapMgr {

using EnterMissionFn = void(__cdecl*)();
using SetDifficultyFn = void(__cdecl*)(uint32_t);

static EnterMissionFn s_enterMissionFn = nullptr;
static SetDifficultyFn s_setDifficultyFn = nullptr;
static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::EnterMission) s_enterMissionFn = reinterpret_cast<EnterMissionFn>(Offsets::EnterMission);
    if (Offsets::SetDifficulty) s_setDifficultyFn = reinterpret_cast<SetDifficultyFn>(Offsets::SetDifficulty);

    s_initialized = true;
    Log::Info("MapMgr: Initialized");
    return true;
}

void Travel(uint32_t mapId, uint32_t region, uint32_t district, uint32_t language) {
    CtoS::MapTravel(mapId, region, district, language);
}

void TravelGuildHall() {
    CtoS::SendPacket(1, Packets::GUILDHALL_TRAVEL);
}

void LeaveGuildHall() {
    CtoS::SendPacket(1, Packets::GUILDHALL_LEAVE);
}

void ReturnToOutpost() {
    CtoS::SendPacket(1, Packets::PARTY_RETURN_TO_OUTPOST);
}

void EnterMission() {
    if (s_enterMissionFn) {
        GameThread::Enqueue([]() { s_enterMissionFn(); });
    } else {
        CtoS::SendPacket(1, Packets::PARTY_ENTER_CHALLENGE);
    }
}

void EnterChallenge() {
    CtoS::SendPacket(1, Packets::PARTY_ENTER_CHALLENGE);
}

void CancelEnterChallenge() {
    CtoS::SendPacket(1, Packets::PARTY_CANCEL_ENTER_CHALLENGE);
}

void SetHardMode(bool enabled) {
    if (s_setDifficultyFn) {
        uint32_t mode = enabled ? 1u : 0u;
        GameThread::Enqueue([mode]() { s_setDifficultyFn(mode); });
    } else {
        CtoS::SendPacket(2, Packets::SET_DIFFICULTY, enabled ? 1u : 0u);
    }
}

void SkipCinematic() {
    CtoS::SendPacket(1, Packets::CINEMATIC_SKIP);
}

uint32_t GetMapId() {
    if (!Offsets::InstanceInfo) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::InstanceInfo);
}

uint32_t GetRegion() {
    if (!Offsets::Region) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::Region);
}

uint32_t GetDistrict() {
    if (!Offsets::Region) return 0;
    // District is typically at Region + 4
    return *reinterpret_cast<uint32_t*>(Offsets::Region + 4);
}

uint32_t GetInstanceTime() {
    if (!Offsets::SceneContext) return 0;
    // TimeOnMap is at SceneContext + 0xC
    uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::SceneContext);
    if (!ctx) return 0;
    return *reinterpret_cast<uint32_t*>(ctx + 0xC);
}

bool GetIsMapLoaded() {
    if (!Offsets::StatusCode) return false;
    uint32_t status = *reinterpret_cast<uint32_t*>(Offsets::StatusCode);
    return status != 0;
}

bool GetIsObserving() {
    // TODO: implement when observation state offset is known
    return false;
}

bool GetIsInCinematic() {
    // TODO: implement when cinematic state offset is known
    return false;
}

const AreaInfo* GetAreaInfo(uint32_t mapId) {
    if (!Offsets::AreaInfo) return nullptr;
    auto* base = *reinterpret_cast<AreaInfo**>(Offsets::AreaInfo);
    if (!base) return nullptr;
    return &base[mapId];
}

} // namespace GWA3::MapMgr
