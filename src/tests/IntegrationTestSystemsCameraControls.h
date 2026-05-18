bool TestCameraControls() {
    IntReport("=== Camera Controls ===");

    if (ReadMyId() == 0) {
        IntSkip("Camera controls", "Not in game");
        IntReport("");
        return false;
    }

    Camera* cam = CameraMgr::GetCamera();
    if (!cam) {
        IntSkip("Camera controls", "Camera struct not resolved");
        IntReport("");
        return false;
    }

    const float origMaxDist = cam->max_distance;
    IntReport("  Original max distance: %.1f", origMaxDist);

    const bool setDist = CameraMgr::SetMaxDist(900.0f);
    IntCheck("SetMaxDist(900) succeeded", setDist);
    if (setDist) {
        IntReport("  After SetMaxDist(900): %.1f", cam->max_distance);
        IntCheck("Max distance changed to 900", cam->max_distance == 900.0f);
    }

    CameraMgr::SetMaxDist(origMaxDist);
    IntReport("  Restored max distance: %.1f", cam->max_distance);

    const bool fogOff = CameraMgr::SetFog(false);
    IntReport("  SetFog(false): %s", fogOff ? "success" : "unavailable");
    if (fogOff) {
        IntCheck("Fog disabled without crash", true);
        Sleep(500);

        const bool fogOn = CameraMgr::SetFog(true);
        IntCheck("Fog re-enabled without crash", fogOn);
    } else {
        IntSkip("Fog toggle", "FogPatch offset not resolved");
    }

    const bool origUnlock = CameraMgr::GetCameraUnlock();
    IntReport("  Camera unlock state: %d", origUnlock);

    CameraMgr::UnlockCam(true);
    IntCheck("UnlockCam(true) no crash", true);
    const bool nowUnlocked = CameraMgr::GetCameraUnlock();
    IntReport("  After UnlockCam(true): %d", nowUnlocked);
    IntCheck("Camera is now unlocked", nowUnlocked);

    CameraMgr::UnlockCam(origUnlock);
    IntReport("  Restored camera unlock: %d", CameraMgr::GetCameraUnlock());

    IntReport("");
    return true;
}
