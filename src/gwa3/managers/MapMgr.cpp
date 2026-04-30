#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/MapIds.h>

#include <Windows.h>

namespace GWA3::MapMgr {

using EnterMissionFn = void(__cdecl*)();
using SetDifficultyFn = void(__cdecl*)(uint32_t);

namespace {

struct PendingTravelRequest {
    uint32_t mapId = 0;
    uint32_t region = 0;
    uint32_t district = 0;
    uint32_t language = 0;
    uint32_t originMapId = 0;
    DWORD queuedAt = 0;
    bool valid = false;
};

static PendingTravelRequest s_pendingTravel = {};

static bool IsSameTravelRequest(const PendingTravelRequest& pending,
                                uint32_t mapId,
                                uint32_t region,
                                uint32_t district,
                                uint32_t language) {
    return pending.valid &&
           pending.mapId == mapId &&
           pending.region == region &&
           pending.district == district &&
           pending.language == language;
}

static void RefreshPendingTravelState() {
    if (!s_pendingTravel.valid) return;

    const DWORD now = GetTickCount();
    const uint32_t currentMapId = GetMapId();
    if (currentMapId == s_pendingTravel.mapId ||
        (currentMapId != 0 && currentMapId != s_pendingTravel.originMapId) ||
        (now - s_pendingTravel.queuedAt) > 30000u) {
        s_pendingTravel = {};
    }
}

} // namespace

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

bool Travel(uint32_t mapId, uint32_t region, uint32_t district, uint32_t language) {
    Log::Info("MapMgr: Travel request map=%u region=%u district=%u language=%u", mapId, region, district, language);
    RefreshPendingTravelState();
    if (GameThread::IsOnGameThread()) {
        s_pendingTravel = {};
        Log::Info("MapMgr: Travel using direct game-thread packet send");
        CtoS::MapTravel(mapId, region, district, language);
        Log::Info("MapMgr: Travel direct send returned");
        return true;
    }

    if (IsSameTravelRequest(s_pendingTravel, mapId, region, district, language)) {
        Log::Warn("MapMgr: Travel duplicate suppressed map=%u originMap=%u age=%lums",
                  mapId,
                  s_pendingTravel.originMapId,
                  static_cast<unsigned long>(GetTickCount() - s_pendingTravel.queuedAt));
        return true;
    }

    if (!GameThread::IsResponsive(5000u)) {
        Log::Warn("MapMgr: Travel refused for map=%u because GameThread detour is not responsive", mapId);
        return false;
    }

    const uint32_t pendingPreCount = GameThread::GetPendingPreCount();
    if (pendingPreCount >= 240u) {
        Log::Warn("MapMgr: Travel refused for map=%u because GameThread prequeue is saturated (%u pending)",
                  mapId, pendingPreCount);
        return false;
    }

    s_pendingTravel.mapId = mapId;
    s_pendingTravel.region = region;
    s_pendingTravel.district = district;
    s_pendingTravel.language = language;
    s_pendingTravel.originMapId = GetMapId();
    s_pendingTravel.queuedAt = GetTickCount();
    s_pendingTravel.valid = true;

    Log::Info("MapMgr: Travel enqueueing to GameThread");
    GameThread::Enqueue([mapId, region, district, language]() {
        Log::Info("MapMgr: Travel lambda running on GameThread");
        CtoS::MapTravel(mapId, region, district, language);
        Log::Info("MapMgr: Travel lambda returned");
    });
    Log::Info("MapMgr: Travel enqueue returned to caller");
    return true;
}

void ReturnToOutpost() {
    uint32_t mapId = GetMapId();
    if (mapId != 0 && !GetIsMapLoaded()) {
        // Natural post-reward transitions out of Bogroot can leave the
        // client in loaded=0 for 1-2 minutes. Give the server a generous
        // window before falling through to the packet path — sending
        // packets into the transition crashes Gw.exe.
        const DWORD settleMs =
            (mapId == MapIds::BOGROOT_GROWTHS_LVL1 || mapId == MapIds::BOGROOT_GROWTHS_LVL2) ? 180000u : 30000u;
        Log::Info("MapMgr: ReturnToOutpost waiting for map load to settle map=%u budget=%lums",
                  mapId, static_cast<unsigned long>(settleMs));
        const DWORD start = GetTickCount();
        while ((GetTickCount() - start) < settleMs) {
            if (GetIsMapLoaded()) {
                break;
            }
            Sleep(250);
        }
        mapId = GetMapId();
        Log::Info("MapMgr: ReturnToOutpost settled map=%u loaded=%d myId=%u",
                  mapId,
                  GetIsMapLoaded() ? 1 : 0,
                  AgentMgr::GetMyId());
        // If the load never completed, the client is in a transition
        // ghost state. Sending any map/return packet into that state
        // reliably triggers a Gw.exe crash dialog — bail out instead.
        if (!GetIsMapLoaded()) {
            Log::Warn("MapMgr: ReturnToOutpost bailing — load did not complete within budget; skipping packet to avoid crash");
            return;
        }
    }

    const AreaInfo* area = GetAreaInfo(mapId);
    const bool inExplorable = area && IsExplorableMapRegionType(area->type);

    if (!inExplorable) {
        Log::Info("MapMgr: ReturnToOutpost sending direct packet outside explorable");
        CtoS::SendPacket(1, Packets::PARTY_RETURN_TO_OUTPOST);
        return;
    }

    Log::Info("MapMgr: ReturnToOutpost using /resign + button flow in explorable map %u", mapId);
    ChatMgr::SendChat(L"resign", L'/');

    // Match the previously working test path: allow resign and party defeat
    // state to settle before looking for the Return to Outpost button.
    Sleep(5000);

    for (int attempt = 1; attempt <= 10; ++attempt) {
        if (UIMgr::IsFrameVisible(UIMgr::Hashes::ReturnToOutpost)) {
            Log::Info("MapMgr: ReturnToOutpost button visible on attempt %d; clicking hash=%u",
                      attempt, UIMgr::Hashes::ReturnToOutpost);
            if (UIMgr::ButtonClickByHash(UIMgr::Hashes::ReturnToOutpost)) {
                return;
            }
            Log::Warn("MapMgr: ReturnToOutpost button click failed on attempt %d", attempt);
        } else {
            Log::Info("MapMgr: ReturnToOutpost button not yet visible (attempt %d)", attempt);
        }
        Sleep(500);
    }

    Log::Warn("MapMgr: ReturnToOutpost button never appeared; falling back to direct packet");
    CtoS::SendPacket(1, Packets::PARTY_RETURN_TO_OUTPOST);
}

void EnterMission() {
    if (s_enterMissionFn) {
        GameThread::Enqueue([]() { s_enterMissionFn(); });
    } else {
        CtoS::SendPacket(1, Packets::PARTY_ENTER_CHALLENGE);
    }
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
    // Prefer native function (handles edge cases like checking cinematic state)
    if (Offsets::SkipCinematicFunc && Offsets::SkipCinematicFunc > 0x10000) {
        auto fn = reinterpret_cast<void(__cdecl*)()>(Offsets::SkipCinematicFunc);
        GameThread::Enqueue([fn]() { fn(); });
        return;
    }
    CtoS::SendPacket(1, Packets::CINEMATIC_SKIP);
}

uint32_t GetMapId() {
    // MapID via BasePointer chain: *BasePointer -> +0x18 -> +0x44 -> +0x198
    if (!Offsets::BasePointer) return 0;
    uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
    if (ctx < 0x10000) return 0;
    uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
    if (p1 < 0x10000) return 0;
    uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x44);
    if (p2 < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(p2 + 0x198);
}

uint32_t GetRegion() {
    if (!Offsets::Region || Offsets::Region < 0x10000) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(Offsets::Region);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

uint32_t GetDistrict() {
    // AutoIt: BasePointer → +0x18 → +0x44 → +0x220
    if (!Offsets::BasePointer) return 0;
    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx < 0x10000) return 0;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (p1 < 0x10000) return 0;
        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x44);
        if (p2 < 0x10000) return 0;
        return *reinterpret_cast<uint32_t*>(p2 + 0x220);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

uint32_t GetInstanceTime() {
    // AutoIt: time_on_map_ptr = scene_context_ptr + 0xC
    // Offsets::SceneContext is already post-processed (one deref done at scan+0x1B)
    // So time = *(SceneContext + 0xC) — direct read, no extra deref
    if (!Offsets::SceneContext || Offsets::SceneContext < 0x10000) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(Offsets::SceneContext + 0xC);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool GetIsMapLoaded() {
    return GetLoadingState() == 1;
}

uint32_t GetLoadingState() {
    // 0 = loading (map_id > 0 but agent not ready)
    // 1 = loaded (map_id > 0 and agent exists)
    // 2 = disconnected / no map (map_id == 0)
    uint32_t mapId = GetMapId();
    if (mapId == 0) return 2;
    uint32_t myId = AgentMgr::GetMyId();
    if (myId == 0) return 0;
    if (AgentMgr::GetAgentByID(myId) == nullptr) return 0;
    return 1;
}

// GameContext resolution delegated to Offsets::ResolveGameContext()
// CharContext at GameContext+0x44, Cinematic at GameContext+0x30

bool GetIsObserving() {
    // CharContext at GameContext+0x44
    // observe_map_id at CharContext+0x228, current_map_id at +0x22C
    uintptr_t gc = Offsets::ResolveGameContext();
    if (!gc) return false;

    __try {
        uintptr_t charCtx = *reinterpret_cast<uintptr_t*>(gc + 0x44);
        if (charCtx <= 0x10000) return false;
        uint32_t observeMap = *reinterpret_cast<uint32_t*>(charCtx + 0x228);
        uint32_t currentMap = *reinterpret_cast<uint32_t*>(charCtx + 0x22C);
        return observeMap != currentMap;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GetIsInCinematic() {
    // Cinematic* at GameContext+0x30, check h0004 != 0
    uintptr_t gc = Offsets::ResolveGameContext();
    if (!gc) return false;

    __try {
        uintptr_t cinematic = *reinterpret_cast<uintptr_t*>(gc + 0x30);
        if (cinematic <= 0x10000) return false;
        uint32_t data = *reinterpret_cast<uint32_t*>(cinematic + 0x04);
        return data != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

const AreaInfo* GetAreaInfo(uint32_t mapId) {
    if (!Offsets::AreaInfo) return nullptr;
    auto* base = reinterpret_cast<AreaInfo*>(Offsets::AreaInfo);
    if (!base) return nullptr;
    return &base[mapId];
}

} // namespace GWA3::MapMgr
