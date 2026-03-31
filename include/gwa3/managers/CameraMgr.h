#pragma once

// GWA3-052: CameraMgr — camera control, FOV, unlock, fog toggle.

#include <gwa3/game/Camera.h>
#include <cstdint>

namespace GWA3::CameraMgr {

    bool Initialize();

    // Get camera struct pointer (null if offset not resolved)
    Camera* GetCamera();

    // Set max zoom distance (default GW is ~750, common unlock is 900)
    bool SetMaxDist(float dist = 900.0f);

    // Adjust field of view (radians)
    bool SetFieldOfView(float fov);

    // Get current field of view
    float GetFieldOfView();

    // Get current yaw (radians, east origin)
    float GetYaw();

    // Free camera movement
    bool UnlockCam(bool flag);
    bool GetCameraUnlock();

    // Toggle fog rendering
    bool SetFog(bool flag);

} // namespace GWA3::CameraMgr
