static bool ValidateBuiltinCombatStepSignal(uint32_t foeId,
                                            const CombatObservabilitySnapshot& before,
                                            const CombatObservabilitySnapshot& after,
                                            bool isAutoAttack,
                                            bool observedSignal) {
    const BuiltinCombatSignalSummary signal = BuildBuiltinCombatSignalSummary(before, after, foeId);
    FroggyFeatureReport("  Builtin combat after: active=%u->%u energy=%.3f->%.3f foeHp=%.3f->%.3f rechargeSlot=%d distance=%.0f->%.0f",
              before.player.castingSkill,
              after.player.castingSkill,
              before.player.energy,
              after.player.energy,
              before.foe.hp,
              after.foe.hp,
              signal.rechargeSlot,
              signal.distanceBefore,
              signal.distanceAfter);

    const bool validConcreteSignal = IsBuiltinCombatSignalValid(signal, isAutoAttack);
    if (!validConcreteSignal) {
        FroggyFeatureSkip("Builtin combat single-step proof", isAutoAttack
            ? "Auto-attack selected but no hit or range-closing signal was observed"
            : "Skill action selected but no cast-side signal was observed");
        return false;
    }
    FroggyFeatureCheck("Builtin combat proof observed concrete signal", observedSignal);
    FroggyFeatureCheck("Builtin combat proof signal matches selected action", validConcreteSignal);
    return true;
}
