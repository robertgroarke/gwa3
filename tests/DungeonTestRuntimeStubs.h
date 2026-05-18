namespace GWA3::TestStubs::EffectMgr {

void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration);

void Reset() {
    for (std::size_t i = 0; i < kMaxEffectSkills; ++i) {
        g_test_effect_skill_ids[i] = 0u;
    }
    g_test_effect_skill_count = 0u;
    for (std::size_t agentIndex = 0; agentIndex < kMaxTrackedEffectAgents; ++agentIndex) {
        g_test_effect_agent_ids[agentIndex] = 0u;
        g_test_effect_counts_by_agent[agentIndex] = 0u;
        g_test_agent_effects_by_agent[agentIndex] = {};
        g_test_effect_arrays_by_agent[agentIndex] = {};
        for (std::size_t effectIndex = 0; effectIndex < kMaxTrackedEffectsPerAgent; ++effectIndex) {
            g_test_effects_by_agent[agentIndex][effectIndex] = {};
        }
    }
}

void AddEffect(uint32_t skillId) {
    if (g_test_effect_skill_count >= kMaxEffectSkills) {
        return;
    }
    g_test_effect_skill_ids[g_test_effect_skill_count++] = skillId;
    AddEffectForAgent(g_test_my_agent_id, skillId, 10.0f);
}

void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration) {
    if (agentId == 0u || skillId == 0u) {
        return;
    }

    std::size_t slot = kMaxTrackedEffectAgents;
    for (std::size_t i = 0; i < kMaxTrackedEffectAgents; ++i) {
        if (g_test_effect_agent_ids[i] == agentId) {
            slot = i;
            break;
        }
        if (slot == kMaxTrackedEffectAgents && g_test_effect_agent_ids[i] == 0u) {
            slot = i;
        }
    }
    if (slot >= kMaxTrackedEffectAgents) {
        return;
    }

    if (g_test_effect_agent_ids[slot] == 0u) {
        g_test_effect_agent_ids[slot] = agentId;
    }
    if (g_test_effect_counts_by_agent[slot] >= kMaxTrackedEffectsPerAgent) {
        return;
    }

    auto& effect = g_test_effects_by_agent[slot][g_test_effect_counts_by_agent[slot]++];
    effect = {};
    effect.agent_id = agentId;
    effect.skill_id = skillId;
    effect.duration = duration;
}

} // namespace GWA3::TestStubs::EffectMgr

namespace GWA3::EffectMgr {

AgentEffects* GetAgentEffects(uint32_t agentId) {
    for (std::size_t i = 0; i < kMaxTrackedEffectAgents; ++i) {
        if (g_test_effect_agent_ids[i] != agentId) {
            continue;
        }
        g_test_effect_arrays_by_agent[i].buffer = g_test_effects_by_agent[i];
        g_test_effect_arrays_by_agent[i].capacity = static_cast<uint32_t>(kMaxTrackedEffectsPerAgent);
        g_test_effect_arrays_by_agent[i].size = static_cast<uint32_t>(g_test_effect_counts_by_agent[i]);
        g_test_effect_arrays_by_agent[i].growth = 0u;
        g_test_agent_effects_by_agent[i].agent_id = agentId;
        g_test_agent_effects_by_agent[i].buffs = {};
        g_test_agent_effects_by_agent[i].effects = g_test_effect_arrays_by_agent[i];
        return &g_test_agent_effects_by_agent[i];
    }
    return nullptr;
}

AgentEffects* GetPlayerEffects() {
    return GetAgentEffects(g_test_my_agent_id);
}

GWArray<Effect>* GetAgentEffectArray(uint32_t agentId) {
    auto* effects = GetAgentEffects(agentId);
    return effects ? &effects->effects : nullptr;
}

GWArray<Buff>* GetAgentBuffArray(uint32_t agentId) {
    auto* effects = GetAgentEffects(agentId);
    return effects ? &effects->buffs : nullptr;
}

Effect* GetEffectBySkillId(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->effects.buffer) {
        return nullptr;
    }
    for (uint32_t i = 0; i < effects->effects.size; ++i) {
        if (effects->effects.buffer[i].skill_id == skillId) {
            return &effects->effects.buffer[i];
        }
    }
    return nullptr;
}

Buff* GetBuffBySkillId(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->buffs.buffer) {
        return nullptr;
    }
    for (uint32_t i = 0; i < effects->buffs.size; ++i) {
        if (effects->buffs.buffer[i].skill_id == skillId) {
            return &effects->buffs.buffer[i];
        }
    }
    return nullptr;
}

bool HasEffect(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->effects.buffer) {
        return false;
    }
    for (uint32_t i = 0; i < effects->effects.size; ++i) {
        if (effects->effects.buffer[i].skill_id == skillId) {
            return true;
        }
    }
    return false;
}

bool DropBuff(uint32_t) {
    return false;
}

float GetEffectTimeRemaining(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->effects.buffer) {
        return 0.0f;
    }
    for (uint32_t i = 0; i < effects->effects.size; ++i) {
        if (effects->effects.buffer[i].skill_id == skillId) {
            return effects->effects.buffer[i].duration;
        }
    }
    return 0.0f;
}

} // namespace GWA3::EffectMgr

namespace GWA3::TestStubs::SkillMgr {

void Reset() {
    g_test_player_skillbar = {};
    g_test_player_skillbar.agent_id = g_test_my_agent_id;
    g_test_skill_constant_count = 0u;
    for (std::size_t i = 0; i < kMaxTestSkillConstants; ++i) {
        g_test_skill_constants[i] = {};
        g_test_skill_constant_ids[i] = 0u;
    }
    g_last_used_skill_slot = 0u;
    g_last_used_skill_target_id = 0u;
    g_last_used_skill_call_target = 0u;
}

void SetSkillData(uint32_t skillId, uint32_t type, uint8_t target, uint8_t energyCost,
                  float activation, float aftercast, uint32_t recharge, uint32_t adrenaline) {
    for (std::size_t i = 0; i < g_test_skill_constant_count; ++i) {
        if (g_test_skill_constant_ids[i] != skillId) {
            continue;
        }
        g_test_skill_constants[i].skill_id = skillId;
        g_test_skill_constants[i].type = type;
        g_test_skill_constants[i].target = target;
        g_test_skill_constants[i].energy_cost = energyCost;
        g_test_skill_constants[i].activation = activation;
        g_test_skill_constants[i].aftercast = aftercast;
        g_test_skill_constants[i].recharge = recharge;
        g_test_skill_constants[i].adrenaline = adrenaline;
        return;
    }
    if (g_test_skill_constant_count >= kMaxTestSkillConstants) {
        return;
    }
    const std::size_t index = g_test_skill_constant_count++;
    g_test_skill_constant_ids[index] = skillId;
    g_test_skill_constants[index] = {};
    g_test_skill_constants[index].skill_id = skillId;
    g_test_skill_constants[index].type = type;
    g_test_skill_constants[index].target = target;
    g_test_skill_constants[index].energy_cost = energyCost;
    g_test_skill_constants[index].activation = activation;
    g_test_skill_constants[index].aftercast = aftercast;
    g_test_skill_constants[index].recharge = recharge;
    g_test_skill_constants[index].adrenaline = adrenaline;
}

void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t recharge, uint32_t adrenaline) {
    if (slot < 1u || slot > 8u) {
        return;
    }
    auto& skill = g_test_player_skillbar.skills[slot - 1u];
    skill.skill_id = skillId;
    skill.recharge = recharge;
    skill.adrenaline_a = adrenaline;
}

uint32_t LastUsedSkillSlot() {
    return g_last_used_skill_slot;
}

uint32_t LastUsedSkillTargetId() {
    return g_last_used_skill_target_id;
}

uint32_t LastUsedSkillCallTarget() {
    return g_last_used_skill_call_target;
}

} // namespace GWA3::TestStubs::SkillMgr

namespace GWA3::SkillMgr {

void UseSkill(uint32_t slot, uint32_t targetAgentId, uint32_t callTarget) {
    g_last_used_skill_slot = slot;
    g_last_used_skill_target_id = targetAgentId;
    g_last_used_skill_call_target = callTarget;
    if (slot >= 1u && slot <= 8u) {
        g_test_player_skillbar.skills[slot - 1u].recharge = 100u;
    }
}

void UseHeroSkill(uint32_t, uint32_t, uint32_t) {
}

void LoadSkillbar(const uint32_t skillIds[8], uint32_t) {
    for (uint32_t i = 0; i < 8u; ++i) {
        g_test_player_skillbar.skills[i].skill_id = skillIds ? skillIds[i] : 0u;
        g_test_player_skillbar.skills[i].recharge = 0u;
        g_test_player_skillbar.skills[i].adrenaline_a = 0u;
        g_test_player_skillbar.skills[i].adrenaline_b = 0u;
    }
}

void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t) {
    if (slot < 1u || slot > 8u) {
        return;
    }
    g_test_player_skillbar.skills[slot - 1u].skill_id = skillId;
}

void ToggleHeroSkillSlot(uint32_t, uint32_t) {
}

Skillbar* GetPlayerSkillbar() {
    g_test_player_skillbar.agent_id = g_test_my_agent_id;
    return &g_test_player_skillbar;
}

Skillbar* GetSkillbarByAgentId(uint32_t agentId) {
    return agentId == g_test_my_agent_id ? &g_test_player_skillbar : nullptr;
}

SkillbarSkill* GetSkillbarSkill(uint32_t slot) {
    if (slot < 1u || slot > 8u) {
        return nullptr;
    }
    return &g_test_player_skillbar.skills[slot - 1u];
}

const Skill* GetSkillConstantData(uint32_t skillId) {
    for (std::size_t i = 0; i < g_test_skill_constant_count; ++i) {
        if (g_test_skill_constant_ids[i] == skillId) {
            return &g_test_skill_constants[i];
        }
    }
    return nullptr;
}

} // namespace GWA3::SkillMgr

namespace GWA3::TestStubs::PlayerMgr {

void Reset() {
    g_test_active_title_id = 0u;
}

} // namespace GWA3::TestStubs::PlayerMgr

namespace GWA3::PlayerMgr {

bool SetActiveTitle(uint32_t titleId) {
    g_test_active_title_id = titleId;
    return true;
}

uint32_t GetActiveTitleId() {
    return g_test_active_title_id;
}

} // namespace GWA3::PlayerMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset() {
    g_dialog_shutdown_count = 0u;
    g_dialog_initialize_count = 0u;
    g_test_dialog_open = false;
    g_test_dialog_sender_id = 0u;
    g_test_dialog_button_count = 0u;
    for (auto& button : g_test_dialog_buttons) {
        button = {};
    }
    g_test_auto_open_dialog_on_npc_interact = false;
    g_test_auto_dialog_button_count = 0u;
}

uint32_t ShutdownCount() {
    return g_dialog_shutdown_count;
}

uint32_t InitializeCount() {
    return g_dialog_initialize_count;
}

void SetDialogState(bool open, uint32_t senderAgentId, uint32_t buttonCount) {
    g_test_dialog_open = open;
    g_test_dialog_sender_id = senderAgentId;
    g_test_dialog_button_count = buttonCount;
}

void SetDialogButton(uint32_t index, uint32_t dialogId, uint32_t buttonIcon) {
    if (index >= static_cast<uint32_t>(sizeof(g_test_dialog_buttons) / sizeof(g_test_dialog_buttons[0]))) {
        return;
    }
    g_test_dialog_buttons[index] = {};
    g_test_dialog_buttons[index].dialog_id = dialogId;
    g_test_dialog_buttons[index].button_icon = buttonIcon;
    if (g_test_dialog_button_count <= index) {
        g_test_dialog_button_count = index + 1u;
    }
}

void SetAutoDialogOnNpcInteract(bool enabled, uint32_t buttonCount) {
    g_test_auto_open_dialog_on_npc_interact = enabled;
    g_test_auto_dialog_button_count = buttonCount;
}

} // namespace GWA3::TestStubs::DialogMgr

namespace GWA3::DialogMgr {

bool Initialize() {
    ++g_dialog_initialize_count;
    return true;
}

void Shutdown() {
    ++g_dialog_shutdown_count;
}

bool IsDialogOpen() {
    return g_test_dialog_open;
}

uint32_t GetDialogSenderAgentId() {
    return g_test_dialog_sender_id;
}

uint32_t GetButtonCount() {
    return g_test_dialog_button_count;
}

const DialogButton* GetButton(uint32_t index) {
    if (index >= g_test_dialog_button_count ||
        index >= static_cast<uint32_t>(sizeof(g_test_dialog_buttons) / sizeof(g_test_dialog_buttons[0]))) {
        return nullptr;
    }
    return &g_test_dialog_buttons[index];
}

uint32_t GetLastDialogId() {
    return g_test_last_dialog_id;
}

void ClearDialog() {
    g_test_dialog_open = false;
    g_test_dialog_sender_id = 0u;
    g_test_dialog_button_count = 0u;
}

void ResetHookState() {
}

void ResetRecentUITrace() {
}

} // namespace GWA3::DialogMgr

namespace GWA3::DialogHook {

void RecordDialogSend(uint32_t) {
}

} // namespace GWA3::DialogHook
