bool TestLevelDataBypassPatch() {
    IntReport("=== Level-Data Bypass Patch ===");

    IntReport("  LevelDataBypass: 0x%08X", static_cast<unsigned>(Offsets::LevelDataBypass));

    if (Offsets::LevelDataBypass > 0x10000) {
        IntCheck("LevelDataBypass offset resolved", true);

        auto& patch = Memory::GetLevelDataBypassPatch();
        IntReport("  Patch staged: %s", patch.staged ? "yes" : "no");
        IntCheck("Patch is staged", patch.staged);

        if (patch.staged) {
            patch.Enable();
            IntCheck("LevelDataBypass Enable (no crash)", true);
            patch.Disable();
            IntCheck("LevelDataBypass Disable (no crash)", true);
        }
    } else {
        IntSkip("LevelDataBypass", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
