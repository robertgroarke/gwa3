// Identify/salvage integration test body. Included by IntegrationTestSession.cpp
// so it can use the session-local harness helpers while extraction continues.

#include "IntegrationTestIdentifySalvageStageSupport.h"

bool TestIdentifySalvageIsolation() {
    IntReport("=== GWA3 Identify/Salvage Isolation Harness ===");

    if (ReadMapId() == 0 || ReadMyId() == 0) {
        IntSkip("Identify/salvage isolation", "Not in game");
        IntReport("");
        return false;
    }

    const IdentifySalvageIsolationStage stage = GetIdentifySalvageIsolationStage();
    IntReport("  Identify/salvage stage: %s", DescribeIdentifySalvageIsolationStage(stage));
    ConfigureIdentifySalvageRuntimeOverrides();
    if (!EnsureIdentifySalvageHarnessReady()) return false;

    ReportIdentifySalvageSummary("Pre-run summary");
    ReportGoldSalvageCandidates("Pre-run gold salvage candidates");

    const uint32_t identifyCandidatesBefore = CountIdentifyCandidates();
    if (stage == IdentifySalvageIsolationStage::IdentifyOnly) {
        return RunIdentifyOnlyStage(identifyCandidatesBefore);
    }

    if (stage == IdentifySalvageIsolationStage::Full) {
        return RunFullIdentifySalvageStage();
    }

    RunIdentifySalvagePrepPass();
    IdentifySalvageManualContext context{};
    if (!LoadIdentifySalvageManualContext(context)) return false;

    if (stage == IdentifySalvageIsolationStage::LegacyBotshubTrackedChain) {
        return RunLegacyBotshubTrackedChainStage(context);
    }

    if (stage == IdentifySalvageIsolationStage::NativeSalvage ||
        stage == IdentifySalvageIsolationStage::NativeSalvageEnter ||
        stage == IdentifySalvageIsolationStage::LegacyBotshubStartOnly ||
        stage == IdentifySalvageIsolationStage::LegacyBotshubSalvage ||
        stage == IdentifySalvageIsolationStage::LegacyBotshubSalvageEnter) {
        return RunQueuedIdentifySalvageStage(stage, context);
    }

    if (stage == IdentifySalvageIsolationStage::LegacyBotshubSalvageDone ||
        stage == IdentifySalvageIsolationStage::LegacyBotshubSalvageCancel) {
        return RunLegacyBotshubFollowupStage(stage, context);
    }

    return RunManualSingleSalvageStage(stage, context);
}
