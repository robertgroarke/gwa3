#include <gwa3/managers/UIMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <algorithm>
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
    uintptr_t* buffer;    // +0x00
    uint32_t capacity;    // +0x04 — GW::Array m_capacity
    uint32_t size;        // +0x08 — GW::Array m_size (actual count of entries)
    uint32_t param;       // +0x0C — GW::Array m_param
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
        Log::Info("UIMgr: FrameArray at 0x%08X: buffer=0x%08X capacity=%u size=%u",
                  reinterpret_cast<uintptr_t>(arr),
                  reinterpret_cast<uintptr_t>(arr->buffer),
                  arr->capacity, arr->size);
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

uintptr_t GetFrameByContextAndChildOffset(uintptr_t context, uint32_t childOffsetId, uintptr_t excludeFrame) {
    if (context < 0x10000) return 0;
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;
    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            uintptr_t frame = arr->buffer[i];
            if (frame < 0x10000 || frame == excludeFrame) continue;
            if (GetFrameContext(frame) != context) continue;
            if (GetChildOffsetId(frame) != childOffsetId) continue;
            return frame;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

uintptr_t GetVisibleFrameByChildOffset(uint32_t childOffsetId, uintptr_t excludeFrame, uintptr_t excludeContext) {
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;
    uintptr_t bestFrame = 0;
    uint32_t bestChildCount = 0;
    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            uintptr_t frame = arr->buffer[i];
            if (frame < 0x10000 || frame == excludeFrame) continue;
            if (excludeContext && GetFrameContext(frame) == excludeContext) continue;
            if (GetChildOffsetId(frame) != childOffsetId) continue;
            const uint32_t state = GetFrameState(frame);
            if (!(state & FRAME_CREATED) || (state & FRAME_HIDDEN)) continue;
            const uint32_t childCount = GetChildFrameCount(frame);
            if (!bestFrame || childCount > bestChildCount) {
                bestFrame = frame;
                bestChildCount = childCount;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return bestFrame;
}

uintptr_t GetVisibleFrameByChildOffsetAndChildCount(
    uint32_t childOffsetId,
    uint32_t minChildCount,
    uint32_t maxChildCount,
    uintptr_t excludeFrame,
    uintptr_t excludeContext) {
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;
    uintptr_t bestFrame = 0;
    uint32_t bestChildCount = 0xFFFFFFFFu;
    uint32_t bestFrameId = 0;
    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            uintptr_t frame = arr->buffer[i];
            if (frame < 0x10000 || frame == excludeFrame) continue;
            if (excludeContext && GetFrameContext(frame) == excludeContext) continue;
            if (GetChildOffsetId(frame) != childOffsetId) continue;
            const uint32_t state = GetFrameState(frame);
            if (!(state & FRAME_CREATED) || (state & FRAME_HIDDEN)) continue;
            const uint32_t childCount = GetChildFrameCount(frame);
            if (childCount < minChildCount || childCount > maxChildCount) continue;
            const uint32_t frameId = GetFrameId(frame);
            if (!bestFrame
                || childCount < bestChildCount
                || (childCount == bestChildCount && frameId > bestFrameId)) {
                bestFrame = frame;
                bestChildCount = childCount;
                bestFrameId = frameId;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return bestFrame;
}

void DebugDumpVisibleFramesByChildOffset(uint32_t childOffsetId, const char* label, uint32_t maxCount) {
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) {
        Log::Warn("UIMgr: DebugDumpVisibleFramesByChildOffset no FrameArray label=%s", label ? label : "");
        return;
    }
    uint32_t dumped = 0;
    Log::Info("UIMgr: Visible-frame dump begin label=%s childOffset=%u", label ? label : "", childOffsetId);
    __try {
        for (uint32_t i = 0; i < arr->size && dumped < maxCount; ++i) {
            uintptr_t frame = arr->buffer[i];
            if (frame < 0x10000) continue;
            if (GetChildOffsetId(frame) != childOffsetId) continue;
            const uint32_t state = GetFrameState(frame);
            if (!(state & FRAME_CREATED) || (state & FRAME_HIDDEN)) continue;
            Log::Info("UIMgr:   visible frame=0x%08X hash=%u state=0x%X frameId=%u childOffset=%u context=0x%08X childCount=%u",
                      static_cast<unsigned>(frame),
                      GetFrameHash(frame),
                      state,
                      GetFrameId(frame),
                      GetChildOffsetId(frame),
                      static_cast<unsigned>(GetFrameContext(frame)),
                      GetChildFrameCount(frame));
            ++dumped;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: DebugDumpVisibleFramesByChildOffset faulted label=%s", label ? label : "");
    }
    Log::Info("UIMgr: Visible-frame dump end label=%s dumped=%u", label ? label : "", dumped);
}

uint32_t GetChildFrameCount(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    const uintptr_t parentRelation = frame + 0x128;

    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;

    uint32_t count = 0;
    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            const uintptr_t candidate = arr->buffer[i];
            if (candidate < 0x10000 || candidate == frame) continue;
            const uintptr_t candidateParentRelation = *reinterpret_cast<uintptr_t*>(candidate + 0x128);
            if (candidateParentRelation != parentRelation) continue;
            ++count;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return count;
}

uintptr_t GetChildFrameByIndex(uintptr_t frame, uint32_t index) {
    if (frame < 0x10000) return 0;
    const uintptr_t parentRelation = frame + 0x128;

    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;

    uintptr_t children[256] = {};
    uint32_t count = 0;
    __try {
        for (uint32_t i = 0; i < arr->size && count < _countof(children); ++i) {
            const uintptr_t candidate = arr->buffer[i];
            if (candidate < 0x10000 || candidate == frame) continue;
            const uintptr_t candidateParentRelation = *reinterpret_cast<uintptr_t*>(candidate + 0x128);
            if (candidateParentRelation != parentRelation) continue;
            children[count++] = candidate;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    std::sort(children, children + count, [](uintptr_t lhs, uintptr_t rhs) {
        const uint32_t lhsChild = GetChildOffsetId(lhs);
        const uint32_t rhsChild = GetChildOffsetId(rhs);
        if (lhsChild != rhsChild) return lhsChild < rhsChild;
        return lhs < rhs;
    });

    if (index >= count) return 0;
    return children[index];
}

uintptr_t GetChildFrameByOffset(uintptr_t frame, uint32_t childOffsetId) {
    if (frame < 0x10000) return 0;
    const uintptr_t parentRelation = frame + 0x128;

    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;

    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            const uintptr_t candidate = arr->buffer[i];
            if (candidate < 0x10000 || candidate == frame) continue;
            const uintptr_t candidateParentRelation = *reinterpret_cast<uintptr_t*>(candidate + 0x128);
            if (candidateParentRelation != parentRelation) continue;
            if (GetChildOffsetId(candidate) != childOffsetId) continue;
            return candidate;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

uintptr_t NavigateSortedChildPath(uintptr_t frame, const uint32_t* childIndices, uint32_t childIndexCount) {
    if (frame < 0x10000 || !childIndices) return 0;

    uintptr_t current = frame;
    for (uint32_t i = 0; i < childIndexCount; ++i) {
        current = GetChildFrameByIndex(current, childIndices[i]);
        if (current < 0x10000) return 0;
    }
    return current;
}

void DebugDumpChildFrames(uintptr_t frame, const char* label, uint32_t maxCount) {
    if (frame < 0x10000) {
        Log::Warn("UIMgr: DebugDumpChildFrames invalid frame=0x%08X label=%s",
                  static_cast<unsigned>(frame), label ? label : "");
        return;
    }

    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) {
        Log::Warn("UIMgr: DebugDumpChildFrames no FrameArray for label=%s", label ? label : "");
        return;
    }

    const uintptr_t parentRelation = frame + 0x128;
    uintptr_t children[256] = {};
    uint32_t count = 0;
    __try {
        for (uint32_t i = 0; i < arr->size && count < _countof(children); ++i) {
            const uintptr_t candidate = arr->buffer[i];
            if (candidate < 0x10000 || candidate == frame) continue;
            const uintptr_t candidateParentRelation = *reinterpret_cast<uintptr_t*>(candidate + 0x128);
            if (candidateParentRelation != parentRelation) continue;
            children[count++] = candidate;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: DebugDumpChildFrames fault collecting label=%s", label ? label : "");
        return;
    }

    std::sort(children, children + count, [](uintptr_t lhs, uintptr_t rhs) {
        const uint32_t lhsChild = GetChildOffsetId(lhs);
        const uint32_t rhsChild = GetChildOffsetId(rhs);
        if (lhsChild != rhsChild) return lhsChild < rhsChild;
        return lhs < rhs;
    });

    Log::Info("UIMgr: Child-frame dump begin label=%s frame=0x%08X childCount=%u",
              label ? label : "", static_cast<unsigned>(frame), count);
    __try {
        const uint32_t limit = (count < maxCount) ? count : maxCount;
        for (uint32_t i = 0; i < limit; ++i) {
            const uintptr_t child = children[i];
            Log::Info("UIMgr:   child[%u] frame=0x%08X hash=%u state=0x%X frameId=%u childOffset=%u context=0x%08X childCount=%u",
                      i,
                      static_cast<unsigned>(child),
                      GetFrameHash(child),
                      GetFrameState(child),
                      GetFrameId(child),
                      GetChildOffsetId(child),
                      static_cast<unsigned>(GetFrameContext(child)),
                      GetChildFrameCount(child));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: DebugDumpChildFrames fault dumping label=%s", label ? label : "");
    }
    Log::Info("UIMgr: Child-frame dump end label=%s", label ? label : "");
}

uintptr_t GetRootFrame() {
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;
    return arr->buffer[0];
}

uintptr_t GetFrameById(uint32_t frameId) {
    if (!frameId) return 0;
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) return 0;
    __try {
        for (uint32_t i = 0; i < arr->size; ++i) {
            const uintptr_t frame = arr->buffer[i];
            if (frame < 0x10000) continue;
            if (GetFrameId(frame) == frameId) return frame;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

uintptr_t GetParentFrame(uintptr_t frame) {
    if (frame < 0x10000) return 0;
    __try {
        uintptr_t relation = *reinterpret_cast<uintptr_t*>(frame + 0x128);
        if (relation < 0x10000) return 0;
        return relation - 0x128;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
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

void SendUIMessageAsm(uint32_t msgId, void* wParam, void* lParam) {
    if (!Offsets::UIMessage) return;
    uintptr_t fn = Offsets::UIMessage;
    __asm {
        push lParam
        push wParam
        push msgId
        call fn
        add esp, 0x0C
    }
}

struct MouseAction {
    uint32_t frame_id;
    uint32_t child_offset_id;
    uint32_t action_state;
    uint32_t wparam;
    uint32_t lparam;
};

// POD task for EnqueuePostRaw — no std::function, no heap
struct ClickTask {
    uintptr_t sendFn;
    uintptr_t thisPtr;
    MouseAction action;
};
static_assert(sizeof(ClickTask) <= 64, "ClickTask exceeds InlineTask storage");

static void ExecuteClickTask(void* storage) {
    auto* t = static_cast<ClickTask*>(storage);
    static MouseAction s_act;
    s_act = t->action;
    void* tp = reinterpret_cast<void*>(t->thisPtr);
    uintptr_t fn = t->sendFn;
    void* wParam = &s_act;
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }
}

bool ButtonClickImmediate(uintptr_t frame) {
    if (!s_sendFrameUIAddr || frame < 0x10000) {
        Log::Warn("UIMgr: ButtonClickImmediate no sendAddr or invalid frame");
        return false;
    }

    const uint32_t state = GetFrameState(frame);
    if (!(state & FRAME_CREATED)) {
        Log::Warn("UIMgr: ButtonClickImmediate frame 0x%08X not created (state=0x%X)", frame, state);
        return false;
    }

    uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClickImmediate invalid context for frame 0x%08X", frame);
        return false;
    }

    MouseAction action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.action_state = ACTION_MOUSE_UP;
    action.wparam = 0;
    action.lparam = 0;

    void* tp = reinterpret_cast<void*>(context + 0xA8);
    void* wParam = &action;
    uintptr_t fn = s_sendFrameUIAddr;
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }
    Log::Info("UIMgr: ButtonClickImmediate frame=0x%08X frameId=%u childOffset=%u context=0x%08X",
              static_cast<unsigned>(frame),
              action.frame_id,
              action.child_offset_id,
              static_cast<unsigned>(context));
    return true;
}

bool ButtonClickImmediateFull(uintptr_t frame) {
    if (!s_sendFrameUIAddr || frame < 0x10000) {
        Log::Warn("UIMgr: ButtonClickImmediateFull no sendAddr or invalid frame");
        return false;
    }

    const uint32_t state = GetFrameState(frame);
    if (!(state & FRAME_CREATED)) {
        Log::Warn("UIMgr: ButtonClickImmediateFull frame 0x%08X not created (state=0x%X)", frame, state);
        return false;
    }

    const uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClickImmediateFull invalid context for frame 0x%08X", frame);
        return false;
    }

    MouseAction action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.wparam = 0;
    action.lparam = 0;

    void* tp = reinterpret_cast<void*>(context + 0xA8);
    void* wParam = &action;
    uintptr_t fn = s_sendFrameUIAddr;

    action.action_state = ACTION_MOUSE_DOWN;
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }
    action.action_state = ACTION_MOUSE_UP;
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }

    Log::Info("UIMgr: ButtonClickImmediateFull frame=0x%08X frameId=%u childOffset=%u context=0x%08X",
              static_cast<unsigned>(frame),
              action.frame_id,
              action.child_offset_id,
              static_cast<unsigned>(context));
    return true;
}

bool ButtonClick(uintptr_t frame) {
    if (!s_sendFrameUIAddr || frame < 0x10000) {
        Log::Warn("UIMgr: ButtonClick no sendAddr or invalid frame");
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

    // Build the mouse action data
    MouseAction action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.action_state = ACTION_MOUSE_UP;
    action.wparam = 0;
    action.lparam = 0;

    void* thisPtr = reinterpret_cast<void*>(context + 0xA8);

    // Use the pre-dispatch game-thread queue for in-game UI clicks.
    // The post-dispatch path has been observed to stall on merchant/crafter
    // clicks during live consumable tests on BISCUIT.
    if (GameThread::IsInitialized()) {
        ClickTask ct;
        ct.sendFn = s_sendFrameUIAddr;
        ct.thisPtr = reinterpret_cast<uintptr_t>(thisPtr);
        ct.action = action;

        Log::Info("UIMgr: ButtonClick queueing pre-dispatch click frame=0x%08X frameId=%u childOffset=%u context=0x%08X",
                  static_cast<unsigned>(frame),
                  action.frame_id,
                  action.child_offset_id,
                  static_cast<unsigned>(context));
        GameThread::EnqueueRaw(ExecuteClickTask, &ct, sizeof(ct));
        return true;
    }

    // Fallback: old RenderHook shellcode path (only if GameThread unavailable)
    if (!RenderHook::IsInitialized() || !EnsureFrameClickShellcode()) {
        Log::Warn("UIMgr: ButtonClick — no dispatch mechanism available");
        return false;
    }

    const uint32_t thisPtrU32 = static_cast<uint32_t>(context + 0xA8);
    const uint32_t sendFrame = static_cast<uint32_t>(s_sendFrameUIAddr);
    auto* sc = reinterpret_cast<uint8_t*>(s_frameClickShellcode);

    sc[0] = 0xB9;
    WriteLE32(sc + 1, thisPtrU32);
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

    // Match the stable GWCA-style button click path: SendFrameUIMsg with a
    // single MouseUp action. The local FrameUI research notes that injecting
    // an extra MouseDown here can leave pre-game UI in a bad state.
    action.action_state = ACTION_MOUSE_UP;
    memcpy(reinterpret_cast<void*>(s_frameClickAction), &action, sizeof(action));
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

struct EditableTextCommitPacket {
    uint32_t frame_id;
    uint32_t child_offset_id;
    uint32_t action_or_state;
    const wchar_t* text_value;
    uint32_t unk0;
};

bool SetEditableTextValue(uintptr_t frame, const wchar_t* value, uintptr_t commitParentFrame) {
    if (!value || frame < 0x10000) {
        Log::Warn("UIMgr: SetEditableTextValue invalid args frame=0x%08X value=%p",
                  static_cast<unsigned>(frame), value);
        return false;
    }

    static wchar_t s_editValueBuffer[128] = {};
    wcsncpy_s(s_editValueBuffer, value, _TRUNCATE);

    const uintptr_t parentFrame = commitParentFrame > 0x10000 ? commitParentFrame : GetParentFrame(frame);
    if (parentFrame < 0x10000) {
        Log::Warn("UIMgr: SetEditableTextValue missing parent frame for 0x%08X", static_cast<unsigned>(frame));
        return false;
    }

    CallSendFrameUI(reinterpret_cast<void*>(frame + 0xA8), 0x5D, s_editValueBuffer, nullptr);
    const bool localOk = true;
    if (!localOk) {
        Log::Warn("UIMgr: SetEditableTextValue local setter failed for 0x%08X", static_cast<unsigned>(frame));
        return false;
    }

    EditableTextCommitPacket packet{};
    packet.frame_id = GetFrameId(frame);
    packet.child_offset_id = GetChildOffsetId(frame);
    packet.action_or_state = 7;
    packet.text_value = s_editValueBuffer;
    packet.unk0 = 0;

    CallSendFrameUI(reinterpret_cast<void*>(parentFrame + 0xA8), MSG_MOUSE_CLICK2, &packet, nullptr);
    const bool commitOk = true;

    Log::Info("UIMgr: SetEditableTextValue frame=0x%08X parent=0x%08X frameId=%u childOffset=%u text='%S' local=%u commit=%u",
              static_cast<unsigned>(frame),
              static_cast<unsigned>(parentFrame),
              packet.frame_id,
              packet.child_offset_id,
              s_editValueBuffer,
              localOk ? 1u : 0u,
              commitOk ? 1u : 0u);
    return localOk && commitOk;
}

bool SetEditableTextLocalOnly(uintptr_t frame, const wchar_t* value) {
    if (!value || frame < 0x10000) {
        Log::Warn("UIMgr: SetEditableTextLocalOnly invalid args frame=0x%08X value=%p",
                  static_cast<unsigned>(frame), value);
        return false;
    }

    static wchar_t s_editValueBuffer[128] = {};
    wcsncpy_s(s_editValueBuffer, value, _TRUNCATE);
    CallSendFrameUI(reinterpret_cast<void*>(frame + 0xA8), 0x5D, s_editValueBuffer, nullptr);
    Log::Info("UIMgr: SetEditableTextLocalOnly frame=0x%08X frameId=%u childOffset=%u text='%S'",
              static_cast<unsigned>(frame),
              GetFrameId(frame),
              GetChildOffsetId(frame),
              s_editValueBuffer);
    return true;
}

struct NumericCommitPacket {
    uint32_t field_0;
    uint32_t field_4;
    uint32_t action_or_state;
    uint32_t numeric_value;
    uint32_t unk0;
};

struct KeyActionPacket {
    uint32_t key;
    uint32_t unk0;
    uint32_t unk1;
};

static bool SendMouseActionPacket(uintptr_t thisBase, uint32_t msgid, uint32_t frameId, uint32_t childOffsetId, uint32_t currentState, uint32_t wparam, uint32_t lparam) {
    if (!s_sendFrameUIAddr || thisBase < 0x10000) {
        Log::Warn("UIMgr: SendMouseActionPacket invalid thisBase=0x%08X msg=0x%X", static_cast<unsigned>(thisBase), msgid);
        return false;
    }
    MouseAction packet{};
    packet.frame_id = frameId;
    packet.child_offset_id = childOffsetId;
    packet.action_state = currentState;
    packet.wparam = wparam;
    packet.lparam = lparam;
    CallSendFrameUI(reinterpret_cast<void*>(thisBase + 0xA8), msgid, &packet, nullptr);
    Log::Info("UIMgr: SendMouseActionPacket thisBase=0x%08X msg=0x%X frameId=%u childOffset=%u state=%u wparam=%u lparam=%u",
              static_cast<unsigned>(thisBase),
              msgid,
              frameId,
              childOffsetId,
              currentState,
              wparam,
              lparam);
    return true;
}

bool SetNumericFrameValue(uintptr_t frame, uint32_t value, uintptr_t commitParentFrame) {
    if (frame < 0x10000) {
        Log::Warn("UIMgr: SetNumericFrameValue invalid frame=0x%08X", static_cast<unsigned>(frame));
        return false;
    }

    const uintptr_t parentFrame = commitParentFrame > 0x10000 ? commitParentFrame : GetParentFrame(frame);
    if (parentFrame < 0x10000) {
        Log::Warn("UIMgr: SetNumericFrameValue missing parent frame for 0x%08X", static_cast<unsigned>(frame));
        return false;
    }

    uint32_t field0 = 0;
    uint32_t field4 = 0;
    __try {
        field0 = *reinterpret_cast<uint32_t*>(frame + 0xC0);
        field4 = *reinterpret_cast<uint32_t*>(frame + 0xBC);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: SetNumericFrameValue failed reading frame fields for 0x%08X", static_cast<unsigned>(frame));
        return false;
    }

    CallSendFrameUI(reinterpret_cast<void*>(frame + 0xA8), 0x56, reinterpret_cast<void*>(value), nullptr);

    NumericCommitPacket packet{};
    packet.field_0 = field0;
    packet.field_4 = field4;
    packet.action_or_state = 7;
    packet.numeric_value = value;
    packet.unk0 = 0;

    CallSendFrameUI(reinterpret_cast<void*>(parentFrame + 0xA8), MSG_MOUSE_CLICK2, &packet, nullptr);
    Log::Info("UIMgr: SetNumericFrameValue frame=0x%08X parent=0x%08X field0=%u field4=%u value=%u",
              static_cast<unsigned>(frame),
              static_cast<unsigned>(parentFrame),
              packet.field_0,
              packet.field_4,
              value);
    return true;
}

bool SetNumericFrameLocalOnly(uintptr_t frame, uint32_t value) {
    if (frame < 0x10000) {
        Log::Warn("UIMgr: SetNumericFrameLocalOnly invalid frame=0x%08X", static_cast<unsigned>(frame));
        return false;
    }
    CallSendFrameUI(reinterpret_cast<void*>(frame + 0xA8), 0x56, reinterpret_cast<void*>(value), nullptr);
    Log::Info("UIMgr: SetNumericFrameLocalOnly frame=0x%08X frameId=%u childOffset=%u value=%u",
              static_cast<unsigned>(frame),
              GetFrameId(frame),
              GetChildOffsetId(frame),
              value);
    return true;
}

bool TestMouseAction(uintptr_t frame, uint32_t currentState, uint32_t wparam, uint32_t lparam) {
    if (frame < 0x10000) {
        Log::Warn("UIMgr: TestMouseAction invalid frame=0x%08X", static_cast<unsigned>(frame));
        return false;
    }
    return SendMouseActionPacket(
        frame,
        0x32,
        GetFrameId(frame),
        GetChildOffsetId(frame),
        currentState,
        wparam,
        lparam);
}

bool TestMouseClickAction(uintptr_t frame, uint32_t currentState, uint32_t wparam, uint32_t lparam) {
    if (frame < 0x10000) {
        Log::Warn("UIMgr: TestMouseClickAction invalid frame=0x%08X", static_cast<unsigned>(frame));
        return false;
    }
    const uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: TestMouseClickAction invalid context for frame=0x%08X", static_cast<unsigned>(frame));
        return false;
    }
    return SendMouseActionPacket(
        context,
        MSG_MOUSE_CLICK2,
        GetFrameId(frame),
        GetChildOffsetId(frame),
        currentState,
        wparam,
        lparam);
}

static bool SendKeyAction(uintptr_t frame, uint32_t msgid, uint32_t key) {
    if (frame < 0x10000) {
        Log::Warn("UIMgr: SendKeyAction invalid frame=0x%08X msg=0x%X key=%u",
                  static_cast<unsigned>(frame), msgid, key);
        return false;
    }
    KeyActionPacket packet{};
    packet.key = key;
    CallSendFrameUI(reinterpret_cast<void*>(frame + 0xA8), msgid, &packet, nullptr);
    Log::Info("UIMgr: SendKeyAction frame=0x%08X frameId=%u childOffset=%u msg=0x%X key=%u",
              static_cast<unsigned>(frame),
              GetFrameId(frame),
              GetChildOffsetId(frame),
              msgid,
              key);
    return true;
}

bool KeyDown(uintptr_t frame, uint32_t key) {
    return SendKeyAction(frame, 0x20, key);
}

bool KeyUp(uintptr_t frame, uint32_t key) {
    return SendKeyAction(frame, 0x22, key);
}

bool KeyPress(uintptr_t frame, uint32_t key) {
    const bool downOk = KeyDown(frame, key);
    Sleep(30);
    const bool upOk = KeyUp(frame, key);
    return downOk && upOk;
}

void DebugDumpFramesForContext(uintptr_t context, const char* label, uint32_t maxCount) {
    if (context < 0x10000) {
        Log::Warn("UIMgr: DebugDumpFramesForContext invalid context=0x%08X label=%s",
                  static_cast<unsigned>(context), label ? label : "");
        return;
    }
    auto* arr = GetFrameArray();
    if (!arr || !arr->buffer || arr->size == 0) {
        Log::Warn("UIMgr: DebugDumpFramesForContext no FrameArray for label=%s", label ? label : "");
        return;
    }
    uint32_t dumped = 0;
    Log::Info("UIMgr: Frame context dump begin label=%s context=0x%08X",
              label ? label : "", static_cast<unsigned>(context));
    __try {
        for (uint32_t i = 0; i < arr->size && dumped < maxCount; ++i) {
            uintptr_t frame = arr->buffer[i];
            if (frame < 0x10000) continue;
            if (GetFrameContext(frame) != context) continue;
            Log::Info("UIMgr:   frame=0x%08X hash=%u state=0x%X frameId=%u childOffset=%u",
                      static_cast<unsigned>(frame),
                      GetFrameHash(frame),
                      GetFrameState(frame),
                      GetFrameId(frame),
                      GetChildOffsetId(frame));
            ++dumped;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: DebugDumpFramesForContext faulted label=%s", label ? label : "");
    }
    Log::Info("UIMgr: Frame context dump end label=%s dumped=%u", label ? label : "", dumped);
}

} // namespace GWA3::UIMgr
