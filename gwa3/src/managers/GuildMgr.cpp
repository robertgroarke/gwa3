#include <gwa3/managers/GuildMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

namespace GWA3::GuildMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;

    // TODO: Resolve guild context pointer from Offsets when pattern is added
    s_initialized = true;
    Log::Info("GuildMgr: Initialized");
    return true;
}

Guild* GetPlayerGuild() {
    // TODO: Needs guild context offset — read player guild index,
    // then index into guild array
    return nullptr;
}

GWArray<Guild*>* GetGuildArray() {
    // TODO: Needs guild context offset
    return nullptr;
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

uint32_t GetPlayerGuildIndex() {
    // TODO: Needs guild context offset
    return 0;
}

wchar_t* GetPlayerGuildAnnouncement() {
    // TODO: Needs guild context offset
    return nullptr;
}

bool TravelGH() {
    CtoS::SendPacket(1, Packets::GUILDHALL_TRAVEL);
    return true;
}

bool TravelGH(const GHKey& key) {
    CtoS::SendPacket(5, Packets::GUILDHALL_TRAVEL, key.k[0], key.k[1], key.k[2], key.k[3]);
    return true;
}

bool LeaveGH() {
    CtoS::SendPacket(1, Packets::GUILDHALL_LEAVE);
    return true;
}

} // namespace GWA3::GuildMgr
