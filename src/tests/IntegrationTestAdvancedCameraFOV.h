bool TestCameraFOV() {
    IntReport("=== Camera FOV ===");

    Camera* cam = CameraMgr::GetCamera();
    if (!cam) { IntSkip("CameraFOV", "Camera not resolved"); IntReport(""); return false; }

    float origFov = CameraMgr::GetFieldOfView();
    IntReport("  Original FOV: %.4f", origFov);

    CameraMgr::SetFieldOfView(1.5f);
    float newFov = CameraMgr::GetFieldOfView();
    IntReport("  After SetFieldOfView(1.5): %.4f", newFov);
    float diff = (newFov > 1.5f) ? (newFov - 1.5f) : (1.5f - newFov);
    IntCheck("FOV changed to ~1.5", diff < 0.01f);

    // Restore
    CameraMgr::SetFieldOfView(origFov);
    IntReport("  Restored FOV: %.4f", CameraMgr::GetFieldOfView());

    IntReport("");
    return true;
}

// ===== Memory Personal Dir =====
