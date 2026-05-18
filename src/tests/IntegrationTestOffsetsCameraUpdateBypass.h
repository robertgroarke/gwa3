bool TestCameraUpdateBypassPatch() {
    IntReport("=== Camera Update Bypass Patch ===");

    IntReport("  CameraUpdateBypass: 0x%08X", static_cast<unsigned>(Offsets::CameraUpdateBypass));

    if (Offsets::CameraUpdateBypass > 0x10000) {
        IntCheck("CameraUpdateBypass offset resolved", true);

        auto& patch = Memory::GetCameraUnlockPatch();
        IntReport("  Patch staged: %s", patch.staged ? "yes" : "no");
        IntCheck("Patch is staged", patch.staged);

        if (patch.staged) {
            patch.Enable();
            IntCheck("CameraUpdateBypass Enable (no crash)", true);
            patch.Disable();
            IntCheck("CameraUpdateBypass Disable (no crash)", true);
        }
    } else {
        IntSkip("CameraUpdateBypass", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}
