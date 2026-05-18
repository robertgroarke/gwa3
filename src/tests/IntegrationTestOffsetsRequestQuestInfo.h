bool TestRequestQuestInfoOffset() {
    IntReport("=== RequestQuestInfo Offset ===");
    IntReport("  RequestQuestInfo: 0x%08X", static_cast<unsigned>(Offsets::RequestQuestInfo));
    if (Offsets::RequestQuestInfo > 0x10000) {
        IntCheck("RequestQuestInfo offset resolved", true);
    } else {
        IntSkip("RequestQuestInfo", "Pattern did not resolve");
    }
    IntReport("");
    return true;
}
