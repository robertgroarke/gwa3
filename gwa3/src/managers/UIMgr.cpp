#include <gwa3/managers/UIMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <cstring>

namespace GWA3::UIMgr {

// SendFrameUIMsg: __thiscall (ECX = frame+0xA8, msgid, wParam, lParam)
using SendFrameUIFn = void(__thiscall*)(void* ecx, uint32_t msgid, void* wParam, void* lParam);
// SendUIMessage: global dispatcher
using SendUIMessageFn = void(__cdecl*)(uint32_t msgid, void* wParam, void* lParam);

static SendFrameUIFn s_sendFrameUIFn = nullptr;
static SendUIMessageFn s_sendUIMessageFn = nullptr;
static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::SendFrameUIMsg) {
        s_sendFrameUIFn = reinterpret_cast<SendFrameUIFn>(Offsets::SendFrameUIMsg);
    }
    if (Offsets::UIMessage) {
        s_sendUIMessageFn = reinterpret_cast<SendUIMessageFn>(Offsets::UIMessage);
    }

    s_initialized = true;
    Log::Info("UIMgr: Initialized (SendFrameUIMsg=0x%08X, UIMessage=0x%08X)",
              Offsets::SendFrameUIMsg, Offsets::UIMessage);
    return true;
}

// --- Frame Array Access ---

struct FrameArrayData {
    uintptr_t* buffer;
    uint32_t   size;
};

static FrameArrayData* GetFrameArray() {
    if (!Offsets::FrameArray) return nullptr;
    return reinterpret_cast<FrameArrayData*>(Offsets::FrameArray);
}

uintptr_t GetFrameByHash(uint32_t hash) {
    auto* arr = GetFrameArray();
    if (!arr) {
        Log::Warn("UIMgr: GetFrameByHash — FrameArray is null");
        return 0;
    }

    // Log the array state for debugging
    static bool s_logged = false;
    if (!s_logged) {
        Log::Info("UIMgr: FrameArray at 0x%08X: buffer=0x%08X size=%u",
                  (uintptr_t)arr, (uintptr_t)arr->buffer, arr->size);
        s_logged = true;
    }

    if (!arr->buffer || arr->size == 0 || arr->size > 5000) return 0;

    for (uint32_t i = 0; i < arr->size; i++) {
        uintptr_t fp = arr->buffer[i];
        if (fp < 0x10000) continue;

        uint32_t frameHash = *reinterpret_cast<uint32_t*>(fp + 0x134);
        if (frameHash == hash) return fp;
    }
    return 0;
}

uintptr_t GetRootFrame() {
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;
    return arr->buffer[0];
}

// --- Frame Accessors ---

uint32_t GetFrameId(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(frame + 0xBC);
}

uint32_t GetChildOffsetId(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(frame + 0xB8);
}

uint32_t GetFrameState(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(frame + 0x18C);
}

uint32_t GetFrameHash(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(frame + 0x134);
}

bool IsFrameCreated(uintptr_t frame) {
    return (GetFrameState(frame) & FRAME_CREATED) != 0;
}

bool IsFrameHidden(uintptr_t frame) {
    return (GetFrameState(frame) & FRAME_HIDDEN) != 0;
}

bool IsFrameDisabled(uintptr_t frame) {
    return (GetFrameState(frame) & FRAME_DISABLED) != 0;
}

bool IsFrameVisible(uint32_t hash) {
    uintptr_t frame = GetFrameByHash(hash);
    if (!frame) return false;
    uint32_t state = GetFrameState(frame);
    return (state & FRAME_CREATED) && !(state & FRAME_HIDDEN);
}

uintptr_t GetFrameContext(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    uintptr_t relation = *reinterpret_cast<uintptr_t*>(frame + 0x128);
    if (relation < 0x10000) return 0;
    return relation - 0x128; // parent Frame* = FrameRelation* - 0x128
}

// --- Message Sending ---

void SendFrameUIMessage(uintptr_t frame, uint32_t msgId, void* wParam, void* lParam) {
    if (!s_sendFrameUIFn || frame < 0x10000) return;

    uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) return;

    void* ecx = reinterpret_cast<void*>(context + 0xA8);
    s_sendFrameUIFn(ecx, msgId, wParam, lParam);
}

void SendUIMessage(uint32_t msgId, void* wParam, void* lParam) {
    if (!s_sendUIMessageFn) return;
    s_sendUIMessageFn(msgId, wParam, lParam);
}

// --- Button Click (GWA3-021) ---

// kMouseAction struct: 5 dwords = 20 bytes
struct MouseAction {
    uint32_t frame_id;
    uint32_t child_offset_id;
    uint32_t action_state;
    uint32_t wparam;
    uint32_t lparam;
};

bool ButtonClick(uintptr_t frame) {
    if (!s_sendFrameUIFn || frame < 0x10000) return false;

    // Validate frame is created
    uint32_t state = GetFrameState(frame);
    if (!(state & FRAME_CREATED)) {
        Log::Warn("UIMgr: ButtonClick — frame 0x%08X not created (state=0x%X)", frame, state);
        return false;
    }

    uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClick — invalid context for frame 0x%08X", frame);
        return false;
    }

    // Build MouseAction: single MouseUp only (CRITICAL: not MouseDown+MouseUp)
    MouseAction action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.action_state = ACTION_MOUSE_UP; // 0x7
    action.wparam = 0;
    action.lparam = 0;

    void* ecx = reinterpret_cast<void*>(context + 0xA8);

    // Must execute on game thread
    if (GameThread::IsOnGameThread()) {
        s_sendFrameUIFn(ecx, MSG_MOUSE_CLICK2, &action, nullptr);
    } else {
        // Capture by value — action is small, ecx is a pointer
        uintptr_t capturedEcx = reinterpret_cast<uintptr_t>(ecx);
        SendFrameUIFn fn = s_sendFrameUIFn;
        GameThread::Enqueue([fn, capturedEcx, action]() mutable {
            fn(reinterpret_cast<void*>(capturedEcx), MSG_MOUSE_CLICK2, &action, nullptr);
        });
    }

    return true;
}

bool ButtonClickByHash(uint32_t hash) {
    uintptr_t frame = GetFrameByHash(hash);
    if (!frame) {
        Log::Warn("UIMgr: ButtonClickByHash — hash %u not found", hash);
        return false;
    }
    return ButtonClick(frame);
}

} // namespace GWA3::UIMgr
