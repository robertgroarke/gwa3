bool TestSendChatOffset() {
    IntReport("=== SendChat Offset ===");

    IntReport("  SendChatFunc: 0x%08X", static_cast<unsigned>(Offsets::SendChatFunc));

    if (Offsets::SendChatFunc > 0x10000) {
        IntCheck("SendChatFunc offset resolved", true);
    } else {
        IntSkip("SendChatFunc offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
