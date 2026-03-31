#pragma once

#include <gwa3/game/Party.h>
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

} // namespace GWA3::PartyMgr
