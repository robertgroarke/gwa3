#include <gwa3/managers/PartyMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/PlayerMgr.h>

#include <cmath>
#include <vector>
#include <Windows.h>

namespace GWA3::PartyMgr {

static bool s_initialized = false;
using AddHeroFn = void(__cdecl*)(uint32_t);
static AddHeroFn s_addHeroFn = nullptr;

static uintptr_t ResolvePartyContext() {
    if (Offsets::BasePointer <= 0x10000) return 0;

    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx <= 0x10000) return 0;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (p1 <= 0x10000) return 0;
        uintptr_t party = *reinterpret_cast<uintptr_t*>(p1 + 0x4C);
        if (party <= 0x10000) return 0;
        return party;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static PartyInfo* ResolvePlayerParty() {
    const uintptr_t partyContext = ResolvePartyContext();
    if (partyContext <= 0x10000) return nullptr;

    __try {
        uintptr_t playerParty = *reinterpret_cast<uintptr_t*>(partyContext + 0x54);
        if (playerParty <= 0x10000) return nullptr;
        return reinterpret_cast<PartyInfo*>(playerParty);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool Initialize() {
    if (s_initialized) return true;

    uintptr_t addHero = Scanner::Find(
        "\x55\x8B\xEC\x56\x8B\x75\x08\x83\xFE\x26\x7C\x0C\x68\x0C\x11\x00\x00\xB9",
        "xxxxxxxxxxxxxxxxxx", 0x0);
    if (addHero > 0x10000) {
        s_addHeroFn = reinterpret_cast<AddHeroFn>(addHero);
    }

    s_initialized = true;
    Log::Info("PartyMgr: Initialized (AddHero=0x%08X)",
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_addHeroFn)));
    return true;
}

void AddHero(uint32_t heroId) {
    Log::Info("PartyMgr: AddHero(%u) addHeroFn=0x%08X gt=%d",
              heroId, static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_addHeroFn)),
              GameThread::IsInitialized() ? 1 : 0);
    if (s_addHeroFn && GameThread::IsInitialized()) {
        auto fn = s_addHeroFn;
        GameThread::Enqueue([fn, heroId]() {
            fn(heroId);
        });
        return;
    }
    CtoS::HeroAdd(heroId);
}
void KickHero(uint32_t heroId)   { CtoS::HeroKick(heroId); }
void KickAllHeroes() {
    // Confirmed on the current client/test environment: HERO_KICK with the
    // legacy 0x26 "kick all" sentinel does not reliably remove party heroes,
    // even when forced through increasingly faithful AutoIt-style transport
    // experiments. Individual HERO_KICK(hero_id) packets are the confirmed
    // working path, so we enumerate current heroes and kick them one by one.
    uint32_t heroIds[16] = {};
    const size_t heroCount = GetPartyHeroIds(heroIds, _countof(heroIds));
    if (heroCount) {
        for (size_t i = 0; i < heroCount; ++i) {
            if (heroIds[i] != 0) {
                CtoS::HeroKick(heroIds[i]);
            }
        }
        return;
    }
    // Fallback only when party state is unavailable.
    CtoS::SendPacket(2, Packets::HERO_KICK, 0x26u);
}

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
    const uintptr_t party = ResolvePartyContext();
    if (party <= 0x10000) return 0;

    __try {
        return *reinterpret_cast<uint32_t*>(party + 0x14);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool GetIsPartyDefeated() {
    return (ReadPartyFlags() & 0x20) != 0;
}

uint32_t CountVisibleHeroes() {
    if (Offsets::AgentBase <= 0x10000) return 0;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        if (agentArr <= 0x10000) return 0;

        const uint32_t myId = AgentMgr::GetMyId();
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        uint32_t count = 0;

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;

            auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
            if (living->agent_id == myId) continue;
            if (living->type != 0xDB) continue;
            if (living->allegiance != 1) continue;
            if (living->hp <= 0.0f) continue;

            if (SkillMgr::GetSkillbarByAgentId(living->agent_id) != nullptr) {
                count++;
            }
        }
        return count;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t CountPartyHeroes() {
    PartyInfo* playerParty = ResolvePlayerParty();
    if (!playerParty) return 0;

    __try {
        auto* heroes = &playerParty->heroes;
        if (!heroes || heroes->size > 16) return 0;
        return heroes->size;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t GetCalledTargetId() {
    PartyInfo* playerParty = ResolvePlayerParty();
    if (!playerParty || !playerParty->players.buffer) return 0;

    const uint32_t playerNumber = PlayerMgr::GetPlayerNumber();
    if (playerNumber == 0) return 0;

    __try {
        auto* players = &playerParty->players;
        if (!players || players->size > 16) return 0;
        for (uint32_t i = 0; i < players->size; ++i) {
            if (players->buffer[i].login_number == playerNumber) {
                return players->buffer[i].called_target_id;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    return 0;
}

size_t GetPartyHeroIds(uint32_t* out, size_t maxCount) {
    if (!out || maxCount == 0) return 0;

    PartyInfo* playerParty = ResolvePlayerParty();
    if (!playerParty) return 0;

    __try {
        auto* heroes = &playerParty->heroes;
        if (!heroes || !heroes->buffer || heroes->size > 16) return 0;

        const size_t count = heroes->size < maxCount ? heroes->size : maxCount;
        for (size_t i = 0; i < count; ++i) {
            out[i] = heroes->buffer[i].hero_id;
        }
        return count;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

void DebugDumpPartyState(const char* label) {
    PartyInfo* playerParty = ResolvePlayerParty();
    const uintptr_t partyContext = ResolvePartyContext();
    if (!playerParty) {
        Log::Info("PartyMgr: %s partyContext=0x%08X playerParty=NULL",
                  label ? label : "(null)",
                  static_cast<unsigned>(partyContext));
        return;
    }

    __try {
        Log::Info("PartyMgr: %s partyContext=0x%08X playerParty=0x%08X party_id=%u players=%u heroes=%u hench=%u others=%u",
                  label ? label : "(null)",
                  static_cast<unsigned>(partyContext),
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(playerParty)),
                  playerParty->party_id,
                  playerParty->players.size,
                  playerParty->heroes.size,
                  playerParty->henchmen.size,
                  playerParty->others.size);

        if (playerParty->players.buffer && playerParty->players.size <= 16) {
            for (uint32_t i = 0; i < playerParty->players.size; ++i) {
                const auto& p = playerParty->players.buffer[i];
                Log::Info("PartyMgr:   player[%u] login=%u called=%u state=0x%X",
                          i, p.login_number, p.called_target_id, p.state);
            }
        }

        if (playerParty->heroes.buffer && playerParty->heroes.size <= 16) {
            for (uint32_t i = 0; i < playerParty->heroes.size; ++i) {
                const auto& h = playerParty->heroes.buffer[i];
                Log::Info("PartyMgr:   hero[%u] hero_id=%u agent=%u owner=%u level=%u",
                          i, h.hero_id, h.agent_id, h.owner_player_id, h.level);
            }
        }

        if (playerParty->henchmen.buffer && playerParty->henchmen.size <= 16) {
            for (uint32_t i = 0; i < playerParty->henchmen.size; ++i) {
                const auto& h = playerParty->henchmen.buffer[i];
                Log::Info("PartyMgr:   hench[%u] agent=%u prof=%u level=%u",
                          i, h.agent_id, h.profession, h.level);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("PartyMgr: %s debug dump faulted", label ? label : "(null)");
    }
}

} // namespace GWA3::PartyMgr

