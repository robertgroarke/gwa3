static bool OpenMerchantContextWithSessionHarnessBody(uint32_t npcId, float npcX, float npcY) {
    MovePlayerNear(npcX, npcY, 70.0f, 12000);

    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    FroggyFeatureReport("    Player pos before interact: (%.0f, %.0f) dist=%.0f",
              meX, meY, AgentMgr::GetDistance(meX, meY, npcX, npcY));
    ReportMerchantPreInteractState("Froggy harness-body pre-interact snapshot", npcId, npcX, npcY);
    ReportMerchantRuntimeContext("Froggy harness-body runtime context");

    AgentMgr::ChangeTarget(npcId);
    Sleep(250);
    FroggyFeatureReport("    step 0 complete: ChangeTarget(%u)", npcId);
    ReportMerchantPreInteractState("Froggy harness-body post-target snapshot", npcId, npcX, npcY);
    ReportMerchantRuntimeContext("Froggy harness-body post-target runtime context");

    FroggyFeatureReport("    step 1: native AgentMgr::InteractNPC(%u) x3 with dwell", npcId);
    for (int nativeAttempt = 1; nativeAttempt <= 3; ++nativeAttempt) {
        FroggyFeatureReport("      native interact attempt %d", nativeAttempt);
        AgentMgr::InteractNPC(npcId);
        Sleep(500);
    }
    FroggyFeatureReport("    step 1 complete");

    FroggyFeatureReport("    Native-interact dwell: waiting 2500ms after InteractNPC...");
    Sleep(2500);
    FroggyFeatureReport("    Native-interact wait complete");
    ReportDialogSnapshot("After Froggy harness-body native-interact dwell");
    if (IsMerchantContextVisible("native-interact dwell")) {
        return true;
    }

    FroggyFeatureReport("    step 2: raw GoNPC packet fallback (consumables harness parity)");
    for (int packetAttempt = 1; packetAttempt <= 3; ++packetAttempt) {
        FroggyFeatureReport("      Raw GoNPC attempt %d", packetAttempt);
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        Sleep(500);
    }
    Sleep(2500);
    ReportDialogSnapshot("After Froggy harness-body raw-GoNPC dwell");
    if (IsMerchantContextVisible("raw-GoNPC dwell")) {
        return true;
    }
    return WaitForMerchantContext(1500);
}
