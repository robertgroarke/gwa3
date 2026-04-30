#include <gwa3/managers/SkillMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::SkillMgr {

// Native UseSkill expects: myId, zeroBasedSlot, target, callTarget.
using UseSkillFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t, uint32_t);
using UseHeroSkillFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t);

static UseSkillFn s_useSkillFn = nullptr;
static UseHeroSkillFn s_useHeroSkillFn = nullptr;
static uintptr_t s_myIdPtr = 0;
static bool s_initialized = false;
static bool s_loggedSkillbarMiss = false;
static bool s_loggedUseSkillRenderLane = false;
static bool s_loggedUseSkillEngineLane = false;
static bool s_loggedUseSkillRestrictedMapSuppressed = false;
static bool s_loggedUseSkillRestrictedMapOverride = false;
static bool s_loggedUseSkillSettleSkip = false;
static volatile LONG s_allowRestrictedMapPlayerUseSkill = 0;
static volatile LONG s_restrictedMapPlayerUseSkillCount = 0;
static volatile LONG s_lastUnsafePlayerUseSkillTick = 0;
static constexpr DWORD kPlayerUseSkillSettleMs = 300u;

static uintptr_t GetSkillbarArrayBase() {
    if (!Offsets::BasePointer) return 0;

    __try {
        const uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx <= 0x10000) return 0;

        const uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (p1 <= 0x10000) return 0;

        const uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
        if (p2 <= 0x10000) return 0;

        auto* arr = reinterpret_cast<GWArray<Skillbar>*>(p2 + 0x6F0);
        if (!arr || !arr->buffer || arr->size == 0 || arr->size > 32) return 0;

        return reinterpret_cast<uintptr_t>(arr->buffer);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static uint32_t ResolveSkillbarAgentId(uint32_t heroIndex) {
    if (heroIndex == 0) {
        return AgentMgr::GetMyId();
    }

    PartyInfo* playerParty = PartyMgr::ResolvePlayerParty();
    if (!playerParty || !playerParty->heroes.buffer || heroIndex > playerParty->heroes.size) {
        return 0;
    }

    return playerParty->heroes.buffer[heroIndex - 1].agent_id;
}

namespace {

struct UseSkillCommand {
    uintptr_t fn;
    uint32_t my_id;
    uint32_t zero_based_slot;
    uint32_t target_agent_id;
    uint32_t call_target;
};

__declspec(naked) void BotshubUseSkillCommandStub() {
    __asm {
        mov ecx, dword ptr [eax+16]
        push ecx
        mov ebx, dword ptr [eax+12]
        push ebx
        mov edx, dword ptr [eax+8]
        push edx
        mov ecx, dword ptr [eax+4]
        push ecx
        call dword ptr [s_useSkillFn]
        add esp, 16
        jmp GWA3BotshubCommandReturnThunk
    }
}

__declspec(naked) void RenderUseSkillCommandStub() {
    __asm {
        mov ecx, dword ptr [eax+16]
        push ecx
        mov ebx, dword ptr [eax+12]
        push ebx
        mov edx, dword ptr [eax+8]
        push edx
        mov ecx, dword ptr [eax+4]
        push ecx
        call dword ptr [s_useSkillFn]
        add esp, 16
        jmp GWA3CtoSHookCommandReturnThunk
    }
}

// The native function mutates x87 state, so preserve it around the call.
static __declspec(align(16)) uint8_t s_useSkillFpuSave[108];

void InvokeUseSkillRaw(uint32_t myId, uint32_t oneBasedSlot, uint32_t targetAgentId, uint32_t callTarget) {
    if (!s_useSkillFn) return;

    uintptr_t fn = reinterpret_cast<uintptr_t>(s_useSkillFn);
    __asm {
        fsave [s_useSkillFpuSave]

        push eax
        push ecx
        push edx
        push ebx

        mov ecx, callTarget
        push ecx
        mov ebx, targetAgentId
        push ebx
        mov edx, oneBasedSlot
        dec edx
        push edx
        mov eax, myId
        push eax
        call dword ptr [fn]
        add esp, 16

        pop ebx
        pop edx
        pop ecx
        pop eax

        frstor [s_useSkillFpuSave]
    }
}

} // namespace

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::UseSkill) s_useSkillFn = reinterpret_cast<UseSkillFn>(Offsets::UseSkill);
    if (Offsets::UseHeroSkill) s_useHeroSkillFn = reinterpret_cast<UseHeroSkillFn>(Offsets::UseHeroSkill);
    s_myIdPtr = Offsets::MyID;

    s_initialized = true;
    Log::Info("SkillMgr: Initialized (UseSkill=0x%08X, UseHeroSkill=0x%08X)",
              Offsets::UseSkill, Offsets::UseHeroSkill);
    return true;
}

void SetRestrictedMapPlayerUseSkillOverride(bool enabled) {
    InterlockedExchange(&s_allowRestrictedMapPlayerUseSkill, enabled ? 1 : 0);
}

void ResetRestrictedMapPlayerUseSkillCount() {
    InterlockedExchange(&s_restrictedMapPlayerUseSkillCount, 0);
}

static bool IsPlayerUseSkillRestrictedMap(uint32_t mapId) {
    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    return area == nullptr || area->type != 2u;
}

static bool CanInvokePlayerUseSkillNow(DWORD now) {
    const auto* me = AgentMgr::GetMyAgent();
    const bool queueIdle = CtoS::IsBotshubQueueIdle();
    const bool moving = me && (me->move_x != 0.0f || me->move_y != 0.0f);

    if (!queueIdle || moving) {
        InterlockedExchange(&s_lastUnsafePlayerUseSkillTick, static_cast<LONG>(now));
        return false;
    }

    const DWORD lastUnsafeTick = static_cast<DWORD>(
        InterlockedCompareExchange(&s_lastUnsafePlayerUseSkillTick, 0, 0));
    if (lastUnsafeTick != 0u && (now - lastUnsafeTick) < kPlayerUseSkillSettleMs) {
        return false;
    }

    return true;
}

void UseSkill(uint32_t slot, uint32_t targetAgentId, uint32_t callTarget) {
    if (!s_useSkillFn) {
        Log::Warn("SkillMgr: UseSkill skipped (no native fn) slot=%u", slot);
        return;
    }

    if (slot == 0u) {
        Log::Warn("SkillMgr: UseSkill skipped (slot=%u)", slot);
        return;
    }

    const uint32_t myId = AgentMgr::GetMyId();
    if (!myId) {
        Log::Warn("SkillMgr: UseSkill skipped (MyID=%u slot=%u)", myId, slot);
        return;
    }

    Skillbar* bar = GetPlayerSkillbar();
    if (!bar || slot > 8u) {
        Log::Warn("SkillMgr: UseSkill skipped (skillbar unavailable slot=%u)", slot);
        return;
    }

    const uint32_t skillId = bar->skills[slot - 1u].skill_id;
    if (!skillId) {
        Log::Warn("SkillMgr: UseSkill skipped (empty slot=%u)", slot);
        return;
    }

    const uint32_t mapId = MapMgr::GetMapId();
    const bool restrictedMap = IsPlayerUseSkillRestrictedMap(mapId);
    if (restrictedMap && InterlockedCompareExchange(&s_allowRestrictedMapPlayerUseSkill, 0, 0) == 0) {
        if (!s_loggedUseSkillRestrictedMapSuppressed) {
            Log::Warn("SkillMgr: Suppressing player UseSkill outside explorable map context map=%u", mapId);
            s_loggedUseSkillRestrictedMapSuppressed = true;
        }
        return;
    }
    if (restrictedMap && !s_loggedUseSkillRestrictedMapOverride) {
        Log::Warn("SkillMgr: Allowing player UseSkill outside explorable map context map=%u under explicit override", mapId);
        s_loggedUseSkillRestrictedMapOverride = true;
    }

    const DWORD now = GetTickCount();
    if (!CanInvokePlayerUseSkillNow(now)) {
        if (!s_loggedUseSkillSettleSkip) {
            Log::Warn("SkillMgr: dropping UseSkill until movement/queue settle");
            s_loggedUseSkillSettleSkip = true;
        }
        return;
    }

    if (!GameThread::IsInitialized()) {
        return;
    }

    if (!s_loggedUseSkillEngineLane) {
        Log::Info("SkillMgr: UseSkill using packet path");
        s_loggedUseSkillEngineLane = true;
    }
    if (restrictedMap) {
        InterlockedIncrement(&s_restrictedMapPlayerUseSkillCount);
    }
    Log::Info("SkillMgr: UseSkill packet slot=%u skillId=%u target=%u callTarget=%u",
              slot,
              skillId,
              targetAgentId,
              callTarget);
    CtoS::UseSkill(skillId, targetAgentId, callTarget);
}

void UseHeroSkill(uint32_t heroIndex, uint32_t slot, uint32_t targetAgentId) {
    if (s_useHeroSkillFn) {
        GameThread::Enqueue([heroIndex, slot, targetAgentId]() {
            s_useHeroSkillFn(heroIndex, slot, targetAgentId);
        });
    } else {
        CtoS::SendPacket(4, Packets::USE_HERO_SKILL, heroIndex, slot, targetAgentId);
    }
}

void LoadSkillbar(const uint32_t skillIds[8], uint32_t heroIndex) {
    const uint32_t agentId = ResolveSkillbarAgentId(heroIndex);
    if (!agentId) {
        Log::Warn("SkillMgr: LoadSkillbar skipped (heroIndex=%u agentId unresolved)", heroIndex);
        return;
    }

    CtoS::SendPacket(11, Packets::LOAD_SKILLBAR, agentId, 8u,
                     skillIds[0], skillIds[1], skillIds[2], skillIds[3],
                     skillIds[4], skillIds[5], skillIds[6], skillIds[7]);
}

void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t heroIndex) {
    const uint32_t agentId = ResolveSkillbarAgentId(heroIndex);
    if (!agentId || slot == 0) {
        Log::Warn("SkillMgr: SetSkillbarSkill skipped (heroIndex=%u slot=%u agentId=%u)",
                  heroIndex, slot, agentId);
        return;
    }

    CtoS::SendPacket(5, Packets::SET_SKILLBAR_SKILL, agentId, slot - 1, skillId, 0u);
}

void ToggleHeroSkillSlot(uint32_t heroIndex, uint32_t slot) {
    CtoS::SendPacket(3, Packets::HERO_SKILL_TOGGLE, heroIndex, slot);
}

Skillbar* GetPlayerSkillbar() {
    const uint32_t myId = AgentMgr::GetMyId();
    if (!myId) return nullptr;

    const uintptr_t base = GetSkillbarArrayBase();
    if (base <= 0x10000) return nullptr;

    __try {
        for (size_t i = 0; i < 32; ++i) {
            auto* bar = reinterpret_cast<Skillbar*>(base + i * sizeof(Skillbar));
            const uint32_t agentId = bar->agent_id;
            if (!agentId) continue;
            if (agentId == myId) return bar;
        }
        if (!s_loggedSkillbarMiss) {
            s_loggedSkillbarMiss = true;
            Log::Warn("SkillMgr: player skillbar not found for MyID=%u at base=0x%08X",
                      myId,
                      static_cast<unsigned>(base));
            for (size_t i = 0; i < 8; ++i) {
                auto* bar = reinterpret_cast<Skillbar*>(base + i * sizeof(Skillbar));
                Log::Warn("SkillMgr: skillbar[%u] ptr=0x%08X agent_id=%u disabled=0x%08X",
                          static_cast<unsigned>(i),
                          static_cast<unsigned>(reinterpret_cast<uintptr_t>(bar)),
                          bar->agent_id,
                          bar->disabled);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }

    return nullptr;
}

Skillbar* GetSkillbarByAgentId(uint32_t agentId) {
    if (!agentId) return nullptr;
    const uintptr_t base = GetSkillbarArrayBase();
    if (base <= 0x10000) return nullptr;
    __try {
        for (size_t i = 0; i < 32; ++i) {
            auto* bar = reinterpret_cast<Skillbar*>(base + i * sizeof(Skillbar));
            if (bar->agent_id == agentId) return bar;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

SkillbarSkill* GetSkillbarSkill(uint32_t slot) {
    if (slot >= 8) return nullptr;
    Skillbar* bar = GetPlayerSkillbar();
    if (!bar) return nullptr;
    return &bar->skills[slot];
}

const Skill* GetSkillConstantData(uint32_t skillId) {
    if (!Offsets::SkillBase || skillId == 0) return nullptr;
    auto* base = reinterpret_cast<Skill*>(Offsets::SkillBase);
    if (!base) return nullptr;
    return &base[skillId];
}

} // namespace GWA3::SkillMgr
