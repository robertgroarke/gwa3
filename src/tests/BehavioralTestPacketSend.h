static void RunBehavioralPacketSendTest() {
    CmdReport("--- Test 6: Benign Packet Send ---");
    CtoS::SendPacket(1, 0x0A); // HEARTBEAT
    Sleep(1000);
    CmdCheck("Heartbeat packet sent without crash", true);
    CmdReport("");
}
