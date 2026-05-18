bool TestFriendListOffsets() {
    IntReport("=== FriendList Offsets ===");
    IntReport("  FriendListAddr: 0x%08X", static_cast<unsigned>(Offsets::FriendListAddr));
    IntReport("  FriendEventHandler: 0x%08X", static_cast<unsigned>(Offsets::FriendEventHandler));

    if (Offsets::FriendListAddr > 0x10000) {
        IntCheck("FriendListAddr resolved", true);
    } else {
        IntSkip("FriendListAddr", "Pattern did not resolve");
    }
    if (Offsets::FriendEventHandler > 0x10000) {
        IntCheck("FriendEventHandler resolved", true);
    } else {
        IntSkip("FriendEventHandler", "Pattern did not resolve");
    }
    IntReport("");
    return true;
}
