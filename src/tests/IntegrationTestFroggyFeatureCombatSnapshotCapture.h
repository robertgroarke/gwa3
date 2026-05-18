static bool CaptureCombatActorSnapshot(uint32_t agentId, CombatActorSnapshot& out) {
    out = {};
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (!agent || agent->type != 0xDB) return false;
    auto* living = static_cast<AgentLiving*>(agent);
    __try {
        out.valid = true;
        out.agentId = living->agent_id;
        out.allegiance = living->allegiance;
        out.hp = living->hp;
        out.energy = living->energy;
        out.maxEnergy = living->max_energy;
        out.castingSkill = living->skill;
        out.x = living->x;
        out.y = living->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}

static bool CapturePlayerSkillbarSnapshot(SkillbarSnapshot& out) {
    out = {};
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) return false;
    __try {
        out.valid = true;
        out.agentId = bar->agent_id;
        for (int i = 0; i < 8; ++i) {
            out.skillIds[i] = bar->skills[i].skill_id;
            out.recharge[i] = bar->skills[i].recharge;
            if (out.skillIds[i] != 0) ++out.nonZeroSkills;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}

static bool CaptureCombatObservabilitySnapshot(uint32_t foeId, CombatObservabilitySnapshot& out) {
    out = {};
    const uint32_t myId = AgentMgr::GetMyId();
    if (!myId) return false;

    CaptureCombatActorSnapshot(myId, out.player);
    CaptureCombatActorSnapshot(foeId, out.foe);
    CapturePlayerSkillbarSnapshot(out.skillbar);
    out.targetId = AgentMgr::GetTargetId();

    PartyInfo* playerParty = ResolveTestPlayerParty();
    if (playerParty && playerParty->heroes.buffer) {
        out.heroCount = playerParty->heroes.size;
        bool allReadable = out.heroCount > 0;
        const uint32_t cap = out.heroCount < 8 ? out.heroCount : 8;
        for (uint32_t i = 0; i < cap; ++i) {
            const uint32_t heroAgentId = playerParty->heroes.buffer[i].agent_id;
            out.heroAgentIds[i] = heroAgentId;
            if (!heroAgentId || AgentMgr::GetAgentByID(heroAgentId) == nullptr) {
                allReadable = false;
            }
        }
        out.heroAgentsReadable = allReadable;
    }

    return out.player.valid || out.foe.valid || out.skillbar.valid;
}
