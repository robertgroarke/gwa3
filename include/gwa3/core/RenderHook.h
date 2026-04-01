#pragma once

#include <functional>

namespace GWA3::RenderHook {

    // Install a MinHook detour on the rendering callback.
    // Queued functions execute each frame from the render context.
    bool Initialize();
    void Shutdown();

    // Enqueue a function to run on the render thread (next frame).
    void Enqueue(std::function<void()> task);

    bool IsInitialized();

} // namespace GWA3::RenderHook
