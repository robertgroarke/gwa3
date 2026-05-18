#include "IntegrationTestFroggyFeatureFreshSparkflyOutpostReturn.h"
#include "IntegrationTestFroggyFeatureFreshSparkflyOutpostPrep.h"
#include "IntegrationTestFroggyFeatureFreshSparkflyEntryPush.h"

static bool ResetToFreshSparkflyInstanceForDungeonRun() {
    FroggyFeatureReport("=== PHASE 5K.5: Fresh Sparkfly Reset Before Dungeon Run ===");

    return EnsureAtGaddsForFreshSparkflyReset() &&
           PrepareGaddsForFreshSparkflyEntry() &&
           EnterFreshSparkflyFromGadds();
}
