struct BuiltinCombatStepAction {
    const char* description = nullptr;
    bool autoAttack = false;
};

static bool ExecuteBuiltinCombatProofStep(uint32_t foeId, BuiltinCombatStepAction& outAction) {
    outAction = {};
    const bool stepExecuted = Bot::Froggy::ExecuteBuiltinCombatStep(foeId);
    FroggyFeatureCheck("Builtin combat proof step executed", stepExecuted);
    if (!stepExecuted) {
        FroggyFeatureSkip("Builtin combat single-step proof", "Froggy combat step wrapper refused current target");
        return false;
    }

    outAction.description = Bot::Froggy::g_combatSession.last_step;
    FroggyFeatureReport("  Builtin combat action: %s", outAction.description ? outAction.description : "<null>");
    ReportBuiltinCombatTrace();

    const bool actionChosen = IsBuiltinCombatActionChosen(outAction.description);
    outAction.autoAttack = outAction.description && strncmp(outAction.description, "auto_attack", 11) == 0;
    if (!actionChosen) {
        FroggyFeatureSkip("Builtin combat single-step proof", "Builtin combat step selected no action");
        return false;
    }
    FroggyFeatureCheck("Builtin combat proof action chosen", true);
    return true;
}
