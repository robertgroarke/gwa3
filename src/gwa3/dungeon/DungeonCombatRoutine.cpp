#include <gwa3/dungeon/DungeonCombatRoutine.h>

#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <iterator>

namespace GWA3::DungeonCombatRoutine {

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

void DefaultAutoAttack(uint32_t targetId) {
    AgentMgr::Attack(targetId);
}

AutoAttackFn ResolveAutoAttack(AutoAttackFn autoAttack) {
    return autoAttack ? autoAttack : &DefaultAutoAttack;
}

void LogSkillCacheSummary(const DungeonSkill::CachedSkill cache[8]) {
    Log::Info("DungeonCombatRoutine: Skillbar cached (bitmask roles): %u/%u/%u/%u/%u/%u/%u/%u",
              cache[0].skill_id,
              cache[1].skill_id,
              cache[2].skill_id,
              cache[3].skill_id,
              cache[4].skill_id,
              cache[5].skill_id,
              cache[6].skill_id,
              cache[7].skill_id);
}

void LogSkillCacheSlot(int slotIndex, const DungeonSkill::CachedSkill& skill) {
    Log::Info("DungeonCombatRoutine: cache slot=%d skill=%u roles=0x%X targetType=%u energy=%u type=%u activation=%.2f recharge=%.2f",
              slotIndex + 1,
              skill.skill_id,
              skill.roles,
              skill.target_type,
              skill.energy_cost,
              skill.skill_type,
              skill.activation,
              skill.recharge_time);
}

bool WaitForSkillCastLatch(int slotIndex,
                           const DungeonSkill::CachedSkill& skill,
                           const SkillCastTiming& timing,
                           uint32_t rechargeBefore,
                           uint32_t eventBefore,
                           SkillExecutionContext& context,
                           const SkillCastTimingOptions& options) {
    const DWORD castLatchStart = GetTickCount();
    DWORD castLatchTimeoutMs = static_cast<DWORD>(timing.activation * 1000.0f) + options.latch_extra_ms;
    if (castLatchTimeoutMs < options.latch_min_timeout_ms) {
        castLatchTimeoutMs = options.latch_min_timeout_ms;
    }

    while ((GetTickCount() - castLatchStart) < castLatchTimeoutMs) {
        if (IsDead(context.is_dead)) break;
        auto* meNow = AgentMgr::GetMyAgent();
        auto* barNow = SkillMgr::GetPlayerSkillbar();
        if (!meNow || !barNow) break;
        const bool castingNow = AgentMgr::IsCasting(meNow) || meNow->skill == skill.skill_id;
        const bool slotChanged =
            barNow->skills[slotIndex].recharge != rechargeBefore ||
            barNow->skills[slotIndex].event != eventBefore;
        if (castingNow || slotChanged) {
            return true;
        }
        WaitOrSleep(context.wait_ms, options.latch_poll_ms);
    }
    return false;
}

void WaitForSkillCastClear(const DungeonSkill::CachedSkill& skill,
                           bool sawCastLatch,
                           SkillExecutionContext& context,
                           const SkillCastTimingOptions& options) {
    const DWORD castWaitStart = GetTickCount();
    while ((GetTickCount() - castWaitStart) < options.completion_timeout_ms) {
        if (IsDead(context.is_dead)) break;
        auto* meNow = AgentMgr::GetMyAgent();
        if (!meNow) break;
        const bool castingNow = AgentMgr::IsCasting(meNow) || meNow->skill == skill.skill_id;
        if (!castingNow) {
            if (!sawCastLatch) {
                break;
            }
            WaitOrSleep(context.wait_ms, options.clear_confirm_ms);
            meNow = AgentMgr::GetMyAgent();
            if (!meNow || !(AgentMgr::IsCasting(meNow) || meNow->skill == skill.skill_id)) {
                break;
            }
        }
        WaitOrSleep(context.wait_ms, options.clear_poll_ms);
    }
}

void PrepareDebugCombatTarget(CombatSessionState& session, uint32_t targetId, const char* logPrefix) {
    if (!session.skills_cached) {
        RefreshSkillCacheWithDebugLog(session, logPrefix);
    }
    ResetUsedSkills(session);
    AgentMgr::ChangeTarget(targetId);
}

bool TryUseFirstFoeTargetSkill(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const DebugCombatStepOptions& options) {
    for (int i = 0; i < 8; ++i) {
        const auto& cached = session.skill_cache[i];
        if (cached.skill_id == 0u) {
            continue;
        }
        if (cached.target_type != 5u) {
            continue;
        }
        if (!DungeonSkill::CanUseSkill(cached, targetId, options.aggro_range)) {
            continue;
        }

        TrackedSkillUseOptions skillOptions;
        skillOptions.wait_for_completion = true;
        skillOptions.aggro_range = options.aggro_range;
        skillOptions.timing = MakeSkillCastTimingOptions(options.log_prefix, options.max_aftercast);
        skillOptions.log_prefix = options.log_prefix;
        if (TryUseSkillSlotTracked(session, i, targetId, waitMs, isDead, skillOptions)) {
            return true;
        }
    }
    return false;
}

void ExecuteQuickDebugCombatStep(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const DebugCombatStepOptions& options) {
    PrepareDebugCombatTarget(session, targetId, options.log_prefix);
    bool attacked = false;
    const AutoAttackFn autoAttack = ResolveAutoAttack(options.auto_attack);
    if (DungeonSkill::CanBasicAttack()) {
        autoAttack(targetId);
        attacked = true;
    }

    SlotOrderUseOptions slotOptions;
    slotOptions.wait_for_completion = false;
    slotOptions.aggro_range = options.aggro_range;
    slotOptions.skill_use.timing = MakeSkillCastTimingOptions(options.log_prefix, options.max_aftercast);
    slotOptions.skill_use.log_prefix = options.log_prefix;
    const int usedSkills = UseSkillsInSlotOrderTracked(session, targetId, waitMs, isDead, slotOptions);
    if (usedSkills <= 0 && attacked) {
        RecordAutoAttackAction(
            session,
            targetId,
            DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE);
    }
}

void ExecuteFoeTargetPreferredDebugCombatStep(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const DebugCombatStepOptions& options) {
    PrepareDebugCombatTarget(session, targetId, options.log_prefix);
    if (TryUseFirstFoeTargetSkill(session, targetId, waitMs, isDead, options)) {
        return;
    }

    bool attacked = false;
    const AutoAttackFn autoAttack = ResolveAutoAttack(options.auto_attack);
    if (DungeonSkill::CanBasicAttack()) {
        autoAttack(targetId);
        attacked = true;
    }

    SlotOrderUseOptions slotOptions;
    slotOptions.wait_for_completion = true;
    slotOptions.aggro_range = options.aggro_range;
    slotOptions.skill_use.timing = MakeSkillCastTimingOptions(options.log_prefix, options.max_aftercast);
    slotOptions.skill_use.log_prefix = options.log_prefix;
    const int usedSkills = UseSkillsInSlotOrderTracked(session, targetId, waitMs, isDead, slotOptions);
    if (usedSkills <= 0 && attacked) {
        RecordAutoAttackAction(
            session,
            targetId,
            DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE);
    }
}

void DumpUnavailableBuiltinCombatContext(
    CombatSessionState& session,
    AgentLiving* me,
    Agent* target,
    Skillbar* bar,
    const char* logPrefix) {
    AddDecisionDumpLine(
        session,
        "unavailable me=%d target=%d bar=%d",
        me ? 1 : 0, target ? 1 : 0, bar ? 1 : 0);
    Log::Info("%s: BuiltinCombatDump: unavailable me=%d target=%d bar=%d",
              logPrefix ? logPrefix : "DungeonCombatRoutine",
              me ? 1 : 0,
              target ? 1 : 0,
              bar ? 1 : 0);
}

void DumpBuiltinCombatTargetHeader(
    CombatSessionState& session,
    uint32_t targetId,
    AgentLiving* me,
    AgentLiving* target,
    const char* logPrefix) {
    const float myEnergy = me->energy * me->max_energy;
    const float distance = AgentMgr::GetDistance(me->x, me->y, target->x, target->y);
    AddDecisionDumpLine(
        session,
        "target=%u distance=%.0f hp=%.3f energy=%.1f activeSkill=%u modelState=0x%X",
        targetId,
        distance,
        target->hp,
        myEnergy,
        me->skill,
        me->model_state);
    Log::Info("%s: BuiltinCombatDump: target=%u distance=%.0f hp=%.3f energy=%.1f activeSkill=%u modelState=0x%X",
              logPrefix ? logPrefix : "DungeonCombatRoutine",
              targetId,
              distance,
              target->hp,
              myEnergy,
              me->skill,
              me->model_state);
}

void DumpBuiltinCombatSkillSlot(
    CombatSessionState& session,
    int slotIndex,
    const DungeonSkill::CachedSkill& skill,
    uint32_t targetId,
    const char* logPrefix) {
    const int displaySlot = slotIndex + 1;
    if (skill.skill_id == 0u) {
        AddDecisionDumpLine(session, "slot=%d empty", displaySlot);
        Log::Info("%s: BuiltinCombatDump: slot=%d empty",
                  logPrefix ? logPrefix : "DungeonCombatRoutine",
                  displaySlot);
        return;
    }

    const auto inspection = InspectSkillCandidate(
        skill,
        slotIndex,
        targetId,
        DungeonSkill::ROLE_OFFENSIVE | DungeonSkill::ROLE_ATTACK);
    const char* canUseReason = DungeonSkill::ExplainCanUseSkillFailure(skill, targetId);
    AddDecisionDumpLine(
        session,
        "slot=%d skill=%u roles=0x%X match=%d recharge=%u ready=%d e=%u/%0.1f adren=%u/%u canCast=%d canUse=%d castReason=%s useReason=%s target=%u tgtType=%u type=%u",
        displaySlot,
        skill.skill_id,
        skill.roles,
        inspection.role_match ? 1 : 0,
        inspection.recharge,
        inspection.recharge_ready ? 1 : 0,
        skill.energy_cost,
        inspection.current_energy,
        inspection.adrenaline_current,
        inspection.adrenaline_required,
        inspection.can_cast ? 1 : 0,
        inspection.can_use ? 1 : 0,
        inspection.can_cast_reason ? inspection.can_cast_reason : "ok",
        canUseReason ? canUseReason : "ok",
        inspection.resolved_target,
        skill.target_type,
        skill.skill_type);
    Log::Info("%s: BuiltinCombatDump: slot=%d skill=%u roles=0x%X roleMatch=%d recharge=%u ready=%d energyCost=%u energyReady=%d canUse=%d resolvedTarget=%u targetType=%u type=%u",
              logPrefix ? logPrefix : "DungeonCombatRoutine",
              displaySlot,
              skill.skill_id,
              skill.roles,
              inspection.role_match ? 1 : 0,
              inspection.recharge,
              inspection.recharge_ready ? 1 : 0,
              skill.energy_cost,
              inspection.energy_ready ? 1 : 0,
              inspection.can_use ? 1 : 0,
              inspection.resolved_target,
              skill.target_type,
              skill.skill_type);
}

} // namespace

SkillExecutionContext MakeSkillExecutionContext(
    DungeonSkill::CachedSkill* skillCache,
    bool* skillUsedThisStep,
    std::size_t skillCount,
    WaitFn waitMs,
    BoolFn isDead) {
    SkillExecutionContext context;
    context.skill_cache = skillCache;
    context.skill_used_this_step = skillUsedThisStep;
    context.skill_count = skillCount;
    context.wait_ms = waitMs;
    context.is_dead = isDead;
    return context;
}

SkillExecutionContext MakeSkillExecutionContext(
    CombatSessionState& session,
    WaitFn waitMs,
    BoolFn isDead) {
    return MakeSkillExecutionContext(
        session.skill_cache,
        session.skill_used_this_step,
        8u,
        waitMs,
        isDead);
}

void ResetUsedSkills(CombatSessionState& session) {
    std::memset(session.skill_used_this_step, 0, sizeof(session.skill_used_this_step));
}

bool RefreshSkillCache(CombatSessionState& session,
                       const DungeonSkill::SkillCacheLogCallbacks* logCallbacks) {
    return DungeonSkill::CacheSkillBar(session.skill_cache, session.skills_cached, logCallbacks);
}

bool RefreshSkillCacheWithDebugLog(CombatSessionState& session, const char*) {
    static constexpr DungeonSkill::SkillCacheLogCallbacks kLogCallbacks = {
        &LogSkillCacheSummary,
        &LogSkillCacheSlot
    };
    return RefreshSkillCache(session, &kLogCallbacks);
}

void SetLastActionDescription(CombatSessionState& session, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(session.last_step, sizeof(session.last_step), _TRUNCATE, fmt, args);
    va_end(args);
}

void ResetLastAction(CombatSessionState& session) {
    session.last_action = {};
}

void ApplyLastAction(CombatSessionState& session, const SkillActionResult& action) {
    session.last_action = action;
}

void BeginAutoAttackAction(
    CombatSessionState& session,
    const char* descriptionFormat,
    uint32_t targetId,
    uint32_t roleMask) {
    SetLastActionDescription(session, descriptionFormat, targetId);
    ResetLastAction(session);
    ApplyLastAction(session, MakeAutoAttackActionResult(targetId, roleMask));
}

void FinishLastAction(CombatSessionState& session) {
    session.last_action.finished_at_ms = GetTickCount();
}

void ResetDebugTrace(CombatSessionState& session) {
    session.debug_trace_count = 0;
    std::memset(session.debug_trace, 0, sizeof(session.debug_trace));
}

void AddDebugTraceLine(CombatSessionState& session, const char* fmt, ...) {
    if (session.debug_trace_count < 0 ||
        session.debug_trace_count >= static_cast<int>(std::size(session.debug_trace))) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(session.debug_trace[session.debug_trace_count],
                sizeof(session.debug_trace[session.debug_trace_count]),
                _TRUNCATE,
                fmt,
                args);
    va_end(args);
    ++session.debug_trace_count;
}

void DebugLog(CombatSessionState& session, const char* logPrefix, const char* fmt, ...) {
    if (!session.debug_logging) return;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    AddDebugTraceLine(session, "%s", buffer);
    Log::Info("%s: CombatDebug: %s", logPrefix ? logPrefix : "DungeonCombatRoutine", buffer);
}

int GetDebugTraceCount(const CombatSessionState& session) {
    return session.debug_trace_count;
}

const char* GetDebugTraceLine(const CombatSessionState& session, int index) {
    if (index < 0 || index >= session.debug_trace_count) {
        return "";
    }
    return session.debug_trace[index];
}

void ResetDecisionDump(CombatSessionState& session) {
    session.decision_dump_count = 0;
    std::memset(session.decision_dump, 0, sizeof(session.decision_dump));
}

void AddDecisionDumpLine(CombatSessionState& session, const char* fmt, ...) {
    if (session.decision_dump_count < 0 ||
        session.decision_dump_count >= static_cast<int>(std::size(session.decision_dump))) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(session.decision_dump[session.decision_dump_count],
                sizeof(session.decision_dump[session.decision_dump_count]),
                _TRUNCATE,
                fmt,
                args);
    va_end(args);
    ++session.decision_dump_count;
}

int GetDecisionDumpCount(const CombatSessionState& session) {
    return session.decision_dump_count;
}

const char* GetDecisionDumpLine(const CombatSessionState& session, int index) {
    if (index < 0 || index >= session.decision_dump_count) {
        return "";
    }
    return session.decision_dump[index];
}

SkillActionResult MakeSkillActionResult(
    int slotIndex,
    const DungeonSkill::CachedSkill& skill,
    uint32_t resolvedTarget,
    uint32_t startedAtMs) {
    SkillActionResult action;
    action.valid = true;
    action.used_skill = true;
    action.auto_attack = false;
    action.slot = slotIndex + 1;
    action.skill_id = skill.skill_id;
    action.target_id = resolvedTarget;
    action.role_mask = skill.roles;
    action.target_type = skill.target_type;
    action.started_at_ms = startedAtMs != 0u ? startedAtMs : GetTickCount();
    return action;
}

SkillActionResult MakeAutoAttackActionResult(
    uint32_t targetId,
    uint32_t roleMask,
    uint32_t startedAtMs) {
    SkillActionResult action;
    action.valid = true;
    action.used_skill = false;
    action.auto_attack = true;
    action.target_id = targetId;
    action.role_mask = roleMask;
    action.started_at_ms = startedAtMs != 0u ? startedAtMs : GetTickCount();
    return action;
}

void RecordAutoAttackAction(
    CombatSessionState& session,
    uint32_t targetId,
    uint32_t roleMask,
    uint32_t startedAtMs,
    const char* descriptionFormat) {
    SetLastActionDescription(
        session,
        descriptionFormat ? descriptionFormat : "auto_attack target=%u",
        targetId);
    ResetLastAction(session);
    ApplyLastAction(session, MakeAutoAttackActionResult(targetId, roleMask, startedAtMs));
    FinishLastAction(session);
}

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
        DungeonSkill::SkillTargetTypeRequiresResolvedTarget(cached.target_type)) {
        return false;
    }

    outAction = MakeSkillActionResult(slotIndex, cached, resolvedTarget);

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

SkillUseResolution ResolveSkillUseTarget(
    int slotIndex,
    uint32_t targetId,
    float aggroRange,
    SkillExecutionContext& context) {
    SkillUseResolution resolution = {};

    if (!context.skill_cache || !context.skill_used_this_step || context.skill_count == 0u) {
        resolution.status = SkillUseResolutionStatus::InvalidContext;
        return resolution;
    }
    if (slotIndex < 0 || slotIndex >= static_cast<int>(context.skill_count)) {
        resolution.status = SkillUseResolutionStatus::SlotOutOfRange;
        return resolution;
    }

    auto& cached = context.skill_cache[slotIndex];
    resolution.skill = &cached;
    if (cached.skill_id == 0u) {
        resolution.status = SkillUseResolutionStatus::EmptySlot;
        return resolution;
    }
    if (context.skill_used_this_step[slotIndex]) {
        resolution.status = SkillUseResolutionStatus::AlreadyUsedThisStep;
        return resolution;
    }

    resolution.resolved_target = DungeonSkill::ResolveSkillTarget(cached, targetId, aggroRange);
    if (!DungeonSkill::CanUseSkill(cached, targetId, aggroRange)) {
        resolution.status = SkillUseResolutionStatus::CannotUse;
        resolution.failure_reason = DungeonSkill::ExplainCanUseSkillFailure(cached, targetId, aggroRange);
        return resolution;
    }

    if (resolution.resolved_target == 0u &&
        DungeonSkill::SkillTargetTypeRequiresResolvedTarget(cached.target_type)) {
        resolution.status = SkillUseResolutionStatus::MissingResolvedTarget;
        return resolution;
    }

    resolution.status = SkillUseResolutionStatus::Ready;
    return resolution;
}

bool MarkSkillUsed(
    SkillExecutionContext& context,
    int slotIndex) {
    if (!context.skill_used_this_step ||
        slotIndex < 0 ||
        slotIndex >= static_cast<int>(context.skill_count)) {
        return false;
    }
    context.skill_used_this_step[slotIndex] = true;
    return true;
}

SkillCastTimingOptions MakeSkillCastTimingOptions(
    const char* logPrefix,
    float maxAftercast) {
    SkillCastTimingOptions options;
    options.log_prefix = logPrefix;
    options.max_aftercast = maxAftercast;
    return options;
}

bool TryUseSkillSlot(
    int slotIndex,
    uint32_t targetId,
    SkillExecutionContext& context,
    const SkillSlotUseOptions& options,
    SkillSlotUseResult& outResult) {
    outResult = {};

    auto* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = AgentMgr::GetMyAgent();
    if (!bar || !me) {
        outResult.resolution.status = SkillUseResolutionStatus::InvalidContext;
        return false;
    }

    outResult.resolution = ResolveSkillUseTarget(slotIndex, targetId, options.aggro_range, context);
    if (outResult.resolution.status != SkillUseResolutionStatus::Ready ||
        outResult.resolution.skill == nullptr) {
        return false;
    }

    const auto& skill = *outResult.resolution.skill;
    const uint32_t skillTarget = outResult.resolution.resolved_target;
    outResult.action = MakeSkillActionResult(slotIndex, skill, skillTarget);
    MarkSkillUsed(context, slotIndex);

    if (options.change_target && skillTarget != 0u && skillTarget != me->agent_id) {
        AgentMgr::ChangeTarget(skillTarget);
    }

    outResult.recharge_before = bar->skills[slotIndex].recharge;
    outResult.event_before = bar->skills[slotIndex].event;
    SkillMgr::UseSkill(slotIndex + 1, skillTarget, 0u);

    if (!options.wait_for_completion) {
        outResult.action.finished_at_ms = GetTickCount();
        outResult.used = true;
        return true;
    }

    const auto timing = BuildSkillCastTiming(slotIndex, skill, options.timing);
    SkillActionResult waitAction = {};
    WaitForSkillCastCompletion(
        slotIndex,
        skill,
        timing,
        outResult.recharge_before,
        outResult.event_before,
        context,
        waitAction,
        options.timing);
    outResult.action.expected_aftercast_ms = waitAction.expected_aftercast_ms;
    outResult.action.finished_at_ms = waitAction.finished_at_ms;
    outResult.used = true;
    return true;
}

static void LogSkillUseResolutionFailure(
    CombatSessionState& session,
    const char* logPrefix,
    int slotIndex,
    uint32_t targetId,
    const SkillUseResolution& resolution) {
    const auto* skill = resolution.skill;
    const uint32_t skillId = skill ? skill->skill_id : 0u;
    switch (resolution.status) {
    case SkillUseResolutionStatus::EmptySlot:
        DebugLog(session, logPrefix, "slot=%d skip empty", slotIndex + 1);
        break;
    case SkillUseResolutionStatus::AlreadyUsedThisStep:
        DebugLog(session, logPrefix, "slot=%d skill=%u skip already_used_this_step", slotIndex + 1, skillId);
        break;
    case SkillUseResolutionStatus::CannotUse:
        DebugLog(session, logPrefix, "slot=%d skill=%u skip CanUseSkill=false reason=%s target=%u",
                 slotIndex + 1,
                 skillId,
                 resolution.failure_reason ? resolution.failure_reason : "unknown",
                 targetId);
        break;
    case SkillUseResolutionStatus::MissingResolvedTarget:
        DebugLog(session, logPrefix, "slot=%d skill=%u skip target resolution=0 targetType=%u",
                 slotIndex + 1,
                 skillId,
                 skill ? skill->target_type : 0u);
        break;
    default:
        DebugLog(session, logPrefix, "slot=%d skill=%u skip resolution_status=%u",
                 slotIndex + 1,
                 skillId,
                 static_cast<unsigned>(resolution.status));
        break;
    }
}

bool TryUseSkillSlotTracked(
    CombatSessionState& session,
    int slotIndex,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const TrackedSkillUseOptions& options) {
    SkillSlotUseOptions slotOptions;
    slotOptions.wait_for_completion = options.wait_for_completion;
    slotOptions.aggro_range = options.aggro_range;
    slotOptions.change_target = options.change_target;
    slotOptions.timing = options.timing;

    auto context = MakeSkillExecutionContext(session, waitMs, isDead);
    SkillSlotUseResult result = {};
    if (!TryUseSkillSlot(slotIndex, targetId, context, slotOptions, result) ||
        result.resolution.skill == nullptr) {
        LogSkillUseResolutionFailure(
            session,
            options.log_prefix,
            slotIndex,
            targetId,
            result.resolution);
        return false;
    }

    const auto* skill = result.resolution.skill;
    const uint32_t skillTarget = result.resolution.resolved_target;
    ApplyLastAction(session, result.action);
    SetLastActionDescription(
        session,
        "skill slot=%d skill=%u target=%u",
        slotIndex + 1,
        skill->skill_id,
        skillTarget);
    DebugLog(session, options.log_prefix, "slot=%d skill=%u USE target=%u",
             slotIndex + 1,
             skill->skill_id,
             skillTarget);
    return true;
}

SkillCastTiming BuildSkillCastTiming(
    int slotIndex,
    const DungeonSkill::CachedSkill& skill,
    const SkillCastTimingOptions& options) {
    const auto* skillData = SkillMgr::GetSkillConstantData(skill.skill_id);
    SkillCastTiming timing = {};
    timing.activation = (skillData && skillData->activation > 0.0f) ? skillData->activation : 0.0f;
    timing.aftercast = (skillData && skillData->aftercast > 0.0f) ? skillData->aftercast : 0.0f;

    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonCombatRoutine";
    if (!std::isfinite(timing.aftercast) || timing.aftercast < 0.0f) {
        Log::Warn("%s: clamping invalid aftercast slot=%d skill=%u raw=%.3f",
                  prefix,
                  slotIndex + 1,
                  skill.skill_id,
                  timing.aftercast);
        timing.aftercast = 0.0f;
    } else if (timing.aftercast > options.max_aftercast) {
        Log::Info("%s: clamping long aftercast slot=%d skill=%u raw=%.3f cap=%.3f",
                  prefix,
                  slotIndex + 1,
                  skill.skill_id,
                  timing.aftercast,
                  options.max_aftercast);
        timing.aftercast = options.max_aftercast;
    }
    return timing;
}

void WaitForSkillCastCompletion(
    int slotIndex,
    const DungeonSkill::CachedSkill& skill,
    const SkillCastTiming& timing,
    uint32_t rechargeBefore,
    uint32_t eventBefore,
    SkillExecutionContext& context,
    SkillActionResult& outAction,
    const SkillCastTimingOptions& options) {
    const bool sawCastLatch =
        WaitForSkillCastLatch(slotIndex, skill, timing, rechargeBefore, eventBefore, context, options);
    WaitForSkillCastClear(skill, sawCastLatch, context, options);

    outAction.expected_aftercast_ms =
        timing.aftercast > 0.0f ? static_cast<uint32_t>(timing.aftercast * 1000.0f) : 0u;
    if (outAction.expected_aftercast_ms > 0u) {
        WaitOrSleep(context.wait_ms, outAction.expected_aftercast_ms);
    }
    outAction.finished_at_ms = GetTickCount();
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

int UseSkillsInSlotOrderTracked(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const SlotOrderUseOptions& options) {
    if (!session.skills_cached) {
        RefreshSkillCacheWithDebugLog(session, options.skill_use.log_prefix);
    }

    TrackedSkillUseOptions skillOptions = options.skill_use;
    skillOptions.wait_for_completion = options.wait_for_completion;
    skillOptions.aggro_range = options.aggro_range;
    if (skillOptions.log_prefix == nullptr) {
        skillOptions.log_prefix = "DungeonCombatRoutine";
    }

    int usedCount = 0;
    for (int i = 0; i < static_cast<int>(std::size(session.skill_cache)); ++i) {
        if (TryUseSkillSlotTracked(session, i, targetId, waitMs, isDead, skillOptions)) {
            ++usedCount;
        }
        if (options.stop_when_enemy_out_of_range &&
            DungeonCombat::GetNearestLivingEnemyDistance() > options.aggro_range) {
            break;
        }
    }
    if (usedCount > 0) {
        DebugLog(session, skillOptions.log_prefix, "slot_order_use count=%d", usedCount);
    }
    return usedCount;
}

int UseSkillsInAggroTracked(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const AggroSkillUseOptions& options) {
    SlotOrderUseOptions slotOptions;
    slotOptions.wait_for_completion = options.wait_for_completion;
    slotOptions.aggro_range = options.aggro_range;
    slotOptions.skill_use.timing = MakeSkillCastTimingOptions(
        options.log_prefix,
        options.max_aftercast);
    slotOptions.skill_use.log_prefix = options.log_prefix;
    return UseSkillsInSlotOrderTracked(session, targetId, waitMs, isDead, slotOptions);
}

void FightTarget(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const TargetFightOptions& options) {
    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonCombatRoutine";
    AutoAttackFn autoAttack = options.auto_attack ? options.auto_attack : &DefaultAutoAttack;

    if (options.llm_combat) {
        BeginAutoAttackAction(session, "llm_auto_attack target=%u", targetId);
        DebugLog(session, prefix, "LLM combat mode issuing auto-attack target=%u", targetId);
        autoAttack(targetId);
        FinishLastAction(session);
        return;
    }

    if (!session.skills_cached) {
        RefreshSkillCacheWithDebugLog(session, prefix);
    }
    SetLastActionDescription(session, "no_action target=%u", targetId);
    ResetLastAction(session);
    ResetUsedSkills(session);
    DebugLog(session, prefix, "FightTarget start target=%u", targetId);

    auto* me = AgentMgr::GetMyAgent();
    if (!me) return;
    if (AgentMgr::IsCasting(me)) {
        DebugLog(session,
                 prefix,
                 "FightTarget skip while casting target=%u skill=%u model=0x%X",
                 targetId,
                 me->skill,
                 me->model_state);
        return;
    }

    bool attacked = false;
    if (targetId != 0 && DungeonSkill::CanBasicAttack()) {
        DebugLog(session, prefix, "FightTarget opening auto-attack target=%u before slot sweep", targetId);
        autoAttack(targetId);
        attacked = true;
    }

    SlotOrderUseOptions slotOptions;
    slotOptions.wait_for_completion = true;
    slotOptions.aggro_range = options.aggro_range;
    slotOptions.skill_use.timing = options.timing;
    slotOptions.skill_use.log_prefix = prefix;
    const int uses = UseSkillsInSlotOrderTracked(session, targetId, waitMs, isDead, slotOptions);
    if (uses <= 0 && attacked) {
        DebugLog(session, prefix, "FightTarget slot sweep found no skill; auto-attack target=%u", targetId);
        BeginAutoAttackAction(
            session,
            "auto_attack target=%u",
            targetId,
            DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE);
        FinishLastAction(session);
    }
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
        outAction = MakeAutoAttackActionResult(
            targetId,
            DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE);
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

bool ExecuteBuiltinDebugCombatStep(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs,
    BoolFn isDead,
    const DebugCombatStepOptions& options) {
    if (targetId == 0u) {
        return false;
    }

    auto* target = AgentMgr::GetAgentByID(targetId);
    if (!target || target->type != 0xDBu) {
        return false;
    }

    SetLastActionDescription(session, "uninitialized");
    ResetLastAction(session);
    ResetDebugTrace(session);
    const bool originalDebugLogging = session.debug_logging;
    session.debug_logging = true;

    const bool enableSkillOverride =
        options.restricted_skill_override_map_id != 0u &&
        MapMgr::GetMapId() == options.restricted_skill_override_map_id;
    if (enableSkillOverride) {
        SkillMgr::SetRestrictedMapPlayerUseSkillOverride(true);
    }

    if (options.quick_step) {
        ExecuteQuickDebugCombatStep(session, targetId, waitMs, isDead, options);
    } else if (options.prefer_first_foe_target_skill) {
        ExecuteFoeTargetPreferredDebugCombatStep(session, targetId, waitMs, isDead, options);
    } else {
        TargetFightOptions fightOptions;
        fightOptions.llm_combat = false;
        fightOptions.aggro_range = options.aggro_range;
        fightOptions.auto_attack = ResolveAutoAttack(options.auto_attack);
        fightOptions.log_prefix = options.log_prefix;
        fightOptions.timing = MakeSkillCastTimingOptions(options.log_prefix, options.max_aftercast);
        FightTarget(session, targetId, waitMs, isDead, fightOptions);
    }

    if (enableSkillOverride) {
        SkillMgr::SetRestrictedMapPlayerUseSkillOverride(false);
    }
    session.debug_logging = originalDebugLogging;
    return true;
}

bool RefreshCombatSkillbarForDebug(
    CombatSessionState& session,
    const char* logPrefix) {
    session.skills_cached = false;
    RefreshSkillCacheWithDebugLog(session, logPrefix);
    if (!session.skills_cached) {
        return false;
    }

    bool hasNonZero = false;
    bool hasClassifiedRole = false;
    for (int i = 0; i < 8; ++i) {
        if (session.skill_cache[i].skill_id != 0u) {
            hasNonZero = true;
        }
        if (session.skill_cache[i].roles != DungeonSkill::ROLE_NONE) {
            hasClassifiedRole = true;
        }
    }

    Log::Info("%s: combat skillbar refresh: cached=%d hasNonZero=%d hasRole=%d",
              logPrefix ? logPrefix : "DungeonCombatRoutine",
              session.skills_cached ? 1 : 0,
              hasNonZero ? 1 : 0,
              hasClassifiedRole ? 1 : 0);
    return hasNonZero;
}

bool ResolveSyntheticSkillTarget(
    uint32_t roleMask,
    uint8_t targetType,
    uint32_t defaultFoeId,
    uint32_t& outTargetId) {
    DungeonSkill::CachedSkill synthetic = {};
    synthetic.roles = roleMask;
    synthetic.target_type = targetType;
    outTargetId = DungeonSkill::ResolveSkillTarget(synthetic, defaultFoeId);
    return AgentMgr::GetMyAgent() != nullptr;
}

bool ResolveUsableSkillTargetForSlot(
    CombatSessionState& session,
    uint32_t slot,
    uint32_t defaultFoeId,
    uint32_t& outSkillId,
    uint32_t& outTargetId,
    uint8_t& outTargetType,
    const char* logPrefix) {
    if (!session.skills_cached) {
        RefreshSkillCacheWithDebugLog(session, logPrefix);
    }
    outSkillId = 0u;
    outTargetId = 0u;
    outTargetType = 0u;
    if (slot == 0u || slot > 8u) {
        return false;
    }

    const auto& cached = session.skill_cache[slot - 1u];
    if (cached.skill_id == 0u) {
        return false;
    }
    if (!DungeonSkill::CanUseSkill(cached, defaultFoeId)) {
        return false;
    }

    const uint32_t resolvedTarget = DungeonSkill::ResolveSkillTarget(cached, defaultFoeId);
    if (resolvedTarget == 0u && DungeonSkill::SkillTargetTypeRequiresResolvedTarget(cached.target_type)) {
        return false;
    }

    outSkillId = cached.skill_id;
    outTargetId = resolvedTarget;
    outTargetType = cached.target_type;
    return true;
}

void DumpBuiltinCombatDecision(
    CombatSessionState& session,
    uint32_t targetId,
    const char* logPrefix) {
    ResetDecisionDump(session);
    auto* me = AgentMgr::GetMyAgent();
    auto* target = AgentMgr::GetAgentByID(targetId);
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (!me || !target || target->type != 0xDBu || !bar) {
        DumpUnavailableBuiltinCombatContext(session, me, target, bar, logPrefix);
        return;
    }

    if (!session.skills_cached) {
        RefreshSkillCacheWithDebugLog(session, logPrefix);
    }

    DumpBuiltinCombatTargetHeader(session, targetId, me, static_cast<AgentLiving*>(target), logPrefix);
    for (int i = 0; i < 8; ++i) {
        DumpBuiltinCombatSkillSlot(session, i, session.skill_cache[i], targetId, logPrefix);
    }
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

} // namespace GWA3::DungeonCombatRoutine
