#pragma once

// GWA3-039: Callback Registry + Module Ownership
//
// Centralized callback registration system matching GWCA's architecture:
// - Three callback families: UIMessage, FrameUIMessage, CreateUIComponent
// - Altitude-sorted dispatch (<=0 pre-processing, >0 post-processing)
// - Module ownership tracking via range table for bulk cleanup
// - Thread-safe registration/removal via mutex
//
// Record layout (matches GWCA's 0x30-byte CallbackRecord):
//   +0x00: int32_t altitude
//   +0x04: HookEntry* owner
//   +0x08: std::function<void(HookStatus*, uint32_t, void*, void*)> callback

#include <cstdint>
#include <functional>

namespace GWA3 {

// Hook status — callbacks can block further processing
struct HookStatus {
    bool blocked;
};

// Hook entry — opaque owner handle for callback registration/removal.
// Address is used as identity (not dereferenced for data).
struct HookEntry {
    void* _internal;
};

// Callback signature: (status, message_id, wparam, lparam)
using UICallback = std::function<void(HookStatus*, uint32_t, void*, void*)>;

namespace CallbackRegistry {

    bool Initialize();
    void Shutdown();

    // ===== UIMessage callbacks (global, per-message-id hash) =====
    bool RegisterUIMessageCallback(HookEntry* entry, uint32_t messageId,
                                   const UICallback& callback, int altitude = -0x8000);
    void RemoveUIMessageCallback(HookEntry* entry, uint32_t messageId);

    // ===== FrameUIMessage callbacks (per-frame-message hash) =====
    bool RegisterFrameUIMessageCallback(HookEntry* entry, uint32_t messageId,
                                        const UICallback& callback, int altitude = -0x8000);
    void RemoveFrameUIMessageCallback(HookEntry* entry, uint32_t messageId);

    // ===== CreateUIComponent callbacks (single global list) =====
    bool RegisterCreateUIComponentCallback(HookEntry* entry,
                                           const UICallback& callback, int altitude = -0x8000);
    void RemoveCreateUIComponentCallback(HookEntry* entry);

    // ===== Bulk cleanup =====
    // Remove ALL callbacks registered with this HookEntry across all families.
    void RemoveCallbacks(HookEntry* entry);

    // ===== Dispatch (called by UIMgr/game hooks) =====
    void DispatchUIMessage(uint32_t messageId, void* wparam, void* lparam);
    void DispatchFrameUIMessage(uint32_t messageId, void* wparam, void* lparam);
    void DispatchCreateUIComponent(uint32_t componentId, void* wparam, void* lparam);

} // namespace CallbackRegistry

} // namespace GWA3
