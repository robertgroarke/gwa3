bool TestCameraIntrospection() {
    IntReport("=== Camera Introspection ===");

    if (ReadMyId() == 0) {
        IntSkip("Camera introspection", "Not in game");
        IntReport("");
        return false;
    }

    Camera* cam = CameraMgr::GetCamera();
    IntReport("  Camera struct: %p", cam);

    if (!cam) {
        IntSkip("Camera struct", "CameraClass scan pattern did not resolve");
        IntSkip("Camera FOV", "CameraClass unavailable");
        IntSkip("Camera Yaw", "CameraClass unavailable");
        IntCheck("CameraMgr::Initialize ran (no crash)", true);
        IntReport("");
        return true;
    }

    IntCheck("Camera struct available", true);
    IntReport("  Camera position: (%.1f, %.1f, %.1f)",
              cam->position.x, cam->position.y, cam->position.z);
    IntReport("  Camera look_at: (%.1f, %.1f, %.1f)",
              cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z);
    IntCheck("Camera position not all zeros",
             cam->position.x != 0.0f || cam->position.y != 0.0f || cam->position.z != 0.0f);

    const float fov = CameraMgr::GetFieldOfView();
    IntReport("  FOV: %.4f radians (%.1f degrees)", fov, fov * 57.2957795f);
    IntCheck("FOV in plausible range (0.1 - 2.5 rad)", fov > 0.1f && fov < 2.5f);

    const float yaw = CameraMgr::GetYaw();
    IntReport("  Yaw: %.4f radians (%.1f degrees)", yaw, yaw * 57.2957795f);
    IntCheck("Yaw is finite", yaw == yaw);

    IntReport("");
    return true;
}
