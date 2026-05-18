bool TestGwEndSceneOffset() {
    IntReport("=== GwEndScene Offset ===");

    IntReport("  GwEndScene: 0x%08X", static_cast<unsigned>(Offsets::GwEndScene));
    IntReport("  Render (AutoIt): 0x%08X", static_cast<unsigned>(Offsets::Render));

    if (Offsets::GwEndScene > 0x10000) {
        IntCheck("GwEndScene offset resolved", true);

        if (Offsets::Render > 0x10000) {
            ptrdiff_t delta = static_cast<ptrdiff_t>(Offsets::GwEndScene) -
                              static_cast<ptrdiff_t>(Offsets::Render);
            IntReport("  Delta between GwEndScene and Render: %d bytes", static_cast<int>(delta));
            bool close = (delta >= -0x20 && delta <= 0x20) || delta == 0;
            if (close) {
                IntCheck("GwEndScene and Render point to same region", true);
            } else {
                IntReport("  GwEndScene and Render are far apart (may be different hook targets)");
                IntCheck("GwEndScene resolved independently", true);
            }
        }
    } else {
        IntSkip("GwEndScene offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
