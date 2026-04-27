// Froggy combat debug entrypoints. Included by FroggyHM.cpp.

static void RecordAutoAttackCombatStep(uint32_t targetId) {
    BeginAutoAttackCombatStep("auto_attack target=%u", targetId, DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE);
    FinishAutoAttackCombatStep();
}

static void PrepareDebugCombatTarget(uint32_t targetId) {
    if (!s_combatSession.skills_cached) RefreshFroggySkillCache();
    DungeonCombatRoutine::ResetUsedSkills(s_combatSession);
    AgentMgr::ChangeTarget(targetId);
}

static bool TryUseFirstSparkflyFoeTargetSkill(uint32_t targetId) {
    for (int i = 0; i < 8; ++i) {
        const auto& c = s_combatSession.skill_cache[i];
        if (c.skill_id == 0) continue;
        if (c.target_type != 5) continue;
        if (!CanUseSkill(c, targetId, DungeonCombat::LONG_BOW_RANGE)) continue;
        if (TryUseSkillIndex(i, targetId, true, DungeonCombat::LONG_BOW_RANGE)) {
            return true;
        }
    }
    return false;
}

static void ExecuteQuickDebugCombatStep(uint32_t targetId) {
    PrepareDebugCombatTarget(targetId);
    bool attacked = false;
    if (DungeonSkill::CanBasicAttack()) {
        AgentMgr::Attack(targetId);
        attacked = true;
    }
    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(true);
    const int usedSkills = UseSkillsInSlotOrder(targetId, DungeonCombat::LONG_BOW_RANGE, false);
    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(false);
    if (usedSkills <= 0 && attacked) {
        RecordAutoAttackCombatStep(targetId);
    }
}

static void ExecuteSparkflyDebugCombatStep(uint32_t targetId) {
    PrepareDebugCombatTarget(targetId);

    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(true);
    const bool usedFoeTargetSkill = TryUseFirstSparkflyFoeTargetSkill(targetId);

    if (!usedFoeTargetSkill) {
        bool attacked = false;
        if (DungeonSkill::CanBasicAttack()) {
            AgentMgr::Attack(targetId);
            attacked = true;
        }
        const int usedSkills = UseSkillsInSlotOrder(targetId, DungeonCombat::LONG_BOW_RANGE, true);
        if (usedSkills <= 0 && attacked) {
            RecordAutoAttackCombatStep(targetId);
        }
    }
    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(false);
}

bool ExecuteBuiltinCombatStep(uint32_t targetId, bool quickStep) {
    if (!targetId) return false;

    auto* target = AgentMgr::GetAgentByID(targetId);
    if (!target || target->type != 0xDB) return false;

    auto& cfg = Bot::GetConfig();
    const CombatMode originalMode = cfg.combat_mode;
    cfg.combat_mode = CombatMode::Builtin;
    SetLastCombatStepDescription("uninitialized");
    ResetLastCombatStepInfo();
    ResetCombatDebugTrace();
    s_combatSession.debug_logging = true;
    const bool sparkflyMap = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;

    if (quickStep) {
        ExecuteQuickDebugCombatStep(targetId);
    } else if (sparkflyMap) {
        ExecuteSparkflyDebugCombatStep(targetId);
    } else {
        FightTarget(targetId);
    }

    s_combatSession.debug_logging = false;
    cfg.combat_mode = originalMode;
    return true;
}

const char* GetLastCombatStepDescription() {
    return s_combatSession.last_step;
}

LastCombatStepInfo GetLastCombatStepInfo() {
    return s_combatSession.last_action;
}

void ResetSparkflyTraversalCombatStats() {
    ResetSparkflyTraversalCombatStatsState();
}

SparkflyTraversalCombatStats GetSparkflyTraversalCombatStats() {
    return s_sparkflyTraversalCombatStats;
}

bool DebugResolveSyntheticSkillTarget(uint32_t roleMask, uint8_t targetType,
                                      uint32_t defaultFoeId, uint32_t& outTargetId) {
    CachedSkill synthetic = {};
    synthetic.roles = roleMask;
    synthetic.target_type = targetType;
    outTargetId = ResolveSkillTarget(synthetic, defaultFoeId);
    return AgentMgr::GetMyAgent() != nullptr;
}

bool DebugResolveUsableSkillTargetForSlot(uint32_t slot, uint32_t defaultFoeId,
                                          uint32_t& outSkillId, uint32_t& outTargetId, uint8_t& outTargetType) {
    if (!s_combatSession.skills_cached) {
        RefreshFroggySkillCache();
    }
    outSkillId = 0;
    outTargetId = 0;
    outTargetType = 0;
    if (slot == 0 || slot > 8) return false;

    const auto& c = s_combatSession.skill_cache[slot - 1];
    if (c.skill_id == 0) return false;
    if (!CanUseSkill(c, defaultFoeId)) return false;

    const uint32_t resolvedTarget = ResolveSkillTarget(c, defaultFoeId);
    if (resolvedTarget == 0 && DungeonSkill::SkillTargetTypeRequiresResolvedTarget(c.target_type)) {
        return false;
    }

    outSkillId = c.skill_id;
    outTargetId = resolvedTarget;
    outTargetType = c.target_type;
    return true;
}

uint32_t DebugGetCastingEnemy() {
    return DungeonSkill::GetCastingBalledEnemy();
}

uint32_t DebugGetEnchantedEnemy() {
    return DungeonSkill::GetEnchantedBalledEnemy();
}

uint32_t DebugGetMeleeRangeEnemy() {
    return DungeonSkill::GetMeleeBalledEnemy();
}

bool RefreshCombatSkillbar() {
    s_combatSession.skills_cached = false;
    RefreshFroggySkillCache();
    if (!s_combatSession.skills_cached) return false;

    bool hasNonZero = false;
    bool hasClassifiedRole = false;
    for (int i = 0; i < 8; ++i) {
        if (s_combatSession.skill_cache[i].skill_id != 0) {
            hasNonZero = true;
        }
        if (s_combatSession.skill_cache[i].roles != DungeonSkill::ROLE_NONE) {
            hasClassifiedRole = true;
        }
    }

    LogBot("Froggy combat skillbar refresh: cached=%d hasNonZero=%d hasRole=%d",
           s_combatSession.skills_cached ? 1 : 0, hasNonZero ? 1 : 0, hasClassifiedRole ? 1 : 0);
    return hasNonZero;
}

