static bool ObserveBuiltinCombatSignal(
    uint32_t foeId,
    const CombatObservabilitySnapshot& before,
    bool isAutoAttack,
    CombatObservabilitySnapshot& after) {
    DWORD observeStart = GetTickCount();
    while ((GetTickCount() - observeStart) < 5000) {
        Sleep(200);
        if (!CaptureCombatObservabilitySnapshot(foeId, after)) continue;

        const BuiltinCombatSignalSummary signal = BuildBuiltinCombatSignalSummary(before, after, foeId);
        if (IsBuiltinCombatSignalValid(signal, isAutoAttack)) {
            return true;
        }
    }
    return false;
}
