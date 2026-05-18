#include "IntegrationTestFroggyFeatureCombatAgentReadSupport.h"

static void RunCombatAgentReadValidation(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5C: Combat Agent Read Validation (%s) ===", label ? label : "default");

    CombatAgentReadPair before = {};
    if (!CaptureCombatAgentReadPair(AgentMgr::GetMyId(), foeId, "before validation", before)) return;
    if (!ValidateCombatAgentRawPointers(before)) return;
    ValidateCombatAgentSnapshotSanity(before, false);
    ReportCombatAgentReadPair("before", before);

    Sleep(500);

    CombatAgentReadPair after = {};
    if (!CaptureCombatAgentReadPair(before.player.agentId, before.foe.agentId, "after dwell", after)) return;
    ReportCombatAgentReadPair("after", after);
    ValidateCombatAgentReadStability(before, after);
}
