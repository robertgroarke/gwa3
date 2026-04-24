#pragma once

#include <gwa3/bot/DungeonSkill.h>

#include <cstddef>
#include <cstdint>

namespace GWA3::Bot::DungeonCombatRoutine {

using WaitFn = void(*)(uint32_t ms);
using BoolFn = bool(*)();
using AutoAttackFn = void(*)(uint32_t targetId);

struct SkillExecutionContext {
    DungeonSkill::CachedSkill* skill_cache = nullptr;
    bool* skill_used_this_step = nullptr;
    std::size_t skill_count = 0u;
    WaitFn wait_ms = nullptr;
    BoolFn is_dead = nullptr;
};

struct SkillActionResult {
    bool valid = false;
    bool used_skill = false;
    bool auto_attack = false;
    int slot = 0;
    uint32_t skill_id = 0u;
    uint32_t target_id = 0u;
    uint32_t role_mask = 0u;
    uint8_t target_type = 0u;
    uint32_t started_at_ms = 0u;
    uint32_t finished_at_ms = 0u;
    uint32_t expected_aftercast_ms = 0u;
};

struct SkillCandidateInspection {
    bool available = false;
    bool role_match = false;
    bool recharge_ready = false;
    bool energy_ready = false;
    bool can_cast = false;
    bool can_use = false;
    const char* can_cast_reason = nullptr;
    uint32_t recharge = 0u;
    uint32_t resolved_target = 0u;
    uint32_t adrenaline_required = 0u;
    uint32_t adrenaline_current = 0u;
    float current_energy = 0.0f;
};

bool TryUseSkillWithRole(
    uint32_t targetId,
    uint32_t roleMask,
    SkillExecutionContext& context,
    SkillActionResult& outAction);

bool TryUseSkillIndex(
    int slotIndex,
    uint32_t targetId,
    SkillExecutionContext& context,
    SkillActionResult& outAction);

int UseAllSkillsWithRole(
    uint32_t targetId,
    uint32_t roleMask,
    int maxUses,
    SkillExecutionContext& context,
    SkillActionResult* outLastAction = nullptr);

int UseSkillsInSlotOrder(
    uint32_t targetId,
    SkillExecutionContext& context,
    SkillActionResult* outLastAction = nullptr);

bool ExecuteBuiltinPriorityStep(
    uint32_t targetId,
    SkillExecutionContext& context,
    AutoAttackFn auto_attack,
    SkillActionResult& outAction);

bool ExecuteCombatStep(
    uint32_t targetId,
    SkillExecutionContext& context,
    AutoAttackFn auto_attack,
    SkillActionResult& outAction);

SkillCandidateInspection InspectSkillCandidate(
    const DungeonSkill::CachedSkill& skill,
    int slotIndex,
    uint32_t targetId,
    uint32_t roleMask);

} // namespace GWA3::Bot::DungeonCombatRoutine
