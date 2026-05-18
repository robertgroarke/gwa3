struct BuiltinCombatSignalSummary {
    bool rechargeChanged = false;
    int rechargeSlot = -1;
    bool activeSkillChanged = false;
    bool energyChanged = false;
    bool foeHpChanged = false;
    float distanceBefore = 0.0f;
    float distanceAfter = 0.0f;
    bool distanceClosed = false;
};
