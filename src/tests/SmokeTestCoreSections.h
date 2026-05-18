// Core smoke-test sections. Included by SmokeTest.cpp inside GWA3::SmokeTest.

static void RunScannerSmokeSection() {
    Report("--- Scanner ---");
    Check("Scanner initialized", Scanner::IsInitialized());
    auto text = Scanner::GetTextSection();
    auto rdata = Scanner::GetRdataSection();
    Check(".text section found", text.size > 0);
    Check(".rdata section found", rdata.size > 0);
    Report(".text: 0x%08X size %u", text.start, text.size);
    Report(".rdata: 0x%08X size %u", rdata.start, rdata.size);
    Report("");
}

static void RunCoreOffsetsSmokeSection() {
    Report("--- Core Offsets (post-processed) ---");
    CheckNonZero("BasePointer", Offsets::BasePointer);
    CheckNonZero("PacketSend", Offsets::PacketSend);
    CheckNonZero("PacketLocation", Offsets::PacketLocation);
    CheckNonZero("AgentBase", Offsets::AgentBase);
    CheckNonZero("MyID", Offsets::MyID);
    CheckNonZero("SkillBase", Offsets::SkillBase);
    CheckNonZero("Move", Offsets::Move);
    CheckNonZero("FrameArray", Offsets::FrameArray);
    CheckNonZero("UIMessage", Offsets::UIMessage);
    CheckNonZero("SendFrameUIMsg", Offsets::SendFrameUIMsg);
    Report("Offsets resolved: %d/%d (failed: %d)",
           Offsets::GetResolvedCount(),
           Offsets::GetResolvedCount() + Offsets::GetFailedCount(),
           Offsets::GetFailedCount());
    Report("");
}

static void RunFrameUiSmokeSection() {
    Report("--- Frame UI ---");
    uintptr_t playFrame = UIMgr::GetFrameByHash(UIMgr::Hashes::PlayButton);
    Report("PlayButton frame: 0x%08X", playFrame);
    if (playFrame) {
        uint32_t frameId = UIMgr::GetFrameId(playFrame);
        uint32_t state = UIMgr::GetFrameState(playFrame);
        Report("  frame_id: %u, state: 0x%X", frameId, state);
        Check("PlayButton frame found", true);
        Check("PlayButton is created", (state & UIMgr::FRAME_CREATED) != 0);
    } else {
        Report("  Not found (may not be at char select)");
    }
    Report("");
}
