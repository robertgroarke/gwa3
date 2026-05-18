static bool CaptureCombatAgentReadPair(uint32_t playerId, uint32_t foeId, const char* phaseLabel, CombatAgentReadPair& out) {
    out = {};
    const bool playerOk = CaptureCombatActorSnapshot(playerId, out.player);
    const bool foeOk = CaptureCombatActorSnapshot(foeId, out.foe);

    char playerCheck[96];
    char foeCheck[96];
    snprintf(playerCheck, sizeof(playerCheck), "Combat agent read captured player %s", phaseLabel ? phaseLabel : "snapshot");
    snprintf(foeCheck, sizeof(foeCheck), "Combat agent read captured foe %s", phaseLabel ? phaseLabel : "snapshot");
    FroggyFeatureCheck(playerCheck, playerOk);
    FroggyFeatureCheck(foeCheck, foeOk);

    if (!playerOk || !foeOk) {
        FroggyFeatureSkip("Combat agent read validation", "Could not capture player/foe snapshot");
        return false;
    }
    return true;
}
