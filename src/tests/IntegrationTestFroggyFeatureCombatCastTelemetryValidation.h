#include "IntegrationTestFroggyFeatureCombatCastTelemetrySupport.h"

static void RunCombatTargetAndCastTelemetryValidation(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5E: Combat Target/Cast Telemetry Validation (%s) ===", label ? label : "default");

    PrepareCombatCastTelemetryTarget(foeId);

    SkillTestCandidate candidate = {};
    if (!ChooseCombatCastTelemetryCandidate(candidate)) return;

    CombatCastTelemetrySnapshot before = {};
    const bool beforeCaptured = CaptureCombatCastTelemetrySnapshot(candidate, before);
    FroggyFeatureCheck("Combat cast telemetry captured before cast", beforeCaptured);
    if (!beforeCaptured) {
        FroggyFeatureSkip("Combat target/cast telemetry validation", "Could not capture pre-cast telemetry");
        return;
    }
    ReportCombatCastTelemetrySnapshot("before", candidate, before);

    const bool telemetryChanged = TriggerCombatCastTelemetryProbe(candidate, before);
    FroggyFeatureCheck("Combat cast telemetry changes runtime state", telemetryChanged);

    CombatCastTelemetrySnapshot after = {};
    const bool afterCaptured = CaptureCombatCastTelemetrySnapshot(candidate, after);
    FroggyFeatureCheck("Combat cast telemetry captured after cast", afterCaptured);
    if (!afterCaptured) {
        FroggyFeatureSkip("Combat target/cast telemetry after cast", "Could not capture post-cast telemetry");
        return;
    }
    ReportCombatCastTelemetrySnapshot("after", candidate, after);

    const bool observedConcreteSignal = CombatCastTelemetryChanged(candidate, before, after);
    FroggyFeatureCheck("Combat cast telemetry has concrete observable signal", observedConcreteSignal);
    FroggyFeatureCheck("Combat target telemetry still has target after cast", after.targetId != 0);
}
