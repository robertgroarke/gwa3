static void RunTekksReacceptAfterBogrootReturn(bool& preserveSparkflyLoopState) {
    ReportQuestSnapshot("Quest state after Bogroot loop return");
    const bool reachedTekksAfterReturn = MoveToTekksForQuestDialog();
    FroggyFeatureCheck("Reached Tekks after Bogroot return", reachedTekksAfterReturn);
    if (!reachedTekksAfterReturn) {
        FroggyFeatureSkip("Tekks reaccept after Bogroot return", "Could not reach Tekks after returning to Sparkfly");
        return;
    }

    RunTekksQuestAcceptProof();
    ReportQuestSnapshot("Quest state after Tekks reaccept on Sparkfly return");
    const bool questPresentAfterReturn =
        QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR) != nullptr ||
        QuestMgr::GetActiveQuestId() == GWA3::QuestIds::TEKKS_WAR;
    FroggyFeatureCheck("Tekks quest present after Sparkfly return loop", questPresentAfterReturn);
    preserveSparkflyLoopState =
        questPresentAfterReturn &&
        MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP &&
        MapMgr::GetIsMapLoaded() &&
        AgentMgr::GetMyId() > 0;
}
