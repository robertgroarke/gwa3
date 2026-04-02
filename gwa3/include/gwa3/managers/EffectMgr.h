#pragma once

// GWA3-058: EffectMgr — buff/effect tracking for agents.
// Reads party effect arrays from WorldContext to check active
// buffs, enchantments, and conditions on any party member.

#include <gwa3/game/Skill.h>
#include <cstdint>

namespace GWA3::EffectMgr {

    bool Initialize();

    // Get the full party effects array (all agents with tracked effects)
    GWArray<AgentEffects>* GetPartyEffectsArray();

    // Get effects/buffs for a specific agent (null if not found)
    AgentEffects* GetAgentEffects(uint32_t agentId);

    // Shorthand: get effects/buffs for the player
    AgentEffects* GetPlayerEffects();

    // Get the effect array for an agent (null if agent not in party effects)
    GWArray<Effect>* GetAgentEffectArray(uint32_t agentId);
    GWArray<Buff>* GetAgentBuffArray(uint32_t agentId);

    // Search for a specific skill in the effect/buff list
    Effect* GetEffectBySkillId(uint32_t agentId, uint32_t skillId);
    Buff* GetBuffBySkillId(uint32_t agentId, uint32_t skillId);

    // Convenience: check if agent has a specific effect/buff active
    bool HasEffect(uint32_t agentId, uint32_t skillId);
    bool HasBuff(uint32_t agentId, uint32_t skillId);

    // Get remaining time for an effect (seconds, 0 if expired or not found)
    float GetEffectTimeRemaining(uint32_t agentId, uint32_t skillId);

    // Drop a buff by buff_id (sends packet)
    bool DropBuff(uint32_t buffId);

} // namespace GWA3::EffectMgr
