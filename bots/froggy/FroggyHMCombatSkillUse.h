// Froggy combat skill execution helpers. Included by FroggyHM.cpp.

static DungeonCombatRoutine::SkillCastTimingOptions GetFroggySkillTimingOptions() {
    const bool bogrootMap = IsBogrootMapId(MapMgr::GetMapId());
    return DungeonCombatRoutine::MakeSkillCastTimingOptions("Froggy", bogrootMap ? 1.5f : 3.0f);
}

static DungeonCombatRoutine::SkillExecutionContext MakeFroggySkillExecutionContext() {
    return DungeonCombatRoutine::MakeSkillExecutionContext(
        s_combatSession,
        &DungeonRuntime::WaitMs,
        &IsDead);
}

static void LogFroggySkillUseResolutionFailure(
    int idx,
    uint32_t targetId,
    const DungeonCombatRoutine::SkillUseResolution& resolution) {
    const auto* skill = resolution.skill;
    const uint32_t skillId = skill ? skill->skill_id : 0u;
    switch (resolution.status) {
    case DungeonCombatRoutine::SkillUseResolutionStatus::EmptySlot:
        CombatDebugLog("slot=%d skip empty", idx + 1);
        break;
    case DungeonCombatRoutine::SkillUseResolutionStatus::AlreadyUsedThisStep:
        CombatDebugLog("slot=%d skill=%u skip already_used_this_step", idx + 1, skillId);
        break;
    case DungeonCombatRoutine::SkillUseResolutionStatus::CannotUse:
        CombatDebugLog("slot=%d skill=%u skip CanUseSkill=false reason=%s target=%u",
                       idx + 1,
                       skillId,
                       resolution.failure_reason ? resolution.failure_reason : "unknown",
                       targetId);
        break;
    case DungeonCombatRoutine::SkillUseResolutionStatus::MissingResolvedTarget:
        CombatDebugLog("slot=%d skill=%u skip target resolution=0 targetType=%u",
                       idx + 1,
                       skillId,
                       skill ? skill->target_type : 0u);
        break;
    default:
        CombatDebugLog("slot=%d skill=%u skip resolution_status=%u",
                       idx + 1,
                       skillId,
                       static_cast<unsigned>(resolution.status));
        break;
    }
}

static bool TryUseSkillIndex(
    int idx,
    uint32_t targetId,
    bool waitForCompletion = true,
    float aggroRange = DungeonCombat::LONG_BOW_RANGE) {
    DungeonCombatRoutine::SkillSlotUseOptions options;
    options.wait_for_completion = waitForCompletion;
    options.aggro_range = aggroRange;
    options.change_target = true;
    options.timing = GetFroggySkillTimingOptions();

    auto context = MakeFroggySkillExecutionContext();
    DungeonCombatRoutine::SkillSlotUseResult result = {};
    if (!DungeonCombatRoutine::TryUseSkillSlot(idx, targetId, context, options, result) ||
        result.resolution.skill == nullptr) {
        LogFroggySkillUseResolutionFailure(idx, targetId, result.resolution);
        return false;
    }
    CachedSkill* skill = result.resolution.skill;
    const uint32_t skillTarget = result.resolution.resolved_target;

    ApplyLastCombatStepInfo(result.action);
    SetLastCombatStepDescription("skill slot=%d skill=%u target=%u",
                                 idx + 1, skill->skill_id, skillTarget);
    CombatDebugLog("slot=%d skill=%u USE target=%u", idx + 1, skill->skill_id, skillTarget);
    return true;
}

static int UseSkillsInSlotOrder(
    uint32_t targetId,
    float aggroRange = DungeonCombat::LONG_BOW_RANGE,
    bool waitForCompletion = true) {
    // Ensure the player skillbar cache is populated. Without this, every
    // TryUseSkillIndex call sees an empty cached skill slot and
    // returns false, so no player skills ever fire. FightTarget caches
    // up front but the aggro path (FightEnemiesInAggro ->
    // UseSkillsInSlotOrder) did not - meaning LLM-driven aggro_move_to
    // walks would auto-attack + call targets but never cast player
    // skills, while heroes fought normally.
    if (!s_combatSession.skills_cached) RefreshFroggySkillCache();
    int usedCount = 0;
    for (int i = 0; i < 8; ++i) {
        if (TryUseSkillIndex(i, targetId, waitForCompletion, aggroRange)) {
            ++usedCount;
        }
        if (DungeonCombat::GetNearestLivingEnemyDistance() > aggroRange) {
            break;
        }
    }
    if (usedCount > 0) {
        CombatDebugLog("slot_order_use count=%d", usedCount);
    }
    return usedCount;
}

static void BeginAutoAttackCombatStep(const char* descriptionFormat, uint32_t targetId, uint32_t roleMask = 0u) {
    SetLastCombatStepDescription(descriptionFormat, targetId);
    ResetLastCombatStepInfo();
    ApplyLastCombatStepInfo(DungeonCombatRoutine::MakeAutoAttackActionResult(targetId, roleMask));
}

static void FinishAutoAttackCombatStep() {
    s_combatSession.last_action.finished_at_ms = GetTickCount();
}

// Full combat routine: use skills then fall back to auto-attack
static void FightTarget(uint32_t targetId, float aggroRange = DungeonCombat::LONG_BOW_RANGE) {
    // Combat mode toggle: if LLM mode is active, only auto-attack.
    // Gemma handles skill decisions via the bridge
    auto& cfg = Bot::GetConfig();
    if (cfg.combat_mode == CombatMode::LLM) {
        BeginAutoAttackCombatStep("llm_auto_attack target=%u", targetId);
        CombatDebugLog("LLM combat mode issuing auto-attack target=%u", targetId);
        AgentMgr::Attack(targetId);
        FinishAutoAttackCombatStep();
        return;
    }

    if (!s_combatSession.skills_cached) RefreshFroggySkillCache();
    SetLastCombatStepDescription("no_action target=%u", targetId);
    ResetLastCombatStepInfo();
    DungeonCombatRoutine::ResetUsedSkills(s_combatSession);
    CombatDebugLog("FightTarget start target=%u", targetId);

    auto* me = AgentMgr::GetMyAgent();
    if (!me) return;
    if (AgentMgr::IsCasting(me)) {
        CombatDebugLog("FightTarget skip while casting target=%u skill=%u model=0x%X",
                       targetId, me->skill, me->model_state);
        return;
    }

    bool attacked = false;
    if (targetId != 0 && DungeonSkill::CanBasicAttack()) {
        CombatDebugLog("FightTarget opening auto-attack target=%u before slot sweep", targetId);
        AgentMgr::Attack(targetId);
        attacked = true;
    }

    const int uses = UseSkillsInSlotOrder(targetId, aggroRange, true);
    if (uses <= 0 && attacked) {
        CombatDebugLog("FightTarget slot sweep found no skill; auto-attack target=%u", targetId);
        BeginAutoAttackCombatStep("auto_attack target=%u", targetId, DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE);
        FinishAutoAttackCombatStep();
    }
}
