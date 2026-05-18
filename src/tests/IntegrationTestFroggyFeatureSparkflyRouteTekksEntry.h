static void RunSparkflyRouteTekksEntry(bool reachedTekks) {
    if (reachedTekks) {
        const bool tekksReadyForDungeon = RunTekksQuestAcceptProof();
        FroggyFeatureCheck("Sparkfly route Tekks dialog sequence completed", tekksReadyForDungeon);
        if (tekksReadyForDungeon) {
            const bool enteredBogroot = RunEnterBogrootProof();
            FroggyFeatureCheck("Sparkfly route entered Bogroot after Tekks dialog", enteredBogroot);
        } else {
            FroggyFeatureSkip("Sparkfly route entered Bogroot after Tekks dialog",
                    "Tekks dialog sequence did not complete");
        }
    } else {
        FroggyFeatureSkip("Sparkfly route Tekks dialog sequence completed", "Did not reach Tekks");
        FroggyFeatureSkip("Sparkfly route entered Bogroot after Tekks dialog", "Did not reach Tekks");
    }
}
