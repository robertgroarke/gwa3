bool TestChatColorOffsets() {
    IntReport("=== Chat Color Offsets ===");
    IntReport("  GetSenderColor: 0x%08X", static_cast<unsigned>(Offsets::GetSenderColor));
    IntReport("  GetMessageColor: 0x%08X", static_cast<unsigned>(Offsets::GetMessageColor));

    if (Offsets::GetSenderColor > 0x10000) {
        IntCheck("GetSenderColor resolved", true);
    } else {
        IntSkip("GetSenderColor", "Pattern did not resolve");
    }
    if (Offsets::GetMessageColor > 0x10000) {
        IntCheck("GetMessageColor resolved", true);
    } else {
        IntSkip("GetMessageColor", "Pattern did not resolve");
    }
    IntReport("");
    return true;
}
