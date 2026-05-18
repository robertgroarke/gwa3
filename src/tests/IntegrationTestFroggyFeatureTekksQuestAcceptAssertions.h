static void CheckTekksQuestAcceptResult(
    bool prepared,
    uint32_t activeBefore,
    uint32_t activeAfter,
    uint32_t questLogBefore,
    uint32_t questLogAfter,
    const Quest* questBefore,
    const Quest* questAfter) {
    FroggyFeatureCheck("Tekks Froggy dungeon-entry preparation completed", prepared);
    FroggyFeatureCheck("Tekks quest present after Froggy preparation", questAfter != nullptr);
    FroggyFeatureCheck("Tekks active quest changed or remained Tekks quest",
             activeAfter == GWA3::QuestIds::TEKKS_WAR || activeBefore == GWA3::QuestIds::TEKKS_WAR);
    FroggyFeatureCheck("Quest log size stayed stable or grew after Froggy preparation", questLogAfter >= questLogBefore);
    if (!questBefore && questAfter) {
        FroggyFeatureCheck("Tekks accept created a new quest log entry", true);
    } else if (questBefore && questAfter) {
        FroggyFeatureSkip("Tekks accept created a new quest log entry",
                "Quest already existed before accept validation");
    }
}
