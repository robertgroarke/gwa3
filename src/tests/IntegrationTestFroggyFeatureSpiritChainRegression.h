#include "IntegrationTestFroggyFeatureSpiritChainSetupSupport.h"
#include "IntegrationTestFroggyFeatureSpiritChainTraceSupport.h"

static void RunBuiltinSpiritChainRegression(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5J: Spirit Chain Regression (%s) ===", label ? label : "default");

    if (!PrepareSpiritChainRegressionEngagement(foeId)) return;

    SpiritChainObservation observation = {};
    for (int step = 1; step <= 4; ++step) {
        auto* foe = GetAgentLivingRaw(foeId);
        if (!foe || foe->hp <= 0.0f) {
            FroggyFeatureSkip("Spirit chain regression continuation", "Foe died or became unreadable");
            break;
        }

        const bool stepExecuted = Bot::Froggy::ExecuteBuiltinCombatStep(foeId);
        if (!stepExecuted) {
            FroggyFeatureSkip("Spirit chain regression step", "Builtin combat step wrapper refused current target");
            break;
        }

        ObserveSpiritChainStep(step, observation);
        if (observation.sawSlot2 && observation.sawFollowUpSpirit) {
            FroggyFeatureReport("  Spirit chain proof satisfied by step %d", step);
            break;
        }

        Sleep(250);
    }

    FroggyFeatureCheck("Spirit chain used Signet of Spirits (slot 2)", observation.sawSlot2);
    FroggyFeatureCheck("Spirit chain used follow-up spirit after Signet of Spirits (slot 3/4/5)", observation.sawFollowUpSpirit);
}
