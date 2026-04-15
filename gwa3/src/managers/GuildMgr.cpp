#include <gwa3/managers/GuildMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

namespace GWA3::GuildMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("GuildMgr: Initialized");
    return true;
}

// GuildContext: *BasePointer → +0x18 → +0x3C = GuildContext*
// AutoIt: [0, 0x18, 0x3C]
// Key offsets within GuildContext:
//   +0x60  player_guild_index
//   +0x64  player_gh_key (GHKey, 16 bytes)
//   +0x78  announcement (wchar_t[256])
//   +0x2F8 guilds (GWArray<Guild*>)
static uintptr_t ResolveGuildContext() {
    if (Offsets::BasePointer <= 0x10000) return 0;

    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx <= 0x10000) return 0;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (p1 <= 0x10000) return 0;
        uintptr_t guild = *reinterpret_cast<uintptr_t*>(p1 + 0x3C);
        if (guild <= 0x10000) return 0;
        return guild;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

GWArray<Guild*>* GetGuildArray() {
    uintptr_t gc = ResolveGuildContext();
    if (!gc) return nullptr;

    __try {
        auto* arr = reinterpret_cast<GWArray<Guild*>*>(gc + 0x2F8);
        if (!arr->buffer || arr->size == 0 || arr->size > 200) return nullptr;
        return arr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

uint32_t GetPlayerGuildIndex() {
    uintptr_t gc = ResolveGuildContext();
    if (!gc) return 0;

    __try {
        return *reinterpret_cast<uint32_t*>(gc + 0x60);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

Guild* GetPlayerGuild() {
    uint32_t idx = GetPlayerGuildIndex();
    if (idx == 0) return nullptr;
    return GetGuildInfo(idx);
}

Guild* GetGuildInfo(uint32_t guildId) {
    auto* arr = GetGuildArray();
    if (!arr || !arr->buffer) return nullptr;
    for (uint32_t i = 0; i < arr->size; ++i) {
        Guild* g = arr->buffer[i];
        if (g && g->index == guildId) return g;
    }
    return nullptr;
}

wchar_t* GetPlayerGuildAnnouncement() {
    uintptr_t gc = ResolveGuildContext();
    if (!gc) return nullptr;

    __try {
        wchar_t* ann = reinterpret_cast<wchar_t*>(gc + 0x78);
        if (ann[0] == L'\0') return nullptr;
        return ann;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool TravelGH() {
    CtoS::SendPacket(1, Packets::GUILDHALL_TRAVEL);
    return true;
}

bool TravelGH(const GHKey& key) {
    CtoS::SendPacket(5, Packets::GUILDHALL_TRAVEL, key.k[0], key.k[1], key.k[2], key.k[3]);
    return true;
}

} // namespace GWA3::GuildMgr
