#include "IntegrationTestFroggyFeatureTekksBogrootLoopPreparation.h"
#include "IntegrationTestFroggyFeatureTekksBogrootLoopSkips.h"
#include "IntegrationTestFroggyFeatureTekksBogrootReaccept.h"

static void RunFroggyTekksBogrootPath(bool& preserveSparkflyLoopState) {
    const bool readyForTekksPath = PrepareForTekksBogrootPath();
    const bool reachedTekks = readyForTekksPath && MoveToTekksForQuestDialog();
    if (!reachedTekks) {
        SkipBogrootPathAfterTekksRouteFailure();
        return;
    }

    const bool tekksReadyForDungeon = RunTekksQuestAcceptProof();
    const bool inBogroot = tekksReadyForDungeon ? RunEnterBogrootProof() : false;
    if (!tekksReadyForDungeon) {
        FroggyFeatureSkip("Enter Bogroot", "Tekks dungeon-entry dialog sequence did not complete");
    }

    if (!inBogroot) {
        SkipBogrootPathAfterEntryFailure();
        return;
    }

    RunBogrootBlessingProof();
    const bool returnedToSparkflyAfterDungeon = RunBogrootDungeonLoopProof();
    if (!returnedToSparkflyAfterDungeon) {
        FroggyFeatureSkip("Tekks reaccept after Bogroot return", "Dungeon loop did not return to Sparkfly");
        return;
    }

    RunTekksReacceptAfterBogrootReturn(preserveSparkflyLoopState);
}
