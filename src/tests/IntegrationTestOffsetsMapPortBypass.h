bool TestMapPortBypassPatch() {
    IntReport("=== Map/Port Bypass Patch ===");

    IntReport("  MapPortBypass: 0x%08X", static_cast<unsigned>(Offsets::MapPortBypass));

    if (Offsets::MapPortBypass > 0x10000) {
        IntCheck("MapPortBypass offset resolved", true);

        auto& patch = Memory::GetMapPortBypassPatch();
        IntReport("  Patch staged: %s", patch.staged ? "yes" : "no");
        IntCheck("Patch is staged", patch.staged);

        if (patch.staged) {
            patch.Enable();
            IntCheck("MapPortBypass Enable (no crash)", true);
            patch.Disable();
            IntCheck("MapPortBypass Disable (no crash)", true);
        }
    } else {
        IntSkip("MapPortBypass", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
