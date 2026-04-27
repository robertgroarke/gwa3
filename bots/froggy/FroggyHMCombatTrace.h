// Froggy combat trace and diagnostics helpers. Included by FroggyHM.cpp.

static void SetLastCombatStepDescription(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(s_combatSession.last_step, sizeof(s_combatSession.last_step), _TRUNCATE, fmt, args);
    va_end(args);
}

static void ResetLastCombatStepInfo() {
    DungeonCombatRoutine::ResetLastAction(s_combatSession);
}

static void ApplyLastCombatStepInfo(const DungeonCombatRoutine::SkillActionResult& action) {
    DungeonCombatRoutine::ApplyLastAction(s_combatSession, action);
}

static void ResetSparkflyTraversalCombatStatsState() {
    s_sparkflyTraversalCombatStats = {};
}

static void ResetCombatDebugTrace() {
    DungeonCombatRoutine::ResetDebugTrace(s_combatSession);
}

static void AddCombatDebugTraceLine(const char* fmt, ...) {
    if (s_combatSession.debug_trace_count >= static_cast<int>(std::size(s_combatSession.debug_trace))) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(s_combatSession.debug_trace[s_combatSession.debug_trace_count],
                sizeof(s_combatSession.debug_trace[s_combatSession.debug_trace_count]),
                _TRUNCATE, fmt, args);
    va_end(args);
    s_combatSession.debug_trace_count++;
}

static void CombatDebugLog(const char* fmt, ...) {
    if (!s_combatSession.debug_logging) return;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    AddCombatDebugTraceLine("%s", buffer);
    LogBot("CombatDebug: %s", buffer);
}

static void LogFroggySkillCacheSummary(const CachedSkill cache[8]) {
    LogBot("Skillbar cached (bitmask roles): %u/%u/%u/%u/%u/%u/%u/%u",
           cache[0].skill_id, cache[1].skill_id,
           cache[2].skill_id, cache[3].skill_id,
           cache[4].skill_id, cache[5].skill_id,
           cache[6].skill_id, cache[7].skill_id);
}

static void LogFroggySkillCacheSlot(int slotIndex, const CachedSkill& skill) {
    CombatDebugLog("cache slot=%d skill=%u roles=0x%X targetType=%u energy=%u type=%u activation=%.2f recharge=%.2f",
                   slotIndex + 1, skill.skill_id, skill.roles, skill.target_type, skill.energy_cost,
                   skill.skill_type, skill.activation, skill.recharge_time);
}

static bool RefreshFroggySkillCache() {
    static constexpr DungeonSkill::SkillCacheLogCallbacks kLogCallbacks = {
        &LogFroggySkillCacheSummary,
        &LogFroggySkillCacheSlot
    };
    return DungeonCombatRoutine::RefreshSkillCache(s_combatSession, &kLogCallbacks);
}

static void ResetBuiltinCombatDump() {
    DungeonCombatRoutine::ResetDecisionDump(s_combatSession);
}

static void AddBuiltinCombatDumpLine(const char* fmt, ...) {
    if (s_combatSession.decision_dump_count < 0 ||
        s_combatSession.decision_dump_count >= static_cast<int>(std::size(s_combatSession.decision_dump))) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(s_combatSession.decision_dump[s_combatSession.decision_dump_count],
                sizeof(s_combatSession.decision_dump[s_combatSession.decision_dump_count]),
                _TRUNCATE,
                fmt,
                args);
    va_end(args);
    ++s_combatSession.decision_dump_count;
}

