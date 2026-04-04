#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

namespace GWA3::EffectMgr {

static bool s_initialized = false;

// WorldContext resolution: *BasePointer -> +0x18 -> +0x2C = WorldContext*
// party_effects at WorldContext + 0x508 (GWArray<AgentEffects>)
static uintptr_t ResolveWorldContext() {
    if (Offsets::BasePointer <= 0x10000) return 0;

    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx <= 0x10000) return 0;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (p1 <= 0x10000) return 0;
        uintptr_t world = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
        if (world <= 0x10000) return 0;
        return world;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("EffectMgr: Initialized");
    return true;
}

GWArray<AgentEffects>* GetPartyEffectsArray() {
    uintptr_t world = ResolveWorldContext();
    if (!world) return nullptr;

    __try {
        auto* arr = reinterpret_cast<GWArray<AgentEffects>*>(world + 0x508);
        if (!arr->buffer || arr->size == 0 || arr->size > 64) return nullptr;
        return arr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

AgentEffects* GetAgentEffects(uint32_t agentId) {
    auto* arr = GetPartyEffectsArray();
    if (!arr || !arr->buffer) return nullptr;

    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            if (arr->buffer[i].agent_id == agentId) {
                return &arr->buffer[i];
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

AgentEffects* GetPlayerEffects() {
    if (Offsets::MyID <= 0x10000) return nullptr;

    __try {
        uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);
        if (myId == 0) return nullptr;
        return GetAgentEffects(myId);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

GWArray<Effect>* GetAgentEffectArray(uint32_t agentId) {
    AgentEffects* ae = GetAgentEffects(agentId);
    if (!ae) return nullptr;
    if (!ae->effects.buffer || ae->effects.size > 500) return nullptr;
    return &ae->effects;
}

GWArray<Buff>* GetAgentBuffArray(uint32_t agentId) {
    AgentEffects* ae = GetAgentEffects(agentId);
    if (!ae) return nullptr;
    if (!ae->buffs.buffer || ae->buffs.size > 500) return nullptr;
    return &ae->buffs;
}

Effect* GetEffectBySkillId(uint32_t agentId, uint32_t skillId) {
    auto* arr = GetAgentEffectArray(agentId);
    if (!arr || !arr->buffer) return nullptr;

    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            if (arr->buffer[i].skill_id == skillId) {
                return &arr->buffer[i];
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

Buff* GetBuffBySkillId(uint32_t agentId, uint32_t skillId) {
    auto* arr = GetAgentBuffArray(agentId);
    if (!arr || !arr->buffer) return nullptr;

    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            if (arr->buffer[i].skill_id == skillId) {
                return &arr->buffer[i];
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

bool HasEffect(uint32_t agentId, uint32_t skillId) {
    return GetEffectBySkillId(agentId, skillId) != nullptr;
}

bool HasBuff(uint32_t agentId, uint32_t skillId) {
    return GetBuffBySkillId(agentId, skillId) != nullptr;
}

float GetEffectTimeRemaining(uint32_t agentId, uint32_t skillId) {
    Effect* eff = GetEffectBySkillId(agentId, skillId);
    if (!eff || eff->duration <= 0.0f) return 0.0f;

    // SkillTimer gives the current game time in ms.
    // Effect timestamp is when it was applied (in same timebase).
    // Remaining = duration - (now - timestamp) / 1000.0f
    uint32_t skillTimer = MemoryMgr::GetSkillTimer();
    if (skillTimer == 0) return 0.0f;

    float elapsed = static_cast<float>(skillTimer - eff->timestamp) / 1000.0f;
    float remaining = eff->duration - elapsed;
    return remaining > 0.0f ? remaining : 0.0f;
}

// DropBuff function signature: void __cdecl DropBuff(uint32_t buffId)
typedef void(__cdecl* DropBuffFn)(uint32_t);

bool DropBuff(uint32_t buffId) {
    if (!Offsets::DropBuff || Offsets::DropBuff <= 0x10000) return false;

    auto fn = reinterpret_cast<DropBuffFn>(Offsets::DropBuff);
    __try {
        fn(buffId);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace GWA3::EffectMgr
