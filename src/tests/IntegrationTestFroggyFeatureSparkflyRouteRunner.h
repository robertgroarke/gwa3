#include "IntegrationTestFroggyFeatureSparkflyRoutePhases.h"

int RunFroggySparkflyRouteTest() {
    ResetSparkflyRouteTestState();
    StartWatchdog();

    if (!BootstrapSparkflyRouteTestToGadds()) {
        return FinishSparkflyRouteTest();
    }

    if (!SetupSparkflyRouteOutpost()) {
        return FinishSparkflyRouteTest();
    }

    if (!EnterSparkflyForRouteTest()) {
        return FinishSparkflyRouteTest();
    }

    RunSparkflyRouteCombatProbe();

    FroggyFeatureReport("=== SPARKFLY ROUTE TEST: Route To Tekks ===");
    Bot::Froggy::g_sparkflyTraversalCombatStats = {};
    const bool reachedTekks = MoveToTekksForQuestDialog();
    FroggyFeatureCheck("Reached Tekks via Froggy Sparkfly route", reachedTekks);
    ReportSparkflyTraversalStats();
    RunSparkflyRouteTekksEntry(reachedTekks);

    return FinishSparkflyRouteTest();
}
