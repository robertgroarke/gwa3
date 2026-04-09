#pragma once

#include <gwa3/game/Party.h>
#include <cstddef>
#include <cstdint>

namespace GWA3::PartyMgr {

    bool Initialize();

    // Hero management
    void AddHero(uint32_t heroId);
    void KickHero(uint32_t heroId);
    void KickAllHeroes();

    // Henchman management
    void AddHenchman(uint32_t henchmanId);
    void KickHenchman(uint32_t henchmanId);

    // Hero behavior and flagging
    void SetHeroBehavior(uint32_t heroIndex, uint32_t behavior);
    void FlagHero(uint32_t heroIndex, float x, float y);
    void FlagAll(float x, float y);
    void UnflagHero(uint32_t heroIndex);
    void UnflagAll();
    void LockHeroTarget(uint32_t heroIndex, uint32_t targetId);

    // Party management
    void LeaveParty();
    void InvitePlayer(uint32_t agentId);
    void KickPlayer(uint32_t playerId);
    void AcceptInvite(uint32_t partyId);
    void RefuseInvite(uint32_t partyId);
    void Tick(bool ready);

    // State queries
    bool GetIsPartyDefeated();
    uint32_t CountVisibleHeroes();
    uint32_t CountPartyHeroes();
    uint32_t GetCalledTargetId();
    size_t GetPartyHeroIds(uint32_t* out, size_t maxCount);
    void DebugDumpPartyState(const char* label);

} // namespace GWA3::PartyMgr
