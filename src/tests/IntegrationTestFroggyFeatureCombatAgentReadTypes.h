struct CombatAgentReadPair {
    CombatActorSnapshot player = {};
    CombatActorSnapshot foe = {};
};

static bool IsSaneCombatPosition(float x, float y) {
    return _finite(x) && _finite(y) && fabsf(x) < 50000.0f && fabsf(y) < 50000.0f;
}
