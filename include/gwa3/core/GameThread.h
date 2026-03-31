#pragma once

#include <Windows.h>
#include <cstdint>
#include <functional>

namespace GWA3::GameThread {

    // Opaque handle for persistent callback registration.
    struct HookEntry {
        int altitude;
    };

    using Callback = std::function<void()>;

    // Initialize: scan for FrApi.cpp render callback, install MinHook detour.
    // Must be called after Scanner::Initialize() and Offsets::ResolveAll().
    bool Initialize();

    // Remove hook, destroy critical section, clear all queued work.
    void Shutdown();

    // Enqueue a one-shot task. Thread-safe.
    // If already on game thread, executes immediately (fast path).
    void Enqueue(Callback task);

    // Register a persistent per-frame callback, altitude-sorted (higher = earlier).
    // Default altitude = 0x4000. Unique by HookEntry* — re-registering replaces.
    void RegisterCallback(HookEntry* entry, Callback cb, int altitude = 0x4000);

    // Remove a persistent callback by its HookEntry handle.
    void RemoveCallback(HookEntry* entry);

    // Returns true when called from within the hooked game-thread dispatcher.
    bool IsOnGameThread();

    // Returns true if the hook is installed and active.
    bool IsInitialized();

} // namespace GWA3::GameThread
