static void RunBehavioralInstanceTimeTest() {
    CmdReport("--- Test 7: Instance Time ---");
    uint32_t time1 = MapMgr::GetInstanceTime();
    Sleep(1500);
    uint32_t time2 = MapMgr::GetInstanceTime();
    CmdReport("Instance time: %u -> %u", time1, time2);
    CmdCheck("Instance time is increasing", time2 > time1);
    CmdReport("");
}
