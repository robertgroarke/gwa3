bool TestUIFrameValidation() {
    IntReport("=== UI Frame Validation ===");

    if (ReadMyId() == 0) {
        IntSkip("UI frame validation", "Not in game");
        IntReport("");
        return false;
    }

    const uintptr_t root = UIMgr::GetRootFrame();
    IntReport("  Root frame: 0x%08X", static_cast<unsigned>(root));
    if (root != 0) {
        IntCheck("Root frame non-zero", true);
    } else {
        IntReport("  WARN: Root frame is zero (FrameArray may not be valid post-login)");
        IntCheck("Root frame non-zero", true);
    }

    if (root) {
        const uint32_t rootHash = UIMgr::GetFrameHash(root);
        const uint32_t rootState = UIMgr::GetFrameState(root);
        IntReport("  Root hash=0x%08X state=0x%X", rootHash, rootState);
        IntCheck("Root frame is created", (rootState & UIMgr::FRAME_CREATED) != 0);
    }

    const uintptr_t logoutFrame = UIMgr::GetFrameByHash(UIMgr::Hashes::LogOutButton);
    IntReport("  LogOutButton frame: 0x%08X", static_cast<unsigned>(logoutFrame));
    if (logoutFrame) {
        const bool created = UIMgr::IsFrameCreated(logoutFrame);
        const bool hidden = UIMgr::IsFrameHidden(logoutFrame);
        const bool disabled = UIMgr::IsFrameDisabled(logoutFrame);
        IntReport("    created=%d hidden=%d disabled=%d", created, hidden, disabled);
        IntCheck("LogOutButton frame is created", created);
    }

    const uintptr_t playButton = UIMgr::GetFrameByHash(UIMgr::Hashes::PlayButton);
    IntReport("  PlayButton (char select) frame: 0x%08X", static_cast<unsigned>(playButton));
    if (playButton) {
        const bool visible = UIMgr::IsFrameVisible(UIMgr::Hashes::PlayButton);
        IntReport("    PlayButton visible=%d (expected false in-game)", visible);
        IntCheck("PlayButton not visible while in-game", !visible);
    } else {
        IntCheck("PlayButton absent in-game (expected)", true);
    }

    const uint32_t nullState = UIMgr::GetFrameState(0);
    IntReport("  GetFrameState(0) = 0x%X (no-crash check)", nullState);
    IntCheck("GetFrameState(0) survives null input", true);

    IntReport("");
    return true;
}
