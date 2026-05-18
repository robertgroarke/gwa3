static void RunBehavioralPingTest() {
    CmdReport("--- Test 5: Ping ---");
    uint32_t ping = ChatMgr::GetPing();
    CmdReport("Ping: %u ms", ping);
    CmdCheck("Ping in plausible range (0-2000)", ping <= 2000);
    CmdReport("");
}
