static void RunBehavioralSkillbarTest() {
    CmdReport("--- Test 9: Skillbar ---");
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (bar) {
        bool hasAnySkill = false;
        for (int i = 0; i < 8; i++) {
            if (bar->skills[i].skill_id > 0) {
                hasAnySkill = true;
                CmdReport("  Slot %d: skill %u", i, bar->skills[i].skill_id);
            }
        }
        CmdCheck("Skillbar readable", true);
        CmdCheck("At least one skill equipped", hasAnySkill);
    } else {
        CmdReport("[SKIP] Skillbar not accessible");
    }
    CmdReport("");
}
