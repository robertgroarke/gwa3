#include <gwa3/bot/DungeonCombatRoutine.h>

#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <Windows.h>

namespace GWA3::Bot::DungeonCombatRoutine {

namespace {

constexpr DWORD kUseSkillTimeoutMs = 6000u;

void WaitOrSleep(WaitFn waitFn, uint32_t ms) {
    if (waitFn) {
        waitFn(ms);
        return;
    }
    Sleep(ms);
}

bool IsDead(BoolFn isDeadFn) {
    return isDeadFn ? isDeadFn() : false;
}

} // namespace

bool TryUseSkillWithRole(
    uint32_t targetId,
    uint32_t roleMask,
    SkillExecutionContext& context,
    SkillActionResult& outAction) {
    outAction = {};

    if (!context.skill_cache || !context.skill_used_this_step) {
        return false;
    }
    const int skillCount = static_cast<int>(context.skill_count);
    for (int i = 0; i < skillCount; ++i) {
        auto& cached = context.skill_cache[i];
        if (cached.skill_id == 0u) continue;
        if (context.skill_used_this_step[i]) continue;
        if ((cached.roles & roleMask) == 0u) continue;
        if (TryUseSkillIndex(i, targetId, context, outAction)) {
            return true;
        }
    }

    return false;
}

bool TryUseSkillIndex(
    int slotIndex,
    uint32_t targetId,
    SkillExecutionContext& context,
    SkillActionResult& outAction) {
    outAction = {};

    auto* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = AgentMgr::GetMyAgent();
    if (!bar || !me || !context.skill_cache || !context.skill_used_this_step) {
        return false;
    }
    if (slotIndex < 0 || slotIndex >= static_cast<int>(context.skill_count)) {
        return false;
    }
    if (AgentMgr::IsCasting(me)) {
        return false;
    }

    auto& cached = context.skill_cache[slotIndex];
    if (cached.skill_id == 0u) {
        return false;
    }
    if (context.skill_used_this_step[slotIndex]) {
        return false;
    }
    if (bar->skills[slotIndex].recharge > 0u) {
        return false;
    }

    const float myEnergy = me->energy * me->max_energy;
    if (cached.energy_cost > static_cast<uint8_t>(myEnergy)) {
        return false;
    }
    if (!DungeonSkill::CanUseSkill(cached, targetId)) {
        return false;
    }

    const uint32_t resolvedTarget = DungeonSkill::ResolveSkillTarget(cached, targetId);
    if (resolvedTarget == 0u &&
        (cached.target_type == 1u || cached.target_type == 4u || cached.target_type == 5u ||
         cached.target_type == 6u || cached.target_type == 14u)) {
        return false;
    }

    outAction.valid = true;
    outAction.used_skill = true;
    outAction.auto_attack = false;
    outAction.slot = slotIndex + 1;
    outAction.skill_id = cached.skill_id;
    outAction.target_id = resolvedTarget;
    outAction.role_mask = cached.roles;
    outAction.target_type = cached.target_type;
    outAction.started_at_ms = GetTickCount();

    context.skill_used_this_step[slotIndex] = true;
    SkillMgr::UseSkill(slotIndex + 1, resolvedTarget, 0u);

    const DWORD castStart = GetTickCount();
    while ((GetTickCount() - castStart) < kUseSkillTimeoutMs) {
        if (IsDead(context.is_dead)) break;
        if (!DungeonSkill::CanCast(cached)) break;
        WaitOrSleep(context.wait_ms, 50u);
    }

    float aftercast = cached.activation > 0.0f ? cached.activation : 0.0f;
    const auto* skillData = SkillMgr::GetSkillConstantData(cached.skill_id);
    if (skillData && skillData->aftercast > 0.0f) {
        aftercast = skillData->aftercast;
    }
    outAction.expected_aftercast_ms =
        aftercast > 0.0f ? static_cast<uint32_t>(aftercast * 1000.0f) : 0u;
    if (outAction.expected_aftercast_ms > 0u) {
        WaitOrSleep(context.wait_ms, outAction.expected_aftercast_ms);
    }
    outAction.finished_at_ms = GetTickCount();
    return true;
}

int UseAllSkillsWithRole(
    uint32_t targetId,
    uint32_t roleMask,
    int maxUses,
    SkillExecutionContext& context,
    SkillActionResult* outLastAction) {
    int usedCount = 0;
    SkillActionResult action = {};
    while (usedCount < maxUses) {
        action = {};
        if (!TryUseSkillWithRole(targetId, roleMask, context, action)) {
            break;
        }
        ++usedCount;
        if (outLastAction) {
            *outLastAction = action;
        }
    }
    return usedCount;
}

int UseSkillsInSlotOrder(
    uint32_t targetId,
    SkillExecutionContext& context,
    SkillActionResult* outLastAction) {
    int usedCount = 0;
    SkillActionResult action = {};
    const int skillCount = static_cast<int>(context.skill_count);
    for (int i = 0; i < skillCount; ++i) {
        action = {};
        if (!TryUseSkillIndex(i, targetId, context, action)) {
            continue;
        }
        ++usedCount;
        if (outLastAction) {
            *outLastAction = action;
        }
    }
    return usedCount;
}

bool ExecuteBuiltinPriorityStep(
    uint32_t targetId,
    SkillExecutionContext& context,
    AutoAttackFn autoAttack,
    SkillActionResult& outAction) {
    outAction = {};

    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return false;
    }
    if (AgentMgr::IsCasting(me)) {
        outAction.valid = true;
        outAction.used_skill = false;
        outAction.auto_attack = false;
        outAction.target_id = targetId;
        outAction.started_at_ms = GetTickCount();
        outAction.finished_at_ms = outAction.started_at_ms;
        return true;
    }

    SkillActionResult lastAction = {};

    const uint32_t lowestAlly = DungeonSkill::GetLowestHealthAlly();
    if (lowestAlly != 0u) {
        auto* ally = AgentMgr::GetAgentByID(lowestAlly);
        if (ally && ally->type == 0xDBu) {
            const auto* allyLiving = static_cast<AgentLiving*>(ally);
            if (allyLiving->hp < 0.3f && allyLiving->hp > 0.0f &&
                TryUseSkillWithRole(lowestAlly, DungeonSkill::ROLE_ANY_HEAL, context, outAction)) {
                return true;
            }
        }
    }

    const uint32_t deadAlly = DungeonSkill::GetDeadAlly();
    if (deadAlly != 0u &&
        TryUseSkillWithRole(deadAlly, DungeonSkill::ROLE_RESURRECT, context, outAction)) {
        return true;
    }

    if (me->hp < 0.3f) {
        if (TryUseSkillWithRole(targetId, DungeonSkill::ROLE_SURVIVAL, context, outAction)) return true;
        if (TryUseSkillWithRole(me->agent_id, DungeonSkill::ROLE_ANY_HEAL, context, outAction)) return true;
        if (TryUseSkillWithRole(targetId, DungeonSkill::ROLE_PROT | DungeonSkill::ROLE_DEFENSIVE, context, outAction)) {
            return true;
        }
    }

    if (me->hex != 0u) {
        if (TryUseSkillWithRole(me->agent_id, DungeonSkill::ROLE_HEX_REMOVE | DungeonSkill::ROLE_COND_REMOVE,
                                context, lastAction)) {
            outAction = lastAction;
        }
    }

    const uint32_t castingFoe = DungeonSkill::GetCastingEnemy();
    if (castingFoe != 0u) {
        if (TryUseSkillWithRole(castingFoe, DungeonSkill::ROLE_INTERRUPT_HARD, context, outAction)) {
            return true;
        }
        if (TryUseSkillWithRole(castingFoe, DungeonSkill::ROLE_INTERRUPT_SOFT, context, outAction)) {
            return true;
        }
    }

    if (UseAllSkillsWithRole(targetId, DungeonSkill::ROLE_PRECAST | DungeonSkill::ROLE_SHOUT, 8, context, &lastAction) > 0) {
        outAction = lastAction;
    }
    if (UseAllSkillsWithRole(targetId, DungeonSkill::ROLE_HEX | DungeonSkill::ROLE_PRESSURE, 8, context, &lastAction) > 0) {
        outAction = lastAction;
    }

    const uint32_t enchantedFoe = DungeonSkill::GetEnchantedEnemy();
    if (enchantedFoe != 0u &&
        TryUseSkillWithRole(enchantedFoe, DungeonSkill::ROLE_ENCHANT_REMOVE, context, outAction)) {
        return true;
    }

    if (TryUseSkillWithRole(targetId, DungeonSkill::ROLE_OFFENSIVE | DungeonSkill::ROLE_ATTACK, context, outAction)) {
        return true;
    }

    if (autoAttack) {
        outAction = {};
        outAction.valid = true;
        outAction.used_skill = false;
        outAction.auto_attack = true;
        outAction.target_id = targetId;
        outAction.role_mask = DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE;
        outAction.started_at_ms = GetTickCount();
        autoAttack(targetId);
        outAction.finished_at_ms = GetTickCount();
        return true;
    }

    return outAction.valid;
}

bool ExecuteCombatStep(
    uint32_t targetId,
    SkillExecutionContext& context,
    AutoAttackFn autoAttack,
    SkillActionResult& outAction) {
    outAction = {};
    if (targetId == 0u) {
        return false;
    }

    auto* target = AgentMgr::GetAgentByID(targetId);
    if (!target || target->type != 0xDBu) {
        return false;
    }

    return ExecuteBuiltinPriorityStep(targetId, context, autoAttack, outAction) && outAction.valid;
}

SkillCandidateInspection InspectSkillCandidate(
    const DungeonSkill::CachedSkill& skill,
    int slotIndex,
    uint32_t targetId,
    uint32_t roleMask) {
    SkillCandidateInspection inspection = {};
    if (slotIndex < 0 || slotIndex >= 8) {
        return inspection;
    }

    auto* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = AgentMgr::GetMyAgent();
    if (!bar || !me || skill.skill_id == 0u) {
        return inspection;
    }

    inspection.available = true;
    inspection.role_match = (skill.roles & roleMask) != 0u;
    inspection.recharge = bar->skills[slotIndex].recharge;
    inspection.recharge_ready = inspection.recharge == 0u;
    inspection.current_energy = me->energy * me->max_energy;
    inspection.energy_ready = skill.energy_cost <= static_cast<uint8_t>(inspection.current_energy);
    inspection.can_cast = DungeonSkill::CanCast(skill);
    inspection.can_use = DungeonSkill::CanUseSkill(skill, targetId);
    inspection.can_cast_reason = DungeonSkill::ExplainCanCastFailure(skill);
    inspection.resolved_target = DungeonSkill::ResolveSkillTarget(skill, targetId);

    const auto* skillData = SkillMgr::GetSkillConstantData(skill.skill_id);
    inspection.adrenaline_required = skillData ? skillData->adrenaline : 0u;
    inspection.adrenaline_current = bar->skills[slotIndex].adrenaline_a;
    return inspection;
}

} // namespace GWA3::Bot::DungeonCombatRoutine
