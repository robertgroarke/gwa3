#include "IntegrationTestInternal.h"

#include <bots/froggy/FroggyHM.h>
#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <cstdint>

namespace GWA3::SmokeTest {

static uint32_t NormalizeSkillEnergyCost(uint8_t rawCost) {
    switch (rawCost) {
    case 11: return 15;
    case 12: return 25;
    default: return rawCost;
    }
}

uint32_t GetCurrentEnergyPoints() {
    auto* me = GetAgentLivingRaw(ReadMyId());
    if (!me) return 0;

    const float clampedEnergy = (me->energy < 0.0f) ? 0.0f : me->energy;
    return static_cast<uint32_t>(clampedEnergy * static_cast<float>(me->max_energy) + 0.5f);
}

bool TryChooseOffensiveSkillCandidate(uint32_t targetId, SkillTestCandidate& out) {
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar || !targetId) return false;

    const uint32_t currentEnergy = GetCurrentEnergyPoints();
    bool found = false;
    int bestObservability = -1;
    SkillTestCandidate best{};

    auto observabilityScore = [](const Skill* skill) -> int {
        if (!skill) return 0;
        int score = 0;
        if (skill->recharge > 0) score += 100;
        if (skill->activation > 0.0f) score += 20;
        if (skill->aftercast > 0.0f) score += 10;
        return score;
    };

    for (uint32_t slot = 1; slot <= 8; ++slot) {
        SkillbarSkill* sb = &bar->skills[slot - 1];
        if (!sb || sb->skill_id == 0 || sb->recharge != 0) continue;

        const Skill* skill = SkillMgr::GetSkillConstantData(sb->skill_id);
        if (!skill) continue;
        if (skill->target != 5) continue;
        if (skill->adrenaline != 0) continue;

        const uint32_t energyCost = NormalizeSkillEnergyCost(skill->energy_cost);
        if (energyCost > currentEnergy) continue;

        uint32_t resolvedSkillId = 0;
        uint32_t resolvedTargetId = 0;
        uint8_t resolvedTargetType = 0;
        if (!DungeonCombatRoutine::ResolveUsableSkillTargetForSlot(
                Bot::Froggy::g_combatSession,
                slot,
                targetId,
                resolvedSkillId,
                resolvedTargetId,
                resolvedTargetType,
                "Froggy")) {
            continue;
        }
        if (resolvedSkillId != sb->skill_id || resolvedTargetType != 5u) continue;

        const int observability = observabilityScore(skill);
        if (observability <= 0) continue;

        if (!found || observability > bestObservability) {
            best.slot = slot;
            best.skillId = sb->skill_id;
            best.targetId = resolvedTargetId;
            best.targetType = resolvedTargetType;
            best.energyCost = skill->energy_cost;
            best.type = skill->type;
            best.baseRecharge = skill->recharge;
            best.activation = skill->activation;
            bestObservability = observability;
            found = true;
        }
    }

    if (found) out = best;
    return found;
}

void DumpSkillbarForSkillTest() {
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = GetAgentLivingRaw(ReadMyId());
    if (!bar || !me) {
        IntReport("  Skill dump unavailable (skillbar=%p me=%p)", bar, me);
        return;
    }

    IntReport("  Skill dump: energy=%u target=%u active=%u",
              GetCurrentEnergyPoints(),
              AgentMgr::GetTargetId(),
              me->skill);
    for (uint32_t slot = 1; slot <= 8; ++slot) {
        const SkillbarSkill& sb = bar->skills[slot - 1];
        if (sb.skill_id == 0) continue;
        const Skill* skill = SkillMgr::GetSkillConstantData(sb.skill_id);
        IntReport("    Slot %u skill=%u targetType=%u type=%u energy=%u adrenaline=%u recharge=%u event=%u",
                  slot,
                  sb.skill_id,
                  skill ? skill->target : 0xFF,
                  skill ? skill->type : 0xFFFFFFFFu,
                  skill ? NormalizeSkillEnergyCost(skill->energy_cost) : 0,
                  skill ? skill->adrenaline : 0,
                  sb.recharge,
                  sb.event);
    }
}

bool TryChooseSkillTestCandidate(SkillTestCandidate& out) {
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = GetAgentLivingRaw(ReadMyId());
    const uint32_t myId = ReadMyId();
    if (!bar || !me || myId == 0) return false;

    const uint32_t currentTarget = AgentMgr::GetTargetId();
    const uint32_t currentEnergy = GetCurrentEnergyPoints();

    auto slotPriority = [](uint8_t targetType) -> int {
        switch (targetType) {
        case 0: return 0; // self
        case 3: return 1; // ally/self
        case 4: return 2; // other ally
        case 5: return 3; // enemy
        default: return 99;
        }
    };

    auto observabilityScore = [](const Skill* skill) -> int {
        int score = 0;
        if (!skill) return score;
        if (skill->recharge > 0) score += 100;
        if (skill->activation > 0.0f) score += 20;
        if (skill->aftercast > 0.0f) score += 10;
        return score;
    };

    bool found = false;
    int bestPriority = 100;
    int bestObservability = -1;
    SkillTestCandidate best{};

    for (uint32_t slot = 1; slot <= 8; ++slot) {
        SkillbarSkill* skillbarSkill = &bar->skills[slot - 1];
        if (!skillbarSkill || skillbarSkill->skill_id == 0) continue;
        if (skillbarSkill->recharge != 0) continue;

        const Skill* skill = SkillMgr::GetSkillConstantData(skillbarSkill->skill_id);
        if (!skill) continue;
        if (skill->adrenaline != 0) continue;
        if (skillbarSkill->skill_id == 2233u) continue;

        const uint32_t energyCost = NormalizeSkillEnergyCost(skill->energy_cost);
        if (energyCost > currentEnergy) continue;

        uint32_t resolvedSkillId = 0;
        uint32_t targetId = 0;
        uint8_t targetType = 0;
        if (!DungeonCombatRoutine::ResolveUsableSkillTargetForSlot(
                Bot::Froggy::g_combatSession,
                slot,
                currentTarget,
                resolvedSkillId,
                targetId,
                targetType,
                "Froggy")) {
            continue;
        }
        if (resolvedSkillId != skillbarSkill->skill_id) continue;

        const int priority = slotPriority(targetType);
        const int observability = observabilityScore(skill);
        if (observability <= 0) continue;

        if (!found ||
            observability > bestObservability ||
            (observability == bestObservability && priority < bestPriority)) {
            best.slot = slot;
            best.skillId = skillbarSkill->skill_id;
            best.targetId = targetId;
            best.targetType = targetType;
            best.energyCost = skill->energy_cost;
            best.type = skill->type;
            best.baseRecharge = skill->recharge;
            best.activation = skill->activation;
            bestPriority = priority;
            bestObservability = observability;
            found = true;
        }
    }

    if (found) out = best;
    return found;
}

} // namespace GWA3::SmokeTest
