#pragma once

#include <gwa3/game/Skill.h>
#include <cstdint>

namespace GWA3::SkillMgr {

    bool Initialize();

    // Skill usage
    void UseSkill(uint32_t slot, uint32_t targetAgentId = 0, uint32_t callTarget = 0);
    void UseHeroSkill(uint32_t heroIndex, uint32_t slot, uint32_t targetAgentId = 0);
    void SetRestrictedMapPlayerUseSkillOverride(bool enabled);
    void ResetRestrictedMapPlayerUseSkillCount();

    // Skillbar management
    void LoadSkillbar(const uint32_t skillIds[8], uint32_t heroIndex = 0);
    void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t heroIndex = 0);
    void ToggleHeroSkillSlot(uint32_t heroIndex, uint32_t slot);

    // Skillbar data access
    Skillbar* GetPlayerSkillbar();
    Skillbar* GetSkillbarByAgentId(uint32_t agentId);
    SkillbarSkill* GetSkillbarSkill(uint32_t slot);
    const Skill* GetSkillConstantData(uint32_t skillId);

} // namespace GWA3::SkillMgr
