#include <gwa3/managers/UIMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::UIMgr {

static uintptr_t s_sendFrameUIAddr = 0;
using SendUIMessageFn = void(__cdecl*)(uint32_t msgid, void* wParam, void* lParam);

static SendUIMessageFn s_sendUIMessageFn = nullptr;
static bool s_initialized = false;
static uintptr_t s_frameClickShellcode = 0;
static uintptr_t s_frameClickAction = 0;

static void CallSendFrameUI(void* thisPtr, uint32_t msgid, void* wParam, void* lParam) {
    uintptr_t fn = s_sendFrameUIAddr;
    __asm {
        push lParam
        push wParam
        push msgid
        mov ecx, thisPtr
        call fn
    }
}

static void WriteLE32(uint8_t* dst, uint32_t value) {
    memcpy(dst, &value, sizeof(value));
}

static bool EnsureFrameClickShellcode() {
    if (s_frameClickShellcode && s_frameClickAction) return true;

    void* mem = VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("UIMgr: VirtualAlloc for frame click shellcode failed");
        return false;
    }

    s_frameClickShellcode = reinterpret_cast<uintptr_t>(mem);
    s_frameClickAction = s_frameClickShellcode + 32;
    return true;
}

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::SendFrameUIMsg) {
        s_sendFrameUIAddr = Offsets::SendFrameUIMsg;
    }
    if (Offsets::UIMessage) {
        s_sendUIMessageFn = reinterpret_cast<SendUIMessageFn>(Offsets::UIMessage);
    }

    s_initialized = true;
    Log::Info("UIMgr: Initialized (SendFrameUIMsg=0x%08X, UIMessage=0x%08X)",
              Offsets::SendFrameUIMsg, Offsets::UIMessage);
    return true;
}

struct FrameArrayData {
    uintptr_t* buffer;
    uint32_t size;
};

static FrameArrayData* GetFrameArray() {
    if (!Offsets::FrameArray) return nullptr;
    return reinterpret_cast<FrameArrayData*>(Offsets::FrameArray);
}

uintptr_t GetFrameByHash(uint32_t hash) {
    auto* arr = GetFrameArray();
    if (!arr) {
        Log::Warn("UIMgr: GetFrameByHash FrameArray is null");
        return 0;
    }

    static bool s_logged = false;
    if (!s_logged) {
        Log::Info("UIMgr: FrameArray at 0x%08X: buffer=0x%08X size=%u",
                  reinterpret_cast<uintptr_t>(arr),
                  reinterpret_cast<uintptr_t>(arr->buffer),
                  arr->size);
        s_logged = true;
    }

    static bool s_loggedScanAv = false;
    __try {
        if (!arr->buffer || arr->size == 0 || arr->size > 5000) return 0;

        for (uint32_t i = 0; i < arr->size; ++i) {
            uintptr_t frame = arr->buffer[i];
            if (frame < 0x10000) continue;

            uint32_t frameHash = *reinterpret_cast<uint32_t*>(frame + 0x134);
            if (frameHash == hash) return frame;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (!s_loggedScanAv) {
            Log::Warn("UIMgr: GetFrameByHash encountered volatile frame data during scan");
            s_loggedScanAv = true;
        }
        return 0;
    }
    return 0;
}

uintptr_t GetRootFrame() {
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;
    return arr->buffer[0];
}

uint32_t GetFrameId(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(frame + 0xBC);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t GetChildOffsetId(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(frame + 0xB8);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t GetFrameState(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(frame + 0x18C);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t GetFrameHash(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(frame + 0x134);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
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
    __try {
        uintptr_t relation = *reinterpret_cast<uintptr_t*>(frame + 0x128);
        if (relation < 0x10000) return 0;
        return relation - 0x128;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

void SendFrameUIMessage(uintptr_t frame, uint32_t msgId, void* wParam, void* lParam) {
    if (!s_sendFrameUIAddr || frame < 0x10000) return;

    uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) return;

    void* thisPtr = reinterpret_cast<void*>(context + 0xA8);
    CallSendFrameUI(thisPtr, msgId, wParam, lParam);
}

void SendUIMessage(uint32_t msgId, void* wParam, void* lParam) {
    if (!s_sendUIMessageFn) return;
    s_sendUIMessageFn(msgId, wParam, lParam);
}

struct MouseAction {
    uint32_t frame_id;
    uint32_t child_offset_id;
    uint32_t action_state;
    uint32_t wparam;
    uint32_t lparam;
};

bool ButtonClick(uintptr_t frame) {
    if (!s_sendFrameUIAddr || frame < 0x10000) {
        Log::Warn("UIMgr: ButtonClick no sendAddr or invalid frame");
        return false;
    }
    if (!RenderHook::IsInitialized()) {
        Log::Warn("UIMgr: ButtonClick RenderHook not initialized");
        return false;
    }
    if (!EnsureFrameClickShellcode()) {
        return false;
    }

    uint32_t state = GetFrameState(frame);
    if (!(state & FRAME_CREATED)) {
        Log::Warn("UIMgr: ButtonClick frame 0x%08X not created (state=0x%X)", frame, state);
        return false;
    }

    uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClick invalid context for frame 0x%08X", frame);
        return false;
    }

    MouseAction action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.action_state = ACTION_MOUSE_UP;
    action.wparam = 0;
    action.lparam = 0;
    memcpy(reinterpret_cast<void*>(s_frameClickAction), &action, sizeof(action));

    const uint32_t thisPtr = static_cast<uint32_t>(context + 0xA8);
    const uint32_t sendFrame = static_cast<uint32_t>(s_sendFrameUIAddr);
    auto* sc = reinterpret_cast<uint8_t*>(s_frameClickShellcode);

    sc[0] = 0xB9;
    WriteLE32(sc + 1, thisPtr);
    sc[5] = 0x6A;
    sc[6] = 0x00;
    sc[7] = 0x68;
    WriteLE32(sc + 8, static_cast<uint32_t>(s_frameClickAction));
    sc[12] = 0x6A;
    sc[13] = static_cast<uint8_t>(MSG_MOUSE_CLICK2);
    sc[14] = 0xE8;
    int32_t rel = static_cast<int32_t>(sendFrame - (static_cast<uint32_t>(s_frameClickShellcode) + 19));
    memcpy(sc + 15, &rel, sizeof(rel));
    sc[19] = 0xC3;

    FlushInstructionCache(GetCurrentProcess(), sc, 20);
    return RenderHook::EnqueueCommand(s_frameClickShellcode);
}

bool ButtonClickByHash(uint32_t hash) {
    uintptr_t frame = GetFrameByHash(hash);
    if (!frame) {
        Log::Warn("UIMgr: ButtonClickByHash hash %u not found", hash);
        return false;
    }
    return ButtonClick(frame);
}

} // namespace GWA3::UIMgr
