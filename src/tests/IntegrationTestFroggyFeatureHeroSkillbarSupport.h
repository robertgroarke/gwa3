// ResolveTestPlayerParty delegated to PartyMgr::ResolvePlayerParty()
static PartyInfo* ResolveTestPlayerParty() {
    return PartyMgr::ResolvePlayerParty();
}

static uint32_t ResolveHeroAgentIdForTest(uint32_t heroIndex) {
    PartyInfo* playerParty = ResolveTestPlayerParty();
    if (!playerParty || !playerParty->heroes.buffer || heroIndex == 0 || heroIndex > playerParty->heroes.size) return 0;
    return playerParty->heroes.buffer[heroIndex - 1].agent_id;
}

static bool CopyHeroSkillbar(uint32_t heroIndex, uint32_t out[8]) {
    if (!out) return false;
    const uint32_t agentId = ResolveHeroAgentIdForTest(heroIndex);
    if (!agentId) return false;
    Skillbar* bar = SkillMgr::GetSkillbarByAgentId(agentId);
    if (!bar) return false;
    __try {
        for (int i = 0; i < 8; ++i) out[i] = bar->skills[i].skill_id;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SkillArraysEqual(const uint32_t a[8], const uint32_t b[8]) {
    for (int i = 0; i < 8; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static void ReportHeroSkillbarState(const char* label, uint32_t heroIndex, const uint32_t skills[8]) {
    FroggyFeatureReport("  %s hero %u: [%u %u %u %u %u %u %u %u]",
              label, heroIndex,
              skills[0], skills[1], skills[2], skills[3],
              skills[4], skills[5], skills[6], skills[7]);
}

static bool WaitForHeroSkillbarMatch(uint32_t heroIndex, const uint32_t expected[8], DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    uint32_t live[8] = {};
    while ((GetTickCount() - start) < timeoutMs) {
        if (CopyHeroSkillbar(heroIndex, live) && SkillArraysEqual(live, expected)) {
            return true;
        }
        Sleep(200);
    }
    return false;
}

static bool WaitForHeroSkillbarAvailable(uint32_t heroIndex, uint32_t out[8], DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (CopyHeroSkillbar(heroIndex, out)) return true;
        Sleep(200);
    }
    return false;
}
