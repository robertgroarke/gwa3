static bool PrepareForTekksBogrootPath() {
    if (s_isolatedExplorableFlaggingMode) {
        const bool lootWorked = RunExplorableLootPickupProof();
        if (!lootWorked) {
            FroggyFeatureSkip("Path to Tekks", "Loot proof did not complete cleanly; continuing quest path anyway");
        }
        return true;
    }

    FroggyFeatureSkip("Explorable loot pickup", "Deferred in full Froggy loop to keep the Tekks path aligned with the real bot flow");
    const bool readyForTekksPath = ResetToFreshSparkflyInstanceForDungeonRun();
    FroggyFeatureCheck("Fresh Sparkfly reset completed before Tekks path", readyForTekksPath);
    return readyForTekksPath;
}
