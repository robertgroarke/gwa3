#include <gwa3/managers/SkillMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

namespace GWA3::SkillMgr {

// UseSkill game function: void __cdecl UseSkill(slot, target, callTarget)
using UseSkillFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t);
using UseHeroSkillFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t);

static UseSkillFn s_useSkillFn = nullptr;
static UseHeroSkillFn s_useHeroSkillFn = nullptr;
static bool s_initialized = false;

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
    if (s_useSkillFn) {
        GameThread::Enqueue([slot, targetAgentId, callTarget]() {
            s_useSkillFn(slot, targetAgentId, callTarget);
        });
    } else {
        CtoS::UseSkill(slot, targetAgentId, callTarget);
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
    if (!Offsets::SkillTimer) return nullptr;
    // SkillTimer points to the skillbar array base
    auto** ptr = reinterpret_cast<Skillbar**>(Offsets::SkillTimer);
    return ptr ? *ptr : nullptr;
}

SkillbarSkill* GetSkillbarSkill(uint32_t slot) {
    if (slot >= 8) return nullptr;
    Skillbar* bar = GetPlayerSkillbar();
    if (!bar) return nullptr;
    return &bar->skills[slot];
}

const Skill* GetSkillConstantData(uint32_t skillId) {
    if (!Offsets::SkillBase || skillId == 0) return nullptr;
    auto* base = *reinterpret_cast<Skill**>(Offsets::SkillBase);
    if (!base) return nullptr;
    return &base[skillId];
}

} // namespace GWA3::SkillMgr
