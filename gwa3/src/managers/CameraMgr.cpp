#include <gwa3/managers/CameraMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

namespace GWA3::CameraMgr {

static bool s_initialized = false;
static uintptr_t s_cameraPtr = 0;
static bool s_fogDisabled = false;

// Camera pointer resolved via GWCA scan pattern: D9 EE B9 ?? ?? ?? ?? D9 55 FC
// Post-processed in Offsets to dereference the embedded pointer.
static Camera* ResolveCameraPtr() {
    if (Offsets::CameraClass <= 0x10000) return nullptr;
    return reinterpret_cast<Camera*>(Offsets::CameraClass);
}

bool Initialize() {
    if (s_initialized) return true;

    Camera* cam = ResolveCameraPtr();
    if (cam) {
        s_cameraPtr = reinterpret_cast<uintptr_t>(cam);
    }

    s_initialized = true;
    Log::Info("CameraMgr: Initialized (camera=0x%08X)", s_cameraPtr);
    return true;
}

Camera* GetCamera() {
    if (!s_cameraPtr) return nullptr;
    return reinterpret_cast<Camera*>(s_cameraPtr);
}

bool SetMaxDist(float dist) {
    Camera* cam = GetCamera();
    if (!cam) return false;
    cam->max_distance = dist;
    cam->max_distance2 = dist;
    return true;
}

bool SetFieldOfView(float fov) {
    Camera* cam = GetCamera();
    if (!cam) return false;
    cam->field_of_view = fov;
    cam->field_of_view2 = fov;
    return true;
}

float GetFieldOfView() {
    Camera* cam = GetCamera();
    if (!cam) return 0.0f;
    return cam->field_of_view;
}

float GetYaw() {
    Camera* cam = GetCamera();
    if (!cam) return 0.0f;
    return cam->yaw;
}

bool UnlockCam(bool flag) {
    Camera* cam = GetCamera();
    if (!cam) return false;
    cam->camera_mode = flag ? 3u : 0u;
    return true;
}

bool GetCameraUnlock() {
    Camera* cam = GetCamera();
    if (!cam) return false;
    return cam->camera_mode == 3;
}

bool SetFog(bool flag) {
    // TODO: Fog toggle requires patching a render instruction.
    // Needs a dedicated scan pattern and memory patch in Offsets.
    s_fogDisabled = !flag;
    Log::Warn("CameraMgr: SetFog not yet implemented (needs render patch)");
    return false;
}

} // namespace GWA3::CameraMgr
