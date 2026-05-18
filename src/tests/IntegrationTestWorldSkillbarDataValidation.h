bool TestSkillbarDataValidation() {
    IntReport("=== Skillbar Data Validation ===");

    if (ReadMyId() == 0) {
        IntSkip("Skillbar data", "Not in game");
        IntReport("");
        return false;
    }

    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    IntReport("  Skillbar: %p", bar);
    IntCheck("Skillbar available", bar != nullptr);
    if (!bar) {
        IntReport("");
        return false;
    }

    IntReport("  Skillbar agent_id=%u disabled=%u", bar->agent_id, bar->disabled);
    IntCheck("Skillbar agent_id matches MyID", bar->agent_id == ReadMyId());

    uint32_t loadedSkills = 0;
    for (uint32_t slot = 0; slot < 8; ++slot) {
        const SkillbarSkill& sb = bar->skills[slot];
        if (sb.skill_id == 0) continue;
        loadedSkills++;

        const Skill* skill = SkillMgr::GetSkillConstantData(sb.skill_id);
        IntReport("  Slot %u: skill_id=%u recharge=%u event=%u", slot + 1, sb.skill_id, sb.recharge, sb.event);

        if (skill) {
            IntReport("    => type=%u profession=%u attribute=%u energy=%u activation=%.2f recharge=%u campaign=%u",
                      skill->type,
                      skill->profession,
                      skill->attribute,
                      skill->energy_cost,
                      skill->activation,
                      skill->recharge,
                      skill->campaign);

            char checkName[128];
            snprintf(checkName, sizeof(checkName), "Slot %u skill %u profession valid (0-10)", slot + 1, sb.skill_id);
            IntCheck(checkName, skill->profession <= 10);
            snprintf(checkName, sizeof(checkName), "Slot %u skill %u campaign valid (0-4)", slot + 1, sb.skill_id);
            IntCheck(checkName, skill->campaign <= 4);
        } else {
            char checkName[128];
            snprintf(checkName, sizeof(checkName), "Slot %u skill %u constant data exists", slot + 1, sb.skill_id);
            IntCheck(checkName, false);
        }
    }

    IntReport("  Loaded skills: %u / 8", loadedSkills);
    IntCheck("At least 1 skill loaded", loadedSkills >= 1);

    IntReport("");
    return true;
}
