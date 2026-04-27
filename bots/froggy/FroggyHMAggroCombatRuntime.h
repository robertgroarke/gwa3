// Froggy aggro-combat loop helpers. Included by FroggyHM.cpp.

static bool CanAttackInAggro(float aggroRange, uint32_t* outTargetId = nullptr) {
    const uint32_t bestTargetId = DungeonSkill::GetBestBalledEnemy(aggroRange);
    if (outTargetId) *outTargetId = bestTargetId;
    if (bestTargetId == 0) return false;
    return DungeonSkill::CanBasicAttack();
}

static bool EnableSparkflySkillOverrideForCurrentMap() {
    const bool sparkflyMap = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    if (sparkflyMap) {
        SkillMgr::SetRestrictedMapPlayerUseSkillOverride(true);
    }
    return sparkflyMap;
}

static bool ShouldContinueFroggyAggroFight(float aggroRange, DWORD fightStart, DWORD maxFightMs) {
    return DungeonCombat::GetNearestLivingEnemyDistance() <= aggroRange &&
           !IsDead() &&
           MapMgr::GetIsMapLoaded() &&
           !PartyMgr::GetIsPartyDefeated() &&
           (GetTickCount() - fightStart) < maxFightMs;
}

static void RecordFroggyAggroTargetStats(
    SparkflyTraversalCombatStats* stats,
    uint32_t bestTarget) {
    if (!stats) {
        return;
    }
    ++stats->quick_step_attempts;
    stats->last_target_id = bestTarget;
}

static void RepositionForCarefulAggroFight(uint32_t targetId) {
    auto* target = AgentMgr::GetAgentByID(targetId);
    if (target) {
        AgentMgr::Move(target->x, target->y);
    }
    WaitMs(300);
}

static void RecordFallbackAutoAttackStep(uint32_t targetId, DWORD actionStart) {
    SetLastCombatStepDescription("auto_attack target=%u", targetId);
    ResetLastCombatStepInfo();
    ApplyLastCombatStepInfo(DungeonCombatRoutine::MakeAutoAttackActionResult(
        targetId,
        DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE,
        actionStart));
    s_combatSession.last_action.finished_at_ms = GetTickCount();
}

static void RecordFroggyAggroActionStats(SparkflyTraversalCombatStats* stats, DWORD actionStart) {
    const auto stepInfo = GetLastCombatStepInfo();
    if (!stats || !stepInfo.valid || stepInfo.started_at_ms < actionStart) {
        return;
    }
    if (stepInfo.used_skill) {
        ++stats->skill_steps;
    } else if (stepInfo.auto_attack) {
        ++stats->auto_attack_steps;
    }
}

static bool ExecuteFroggyAggroCombatPass(
    float aggroRange,
    bool careful,
    SparkflyTraversalCombatStats* stats,
    bool waitForSkillCompletion) {
    if (careful) {
        AgentMgr::CancelAction();
    }

    uint32_t bestTarget = 0;
    const bool canAttack = CanAttackInAggro(aggroRange, &bestTarget);
    if (!bestTarget) return false;

    RecordFroggyAggroTargetStats(stats, bestTarget);

    DungeonCombatRoutine::ResetUsedSkills(s_combatSession);
    bool attacked = false;
    if (canAttack) {
        AgentMgr::Attack(bestTarget);
        attacked = true;
    }
    WaitMs(100);

    if (careful) {
        RepositionForCarefulAggroFight(bestTarget);
    }

    const DWORD actionStart = GetTickCount();
    const int uses = UseSkillsInSlotOrder(bestTarget, aggroRange, waitForSkillCompletion);
    if (uses <= 0 && attacked) {
        RecordFallbackAutoAttackStep(bestTarget, actionStart);
    }
    RecordFroggyAggroActionStats(stats, actionStart);

    WaitMs(100);
    return true;
}

static void LogFroggyAggroFightBudgetIfHit(float aggroRange, DWORD fightStart, DWORD maxFightMs) {
    const DWORD elapsedFightMs = GetTickCount() - fightStart;
    if (elapsedFightMs < maxFightMs ||
        DungeonCombat::GetNearestLivingEnemyDistance() > aggroRange ||
        IsDead() ||
        !MapMgr::GetIsMapLoaded()) {
        return;
    }

    auto* me = AgentMgr::GetMyAgent();
    Log::Warn("Froggy: FightEnemiesInAggro budget hit elapsed=%lums aggroRange=%.0f player=(%.0f, %.0f) nearestEnemy=%.0f target=%u",
              static_cast<unsigned long>(elapsedFightMs),
              aggroRange,
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              DungeonCombat::GetNearestLivingEnemyDistance(aggroRange + 500.0f),
              AgentMgr::GetTargetId());
}

static void FightEnemiesInAggro(float aggroRange, bool careful = false,
                                SparkflyTraversalCombatStats* stats = nullptr,
                                bool waitForSkillCompletion = true,
                                DWORD maxFightMs = 240000u) {
    const DWORD fightStart = GetTickCount();
    const bool sparkflyMap = EnableSparkflySkillOverrideForCurrentMap();

    while (ShouldContinueFroggyAggroFight(aggroRange, fightStart, maxFightMs)) {
        if (!ExecuteFroggyAggroCombatPass(aggroRange, careful, stats, waitForSkillCompletion)) break;
    }

    LogFroggyAggroFightBudgetIfHit(aggroRange, fightStart, maxFightMs);

    if (sparkflyMap) {
        SkillMgr::SetRestrictedMapPlayerUseSkillOverride(false);
    }

    LootAfterCombatSweep(aggroRange, stats ? "combat-step-stats" : "combat-step");
}
