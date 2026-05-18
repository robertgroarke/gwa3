static void RunFroggyOutpostSkillbarSanityChecks() {
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) {
        FroggyFeatureSkip("Skillbar validation", "Skillbar not available");
        return;
    }

    int nonZero = 0;
    for (int i = 0; i < 8; i++) {
        if (bar->skills[i].skill_id != 0) nonZero++;
    }
    FroggyFeatureCheck("Skillbar has skills loaded", nonZero > 0);
    if (nonZero <= 0) return;

    for (int i = 0; i < 8; i++) {
        if (bar->skills[i].skill_id == 0) continue;
        const auto* data = SkillMgr::GetSkillConstantData(bar->skills[i].skill_id);
        FroggyFeatureCheck("Skill constant data exists", data != nullptr);
        if (data) {
            FroggyFeatureCheck("Skill profession in range", data->profession <= 10);
            FroggyFeatureCheck("Skill type in range", data->type <= 24);
        }
        break;
    }
}

static void RunFroggyOutpostInventorySanityChecks() {
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return;

    FroggyFeatureCheck("Gold character plausible", inv->gold_character < 1000000);
    FroggyFeatureCheck("Gold storage plausible", inv->gold_storage < 10000000);
    int bagsFound = 0;
    for (int i = 1; i <= 4; i++) {
        auto* bag = ItemMgr::GetBag(i);
        if (bag && bag->items.buffer) bagsFound++;
    }
    FroggyFeatureCheck("At least 1 backpack bag", bagsFound >= 1);
}

static void RunFroggyOutpostSanityChecks() {
    RunFroggyOutpostSkillbarSanityChecks();
    RunFroggyOutpostInventorySanityChecks();

    const uint32_t myId = AgentMgr::GetMyId();
    if (myId > 0) {
        FroggyFeatureCheck("HasEffect(bogus 9999)=false", !EffectMgr::HasEffect(myId, 9999));
    }
}
