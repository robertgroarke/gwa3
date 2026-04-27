#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <cstring>
#include <Windows.h>

namespace GWA3::PlayerMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("PlayerMgr: Initialized");
    return true;
}

// ===== Title Management =====

bool SetActiveTitle(uint32_t titleId) {
    CtoS::SendPacket(2, Packets::TITLE_DISPLAY, titleId);
    return true;
}

bool RemoveActiveTitle() {
    CtoS::SendPacket(1, Packets::TITLE_HIDE);
    return true;
}

// WorldContext resolution delegated to Offsets::ResolveWorldContext()
// Chain: *BasePointer → +0x18 → +0x2C = WorldContext*

// TitleArray at WorldContext + 0x81C (GWCA WorldContext.h)
static GWArray<Title>* GetTitleArray() {
    uintptr_t world = Offsets::ResolveWorldContext();
    if (!world) return nullptr;

    __try {
        return reinterpret_cast<GWArray<Title>*>(world + 0x81C);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

Title* GetTitleTrack(uint32_t titleId) {
    auto* arr = GetTitleArray();
    if (!arr || !arr->buffer || titleId >= arr->size) return nullptr;
    return &arr->buffer[titleId];
}

TitleClientData* GetTitleData(uint32_t titleId) {
    // TitleClientData is a flat static array in .rdata, indexed by titleId.
    // Each entry is 12 bytes (sizeof(TitleClientData)).
    if (Offsets::TitleClientDataBase <= 0x10000) return nullptr;
    if (titleId > 200) return nullptr; // sanity bound

    __try {
        auto* base = reinterpret_cast<TitleClientData*>(Offsets::TitleClientDataBase);
        return &base[titleId];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

uint32_t GetActiveTitleId() {
    // The active title ID is stored in the player's own Player struct.
    Player* self = GetPlayerByID(0);
    if (!self) return 0;
    return self->active_title_tier;
}

Title* GetActiveTitle() {
    uint32_t id = GetActiveTitleId();
    if (id == 0) return nullptr;
    return GetTitleTrack(id);
}

// ===== Player Data =====

// PlayerArray at WorldContext + 0x80C (GWCA WorldContext.h)
// AutoIt: [0, 0x18, 0x2C, 0x80C]
static GWArray<Player>* ResolvePlayerArray() {
    uintptr_t world = Offsets::ResolveWorldContext();
    if (!world) return nullptr;

    __try {
        auto* arr = reinterpret_cast<GWArray<Player>*>(world + 0x80C);
        if (!arr->buffer || arr->size == 0 || arr->size > 1000) return nullptr;
        return arr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

Player* GetPlayerByID(uint32_t playerId) {
    auto* arr = ResolvePlayerArray();
    if (!arr || !arr->buffer) return nullptr;

    if (playerId == 0) {
        // Self: use MyID offset to find our agent, then look up in player array
        if (!Offsets::MyID) return nullptr;
        // PlayerNumber is what indexes into the player array
        // We need to find which player has our agent ID
        uint32_t myAgentId = *reinterpret_cast<uint32_t*>(Offsets::MyID);
        for (uint32_t i = 0; i < arr->size; ++i) {
            if (arr->buffer[i].agent_id == myAgentId) {
                return &arr->buffer[i];
            }
        }
        return nullptr;
    }

    if (playerId >= arr->size) return nullptr;
    return &arr->buffer[playerId];
}

wchar_t* GetPlayerName(uint32_t playerId) {
    Player* p = GetPlayerByID(playerId);
    if (!p) return nullptr;
    return p->name;
}

Player* GetPlayerByName(const wchar_t* name) {
    if (!name) return nullptr;
    auto* arr = ResolvePlayerArray();
    if (!arr || !arr->buffer) return nullptr;

    for (uint32_t i = 0; i < arr->size; ++i) {
        if (arr->buffer[i].name && wcscmp(arr->buffer[i].name, name) == 0) {
            return &arr->buffer[i];
        }
    }
    return nullptr;
}

GWArray<Player>* GetPlayerArray() {
    return ResolvePlayerArray();
}

uint32_t GetPlayerNumber() {
    // PlayerNumber is the index of the player in the player array.
    // GWCA resolves this via context. We can derive from MyID.
    if (!Offsets::MyID) return 0;
    uint32_t myAgentId = *reinterpret_cast<uint32_t*>(Offsets::MyID);

    auto* arr = ResolvePlayerArray();
    if (!arr || !arr->buffer) return 0;

    for (uint32_t i = 0; i < arr->size; ++i) {
        if (arr->buffer[i].agent_id == myAgentId) {
            return arr->buffer[i].player_number;
        }
    }
    return 0;
}

uint32_t GetPlayerAgentId(uint32_t playerId) {
    Player* p = GetPlayerByID(playerId);
    if (!p) return 0;
    return p->agent_id;
}

uint32_t GetAmountOfPlayersInInstance() {
    auto* arr = ResolvePlayerArray();
    if (!arr) return 0;
    return arr->size;
}

} // namespace GWA3::PlayerMgr
