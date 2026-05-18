static void InteractBogrootBlessingNpc(uint32_t npcId) {
    DialogMgr::ResetHookState();
    DialogMgr::ResetRecentUITrace();
    FroggyFeatureReport("  Reset DialogMgr hook state before blessing interact");

    AgentMgr::CancelAction();
    AgentMgr::ChangeTarget(npcId);
    Sleep(500);
    FroggyFeatureReport("  ChangeTarget to agent=%u (targetId now=%u)", npcId, AgentMgr::GetTargetId());

    for (int attempt = 1; attempt <= 3; ++attempt) {
        AgentMgr::InteractNPC(npcId);
        FroggyFeatureReport("  Sent native InteractNPC attempt %d to agent=%u (lastUi=0x%X sender=%u lastDialog=0x%X)",
                  attempt,
                  npcId,
                  DialogMgr::GetLastUIMessageId(),
                  DialogMgr::GetDialogSenderAgentId(),
                  DialogMgr::GetLastDialogId());
        Sleep(1000);
    }

    QuestMgr::Dialog(DIALOG_ACCEPT_BLESSING);
    FroggyFeatureReport("  Sent QuestMgr::Dialog(0x%X)", DIALOG_ACCEPT_BLESSING);
    Sleep(3000);
    FroggyFeatureReport("  Kept DialogMgr active throughout blessing interaction");
}
