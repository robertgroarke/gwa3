#include "IntegrationTestFroggyFeatureCombatTargetSelectionBranches.h"

static void RunCombatTargetSelectionCoverage(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5H: Combat Target Selection Coverage (%s) ===", label ? label : "default");

    RunLastCombatActionTargetSelectionCheck();
    RunDefaultFoeTargetSelectionBranch(foeId);
    RunCastingFoeTargetSelectionBranch(foeId);
    RunEnchantedFoeTargetSelectionBranch(foeId);
    RunMeleeFoeTargetSelectionBranch(foeId);
}
