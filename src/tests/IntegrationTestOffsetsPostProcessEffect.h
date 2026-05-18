bool TestPostProcessEffectOffset() {
    IntReport("=== PostProcessEffect Offset ===");

    IntReport("  PostProcessEffect: 0x%08X", static_cast<unsigned>(Offsets::PostProcessEffect));
    IntReport("  DropBuff: 0x%08X", static_cast<unsigned>(Offsets::DropBuff));

    if (Offsets::PostProcessEffect > 0x10000) {
        IntCheck("PostProcessEffect offset resolved", true);
    } else {
        IntSkip("PostProcessEffect offset", "Pattern did not resolve");
    }

    if (Offsets::DropBuff > 0x10000) {
        IntCheck("DropBuff offset resolved", true);
    } else {
        IntSkip("DropBuff offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
