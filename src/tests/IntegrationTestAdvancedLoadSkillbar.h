bool TestLoadSkillbar() {
    IntReport("=== Skillbar Load ===");

    if (ReadMyId() == 0) { IntSkip("SkillbarLoad", "Not in game"); IntReport(""); return false; }

    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) { IntSkip("SkillbarLoad", "Skillbar unavailable"); IntReport(""); return false; }

    uint32_t savedSkills[8];
    for (int i = 0; i < 8; ++i) savedSkills[i] = bar->skills[i].skill_id;

    IntReport("  Current skillbar: [%u %u %u %u %u %u %u %u]",
              savedSkills[0], savedSkills[1], savedSkills[2], savedSkills[3],
              savedSkills[4], savedSkills[5], savedSkills[6], savedSkills[7]);

    const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
    if (!area || IsSkillCastMapType(area->type)) {
        IntSkip("SkillbarLoad", "Not in outpost - can only load skills in town");
        IntReport("");
        return true;
    }

    uint32_t modifiedSkills[8];
    for (int i = 0; i < 8; ++i) modifiedSkills[i] = savedSkills[i];

    int swapA = -1;
    int swapB = -1;
    for (int i = 0; i < 8; ++i) {
        if (savedSkills[i] == 0) continue;
        if (swapA == -1) {
            swapA = i;
            continue;
        }
        if (savedSkills[i] != savedSkills[swapA]) {
            swapB = i;
            break;
        }
    }
    if (swapA == -1 || swapB == -1) {
        IntSkip("SkillbarLoad", "Need two distinct non-zero skills to verify load");
        IntReport("");
        return true;
    }

    const uint32_t tmp = modifiedSkills[swapA];
    modifiedSkills[swapA] = modifiedSkills[swapB];
    modifiedSkills[swapB] = tmp;

    IntReport("  Loading modified skillbar (swap slot %d and %d)...", swapA + 1, swapB + 1);
    SkillMgr::LoadSkillbar(modifiedSkills, 0);
    Sleep(1000 + ChatMgr::GetPing());

    bar = SkillMgr::GetPlayerSkillbar();
    bool modifiedMatches = bar != nullptr;
    if (bar) {
        for (int i = 0; i < 8; ++i) {
            if (bar->skills[i].skill_id != modifiedSkills[i]) {
                modifiedMatches = false;
                break;
            }
        }
        IntReport("  After modified load: [%u %u %u %u %u %u %u %u]",
                  bar->skills[0].skill_id, bar->skills[1].skill_id, bar->skills[2].skill_id, bar->skills[3].skill_id,
                  bar->skills[4].skill_id, bar->skills[5].skill_id, bar->skills[6].skill_id, bar->skills[7].skill_id);
    }
    IntCheck("Skillbar changed after load", modifiedMatches);

    IntReport("  Restoring original skillbar...");
    SkillMgr::LoadSkillbar(savedSkills, 0);
    Sleep(1000 + ChatMgr::GetPing());

    bar = SkillMgr::GetPlayerSkillbar();
    bool restoredMatches = bar != nullptr;
    if (bar) {
        for (int i = 0; i < 8; ++i) {
            if (bar->skills[i].skill_id != savedSkills[i]) {
                restoredMatches = false;
                break;
            }
        }
        IntReport("  After restore: [%u %u %u %u %u %u %u %u]",
                  bar->skills[0].skill_id, bar->skills[1].skill_id, bar->skills[2].skill_id, bar->skills[3].skill_id,
                  bar->skills[4].skill_id, bar->skills[5].skill_id, bar->skills[6].skill_id, bar->skills[7].skill_id);
    }
    IntCheck("Skillbar restored after reload", restoredMatches);

    IntReport("");
    return true;
}

// ===== Party Management =====
