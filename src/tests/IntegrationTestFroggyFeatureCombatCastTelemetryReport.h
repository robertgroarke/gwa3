static void ReportCombatCastTelemetrySnapshot(const char* phaseLabel,
                                              const SkillTestCandidate& candidate,
                                              const CombatCastTelemetrySnapshot& snap) {
    FroggyFeatureReport("  Combat cast %s: slot=%u skill=%u target=%u liveTarget=%u energy=%u recharge=%u event=%u active=%u",
              phaseLabel ? phaseLabel : "snapshot",
              candidate.slot,
              candidate.skillId,
              candidate.targetId,
              snap.targetId,
              snap.energy,
              snap.recharge,
              snap.event,
              snap.activeSkill);
}
