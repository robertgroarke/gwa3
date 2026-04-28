#pragma once

#include <gwa3/dungeon/DungeonSkill.h>

#include <cstddef>
#include <cstdint>

namespace GWA3::DungeonCombatRoutine {

using WaitFn = void(*)(uint32_t ms);
using BoolFn = bool(*)();
using AutoAttackFn = void(*)(uint32_t targetId);

inline constexpr float DEFAULT_SKILL_AGGRO_RANGE = 1320.0f;

struct SkillExecutionContext {
    DungeonSkill::CachedSkill* skill_cache = nullptr;
    bool* skill_used_this_step = nullptr;
    std::size_t skill_count = 0u;
    WaitFn wait_ms = nullptr;
    BoolFn is_dead = nullptr;
};

enum class SkillUseResolutionStatus : uint8_t {
    Ready,
    InvalidContext,
    SlotOutOfRange,
    EmptySlot,
    AlreadyUsedThisStep,
    CannotUse,
    MissingResolvedTarget,
};

struct SkillUseResolution {
    SkillUseResolutionStatus status = SkillUseResolutionStatus::InvalidContext;
    DungeonSkill::CachedSkill* skill = nullptr;
    uint32_t resolved_target = 0u;
    const char* failure_reason = nullptr;
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

struct CombatSessionState {
    DungeonSkill::CachedSkill skill_cache[8] = {};
    bool skills_cached = false;
    bool skill_used_this_step[8] = {};
    char last_step[128] = "uninitialized";
    SkillActionResult last_action = {};
    bool debug_logging = false;
    char decision_dump[12][256] = {};
    int decision_dump_count = 0;
    char debug_trace[96][256] = {};
    int debug_trace_count = 0;
};

struct SkillCastTiming {
    float activation = 0.0f;
    float aftercast = 0.0f;
};

struct SkillCastTimingOptions {
    float max_aftercast = 3.0f;
    uint32_t latch_extra_ms = 750u;
    uint32_t latch_min_timeout_ms = 500u;
    uint32_t latch_poll_ms = 25u;
    uint32_t completion_timeout_ms = 6000u;
    uint32_t clear_confirm_ms = 75u;
    uint32_t clear_poll_ms = 25u;
    const char* log_prefix = nullptr;
};

struct SkillSlotUseOptions {
    bool wait_for_completion = true;
    float aggro_range = 1320.0f;
    bool change_target = true;
    SkillCastTimingOptions timing = {};
};

struct SkillSlotUseResult {
    bool used = false;
    SkillUseResolution resolution = {};
    SkillActionResult action = {};
    uint32_t recharge_before = 0u;
    uint32_t event_before = 0u;
};

struct TrackedSkillUseOptions {
    bool wait_for_completion = true;
    float aggro_range = DEFAULT_SKILL_AGGRO_RANGE;
    bool change_target = true;
    SkillCastTimingOptions timing = {};
    const char* log_prefix = nullptr;
};

struct SlotOrderUseOptions {
    bool wait_for_completion = true;
    float aggro_range = DEFAULT_SKILL_AGGRO_RANGE;
    bool stop_when_enemy_out_of_range = true;
    TrackedSkillUseOptions skill_use = {};
};

struct TargetFightOptions {
    bool llm_combat = false;
    float aggro_range = DEFAULT_SKILL_AGGRO_RANGE;
    AutoAttackFn auto_attack = nullptr;
    const char* log_prefix = nullptr;
    SkillCastTimingOptions timing = {};
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

SkillExecutionContext MakeSkillExecutionContext(
    DungeonSkill::CachedSkill* skillCache,
    bool* skillUsedThisStep,
    std::size_t skillCount,
    WaitFn waitMs = nullptr,
    BoolFn isDead = nullptr);

SkillExecutionContext MakeSkillExecutionContext(
    CombatSessionState& session,
    WaitFn waitMs = nullptr,
    BoolFn isDead = nullptr);

void ResetUsedSkills(CombatSessionState& session);
bool RefreshSkillCache(CombatSessionState& session,
                       const DungeonSkill::SkillCacheLogCallbacks* logCallbacks = nullptr);
bool RefreshSkillCacheWithDebugLog(CombatSessionState& session, const char* logPrefix = nullptr);
void SetLastActionDescription(CombatSessionState& session, const char* fmt, ...);
void ResetLastAction(CombatSessionState& session);
void ApplyLastAction(CombatSessionState& session, const SkillActionResult& action);
void BeginAutoAttackAction(CombatSessionState& session,
                           const char* descriptionFormat,
                           uint32_t targetId,
                           uint32_t roleMask = 0u);
void FinishLastAction(CombatSessionState& session);
void ResetDebugTrace(CombatSessionState& session);
void AddDebugTraceLine(CombatSessionState& session, const char* fmt, ...);
void DebugLog(CombatSessionState& session, const char* logPrefix, const char* fmt, ...);
int GetDebugTraceCount(const CombatSessionState& session);
const char* GetDebugTraceLine(const CombatSessionState& session, int index);
void ResetDecisionDump(CombatSessionState& session);
void AddDecisionDumpLine(CombatSessionState& session, const char* fmt, ...);
int GetDecisionDumpCount(const CombatSessionState& session);
const char* GetDecisionDumpLine(const CombatSessionState& session, int index);

SkillActionResult MakeSkillActionResult(
    int slotIndex,
    const DungeonSkill::CachedSkill& skill,
    uint32_t resolvedTarget,
    uint32_t startedAtMs = 0u);

SkillActionResult MakeAutoAttackActionResult(
    uint32_t targetId,
    uint32_t roleMask,
    uint32_t startedAtMs = 0u);

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

SkillUseResolution ResolveSkillUseTarget(
    int slotIndex,
    uint32_t targetId,
    float aggroRange,
    SkillExecutionContext& context);

bool MarkSkillUsed(
    SkillExecutionContext& context,
    int slotIndex);

SkillCastTimingOptions MakeSkillCastTimingOptions(
    const char* logPrefix,
    float maxAftercast);

bool TryUseSkillSlot(
    int slotIndex,
    uint32_t targetId,
    SkillExecutionContext& context,
    const SkillSlotUseOptions& options,
    SkillSlotUseResult& outResult);

bool TryUseSkillSlotTracked(
    CombatSessionState& session,
    int slotIndex,
    uint32_t targetId,
    WaitFn waitMs = nullptr,
    BoolFn isDead = nullptr,
    const TrackedSkillUseOptions& options = {});

SkillCastTiming BuildSkillCastTiming(
    int slotIndex,
    const DungeonSkill::CachedSkill& skill,
    const SkillCastTimingOptions& options = {});

void WaitForSkillCastCompletion(
    int slotIndex,
    const DungeonSkill::CachedSkill& skill,
    const SkillCastTiming& timing,
    uint32_t rechargeBefore,
    uint32_t eventBefore,
    SkillExecutionContext& context,
    SkillActionResult& outAction,
    const SkillCastTimingOptions& options = {});

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

int UseSkillsInSlotOrderTracked(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs = nullptr,
    BoolFn isDead = nullptr,
    const SlotOrderUseOptions& options = {});

void FightTarget(
    CombatSessionState& session,
    uint32_t targetId,
    WaitFn waitMs = nullptr,
    BoolFn isDead = nullptr,
    const TargetFightOptions& options = {});

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

} // namespace GWA3::DungeonCombatRoutine
