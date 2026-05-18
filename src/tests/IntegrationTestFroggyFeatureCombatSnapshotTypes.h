struct CombatActorSnapshot {
    bool valid = false;
    uint32_t agentId = 0;
    uint32_t allegiance = 0;
    float hp = 0.0f;
    float energy = 0.0f;
    uint32_t maxEnergy = 0;
    uint16_t castingSkill = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct SkillbarSnapshot {
    bool valid = false;
    uint32_t agentId = 0;
    uint32_t skillIds[8] = {};
    uint32_t recharge[8] = {};
    int nonZeroSkills = 0;
};

struct CombatObservabilitySnapshot {
    CombatActorSnapshot player;
    CombatActorSnapshot foe;
    SkillbarSnapshot skillbar;
    uint32_t targetId = 0;
    uint32_t heroCount = 0;
    uint32_t heroAgentIds[8] = {};
    bool heroAgentsReadable = false;
};

struct CombatCastTelemetrySnapshot {
    bool valid = false;
    SkillTestCandidate candidate = {};
    uint32_t targetId = 0;
    uint32_t energy = 0;
    uint16_t activeSkill = 0;
    uint32_t recharge = 0;
    uint32_t event = 0;
};
