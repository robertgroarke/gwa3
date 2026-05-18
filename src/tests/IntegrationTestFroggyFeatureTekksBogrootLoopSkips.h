static void SkipBogrootPathAfterTekksRouteFailure() {
    FroggyFeatureSkip("Tekks quest accept", "Could not reach Tekks path endpoint");
    FroggyFeatureSkip("Enter Bogroot", "Did not reach Tekks");
    FroggyFeatureSkip("Bogroot blessing", "Did not reach Tekks");
    FroggyFeatureSkip("Bogroot dungeon loop", "Did not reach Tekks");
    FroggyFeatureSkip("Tekks reaccept after Bogroot return", "Did not reach Tekks");
}

static void SkipBogrootPathAfterEntryFailure() {
    FroggyFeatureSkip("Bogroot blessing", "Did not enter Bogroot Growths");
    FroggyFeatureSkip("Bogroot dungeon loop", "Did not enter Bogroot Growths");
    FroggyFeatureSkip("Tekks reaccept after Bogroot return", "Dungeon loop never started");
}
