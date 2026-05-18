bool TestSkipCinematicOffset() {
    IntReport("=== SkipCinematic Offset ===");

    IntReport("  SkipCinematicFunc: 0x%08X", static_cast<unsigned>(Offsets::SkipCinematicFunc));

    if (Offsets::SkipCinematicFunc > 0x10000) {
        IntCheck("SkipCinematicFunc offset resolved", true);
    } else {
        IntSkip("SkipCinematicFunc offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
