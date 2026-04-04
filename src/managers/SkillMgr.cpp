#include <gwa3/managers/SkillMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::SkillMgr {

// Legacy command path pushes: myId, zeroBasedSlot, target, callTarget.
using UseSkillFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t, uint32_t);
using UseHeroSkillFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t);

static UseSkillFn s_useSkillFn = nullptr;
static UseHeroSkillFn s_useHeroSkillFn = nullptr;
static bool s_initialized = false;
static bool s_loggedSkillbarMiss = false;
// Ring buffer of shellcode slots to prevent overwrites during rapid command queuing
static constexpr int kShellcodeSlots = 16;
static constexpr int kSlotSize = 32;
static uintptr_t s_useSkillShellcodeBase = 0;
static volatile LONG s_useSkillSlotIndex = 0;

static bool EnsureUseSkillShellcode() {
    if (s_useSkillShellcodeBase) return true;

    void* mem = VirtualAlloc(nullptr, kShellcodeSlots * kSlotSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("SkillMgr: VirtualAlloc failed for UseSkill shellcode pool");
        return false;
    }

    s_useSkillShellcodeBase = reinterpret_cast<uintptr_t>(mem);
    return true;
}

static uintptr_t NextSkillShellcodeSlot() {
    LONG idx = InterlockedIncrement(&s_useSkillSlotIndex) % kShellcodeSlots;
    return s_useSkillShellcodeBase + idx * kSlotSize;
}

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

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::UseSkill) s_useSkillFn = reinterpret_cast<UseSkillFn>(Offsets::UseSkill);
    if (Offsets::UseHeroSkill) s_useHeroSkillFn = reinterpret_cast<UseHeroSkillFn>(Offsets::UseHeroSkill);

    s_initialized = true;
    Log::Info("SkillMgr: Initialized (UseSkill=0x%08X, UseHeroSkill=0x%08X)",
              Offsets::UseSkill, Offsets::UseHeroSkill);
    return true;
}

void UseSkill(uint32_t slot, uint32_t targetAgentId, uint32_t callTarget) {
    if (!s_useSkillFn) {
        CtoS::UseSkill(slot, targetAgentId, callTarget);
        return;
    }

    const uint32_t myId = AgentMgr::GetMyId();
    if (!myId || slot == 0) {
        Log::Warn("SkillMgr: UseSkill skipped (MyID=%u slot=%u)", myId, slot);
        return;
    }
    const uint32_t zeroBasedSlot = slot - 1;

    if (!RenderHook::IsInitialized() || !EnsureUseSkillShellcode()) {
        GameThread::EnqueuePost([myId, zeroBasedSlot, targetAgentId, callTarget]() {
            s_useSkillFn(myId, zeroBasedSlot, targetAgentId, callTarget);
        });
        return;
    }

    uintptr_t scSlot = NextSkillShellcodeSlot();
    auto* sc = reinterpret_cast<uint8_t*>(scSlot);
    sc[0] = 0x68; // push imm32 (callTarget)
    memcpy(sc + 1, &callTarget, sizeof(callTarget));
    sc[5] = 0x68; // push imm32 (target)
    memcpy(sc + 6, &targetAgentId, sizeof(targetAgentId));
    sc[10] = 0x68; // push imm32 (zero-based slot)
    memcpy(sc + 11, &zeroBasedSlot, sizeof(zeroBasedSlot));
    sc[15] = 0x68; // push imm32 (myId)
    memcpy(sc + 16, &myId, sizeof(myId));
    sc[20] = 0xE8; // call rel32
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(s_useSkillFn) - (scSlot + 25));
    memcpy(sc + 21, &rel, sizeof(rel));
    sc[25] = 0x83; // add esp, 0x10
    sc[26] = 0xC4;
    sc[27] = 0x10;
    sc[28] = 0xC3; // ret
    FlushInstructionCache(GetCurrentProcess(), sc, 29);

    if (!RenderHook::EnqueueCommand(scSlot)) {
        Log::Warn("SkillMgr: RenderHook queue full, falling back to GameThread");
        GameThread::EnqueuePost([myId, zeroBasedSlot, targetAgentId, callTarget]() {
            s_useSkillFn(myId, zeroBasedSlot, targetAgentId, callTarget);
        });
    }
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
    CtoS::SendPacket(10, Packets::LOAD_SKILLBAR, heroIndex,
                     skillIds[0], skillIds[1], skillIds[2], skillIds[3],
                     skillIds[4], skillIds[5], skillIds[6], skillIds[7]);
}

void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t heroIndex) {
    CtoS::SendPacket(4, Packets::SET_SKILLBAR_SKILL, slot, skillId, heroIndex);
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
            Log::Warn("SkillMgr: player skillbar not found for MyID=%u at base=0x%08X", myId, static_cast<unsigned>(base));
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
