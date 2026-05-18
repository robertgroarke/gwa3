bool TestSkillActivation() {
    IntReport("===  Skill Activation ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        IntSkip("Skill activation", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area) {
        IntSkip("Skill activation", "AreaInfo unavailable");
        IntReport("");
        return false;
    }

    IntReport("  Map %u regionType=%u (%s)",
              mapId,
              area->type,
              DescribeMapRegionType(area->type));
    if (!IsSkillCastMapType(area->type)) {
        IntSkip("Skill activation", "Current instance type is outpost-like; explorable phase required");
        IntReport("");
        return false;
    }

    if (!WaitForStablePlayerState()) {
        IntReport("  Player runtime state: TypeMap=0x%X ModelState=%u", GetPlayerTypeMap(), GetPlayerModelState());
        DumpSkillbarForSkillTest();
        IntSkip("Skill activation", "Player state not stable after login/targeting");
        IntReport("");
        return false;
    }

    SkillTestCandidate candidate{};
    if (!TryChooseSkillTestCandidate(candidate)) {
        DumpSkillbarForSkillTest();
        IntSkip("Skill activation", "No suitable recharged skill/target combination found");
        IntReport("");
        return false;
    }

    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = GetAgentLivingRaw(ReadMyId());
    if (!bar || !me) {
        IntSkip("Skill activation", "Skillbar or player agent unavailable");
        IntReport("");
        return false;
    }

    SkillbarSkill before = bar->skills[candidate.slot - 1];
    const uint16_t activeSkillBefore = me->skill;
    const uint32_t energyBefore = GetCurrentEnergyPoints();
    IntReport("  Casting slot %u skill %u target=%u targetType=%u type=%u energyCost=%u baseRecharge=%u activation=%.2f recharge=%u event=%u active=%u energy=%u",
              candidate.slot,
              candidate.skillId,
              candidate.targetId,
              candidate.targetType,
              candidate.type,
              candidate.energyCost,
              candidate.baseRecharge,
              candidate.activation,
              before.recharge,
              before.event,
              activeSkillBefore,
              energyBefore);

    SkillMgr::UseSkill(candidate.slot, candidate.targetId, 0);

    const bool skillStarted = WaitFor("skill activation state change", 5000, [candidate, before, activeSkillBefore, energyBefore]() {
        Skillbar* liveBar = SkillMgr::GetPlayerSkillbar();
        auto* liveMe = GetAgentLivingRaw(ReadMyId());
        if (!liveBar || !liveMe) return false;

        const SkillbarSkill& after = liveBar->skills[candidate.slot - 1];
        const uint32_t energyAfter = GetCurrentEnergyPoints();
        return after.recharge != before.recharge ||
               after.event != before.event ||
               liveMe->skill != activeSkillBefore ||
               liveMe->skill == candidate.skillId ||
               energyAfter < energyBefore;
    });

    bar = SkillMgr::GetPlayerSkillbar();
    me = GetAgentLivingRaw(ReadMyId());
    if (bar && me) {
        const SkillbarSkill& after = bar->skills[candidate.slot - 1];
        IntReport("  After cast: recharge=%u event=%u active=%u energy=%u",
                  after.recharge,
                  after.event,
                  me->skill,
                  GetCurrentEnergyPoints());
    }

    IntCheck("Skill activation changes runtime state", skillStarted);
    IntReport("");
    return skillStarted;
}

// Loot pickup
