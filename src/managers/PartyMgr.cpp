#include <gwa3/managers/PartyMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <cmath>
#include <Windows.h>

namespace GWA3::PartyMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("PartyMgr: Initialized");
    return true;
}

void AddHero(uint32_t heroId)    { CtoS::HeroAdd(heroId); }
void KickHero(uint32_t heroId)   { CtoS::HeroKick(heroId); }
void KickAllHeroes()             { CtoS::SendPacket(2, Packets::HERO_KICK, 0u); }

void AddHenchman(uint32_t id)    { CtoS::SendPacket(2, Packets::PARTY_INVITE_NPC, id); }
void KickHenchman(uint32_t id)   { CtoS::SendPacket(2, Packets::PARTY_KICK_NPC, id); }

void SetHeroBehavior(uint32_t heroIndex, uint32_t behavior) {
    CtoS::HeroBehavior(heroIndex, behavior);
}

void FlagHero(uint32_t heroIndex, float x, float y) {
    CtoS::HeroFlagSingle(heroIndex, x, y);
}

void FlagAll(float x, float y) {
    CtoS::HeroFlagAll(x, y);
}

void UnflagHero(uint32_t heroIndex) {
    // Flag to invalid coords to cancel
    CtoS::HeroFlagSingle(heroIndex, INFINITY, INFINITY); // NaN/inf
}

void UnflagAll() {
    CtoS::HeroFlagAll(INFINITY, INFINITY);
}

void LockHeroTarget(uint32_t heroIndex, uint32_t targetId) {
    CtoS::SendPacket(3, Packets::HERO_LOCK_TARGET, heroIndex, targetId);
}

void LeaveParty() {
    CtoS::SendPacket(1, Packets::PARTY_LEAVE);
}

void InvitePlayer(uint32_t agentId) {
    CtoS::SendPacket(2, Packets::PARTY_INVITE_PLAYER, agentId);
}

void KickPlayer(uint32_t playerId) {
    CtoS::SendPacket(2, Packets::PARTY_KICK_PLAYER, playerId);
}

void AcceptInvite(uint32_t partyId) {
    CtoS::SendPacket(2, Packets::PARTY_ACCEPT_INVITE, partyId);
}

void RefuseInvite(uint32_t partyId) {
    CtoS::SendPacket(2, Packets::PARTY_ACCEPT_REFUSE, partyId);
}

void Tick(bool ready) {
    CtoS::SendPacket(2, Packets::PARTY_READY_STATUS, ready ? 1u : 0u);
}

// PartyContext flags: *BasePointer → +0x18 → +0x4C → +0x14
// Bit 0x10 = Hard Mode, 0x20 = Defeated, 0x80 = Party Leader
static uint32_t ReadPartyFlags() {
    if (Offsets::BasePointer <= 0x10000) return 0;

    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx <= 0x10000) return 0;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (p1 <= 0x10000) return 0;
        uintptr_t party = *reinterpret_cast<uintptr_t*>(p1 + 0x4C);
        if (party <= 0x10000) return 0;
        return *reinterpret_cast<uint32_t*>(party + 0x14);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool GetIsPartyDefeated() {
    return (ReadPartyFlags() & 0x20) != 0;
}

} // namespace GWA3::PartyMgr
