bool TestRenderingToggle() {
    IntReport("=== Rendering Toggle ===");

    if (ReadMyId() == 0) {
        IntSkip("Rendering toggle", "Not in game");
        IntReport("");
        return false;
    }

    IntReport("  Disabling rendering...");
    ChatMgr::SetRenderingEnabled(false);
    Sleep(500);
    IntCheck("SetRenderingEnabled(false) no crash", true);

    IntReport("  Re-enabling rendering...");
    ChatMgr::SetRenderingEnabled(true);
    Sleep(500);
    IntCheck("SetRenderingEnabled(true) no crash", true);

    IntReport("");
    return true;
}
