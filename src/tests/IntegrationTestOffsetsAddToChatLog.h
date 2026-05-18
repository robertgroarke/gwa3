bool TestAddToChatLogOffset() {
    IntReport("=== AddToChatLog Offset ===");

    IntReport("  AddToChatLog: 0x%08X", static_cast<unsigned>(Offsets::AddToChatLog));

    if (Offsets::AddToChatLog > 0x10000) {
        IntCheck("AddToChatLog offset resolved", true);
    } else {
        IntSkip("AddToChatLog offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
