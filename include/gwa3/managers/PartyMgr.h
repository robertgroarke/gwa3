#pragma once

#include <gwa3/game/Party.h>
#include <cstddef>
#include <cstdint>

namespace GWA3::PartyMgr {

    bool Initialize();

    // Context resolution (centralized for manager-level callers).
    uintptr_t ResolvePartyContext();
    PartyInfo* ResolvePlayerParty();

    // Hero management
    void AddHero(uint32_t heroId);
    void KickHero(uint32_t heroId);
    void KickAllHeroes();

    // Hero behavior and flagging
    void SetHeroBehavior(uint32_t heroIndex, uint32_t behavior);
    void FlagHero(uint32_t heroIndex, float x, float y);
    void FlagAll(float x, float y);
    void UnflagHero(uint32_t heroIndex);
    void UnflagAll();
    void LockHeroTarget(uint32_t heroIndex, uint32_t targetId);

    // Party management
    void Tick(bool ready);

    // State queries
    bool GetIsHardMode();
    bool GetIsPartyDefeated();
    uint32_t CountPartyHeroes();
    uint32_t GetCalledTargetId();
    size_t GetPartyHeroIds(uint32_t* out, size_t maxCount);
    void DebugDumpPartyState(const char* label);

} // namespace GWA3::PartyMgr
