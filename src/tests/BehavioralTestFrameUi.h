static void RunBehavioralFrameUiTest() {
    CmdReport("--- Test 10: Frame UI ---");
    uintptr_t reconnect = UIMgr::GetFrameByHash(UIMgr::Hashes::ReconnectYes);
    bool reconnectVisible = UIMgr::IsFrameVisible(UIMgr::Hashes::ReconnectYes);
    CmdReport("ReconnectYes frame: 0x%08X, visible: %s", reconnect, reconnectVisible ? "yes" : "no");
    CmdCheck("Frame lookup does not crash", true);

    uintptr_t root = UIMgr::GetRootFrame();
    CmdReport("Root frame: 0x%08X", root);
    CmdCheck("Root frame is non-null", root != 0);
    CmdReport("");
}
