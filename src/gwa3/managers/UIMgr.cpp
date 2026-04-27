#include <gwa3/managers/UIMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>
#include <algorithm>
#include <cstring>

namespace GWA3::UIMgr {

// Forward declarations for functions used before their definition
uint32_t GetChildFrameCount(uintptr_t frame);
uint32_t GetFrameId(uintptr_t frame);
uint32_t GetChildOffsetId(uintptr_t frame);
struct FrameArrayData;

static uintptr_t s_sendFrameUIAddr = 0;
using SendUIMessageFn = void(__cdecl*)(uint32_t msgid, void* wParam, void* lParam);
using DoActionFn = void(__fastcall*)(void* ecx, void* edx, uint32_t msgid, void* arg1, void* arg2);

static SendUIMessageFn s_sendUIMessageFn = nullptr;
static DoActionFn s_doActionFn = nullptr;
static bool s_initialized = false;
static uintptr_t s_frameClickShellcode = 0;
static uintptr_t s_frameClickAction = 0;
static FrameArrayData* s_actionFrameCache = nullptr;
static bool s_loggedPerformUiActionEngineLane = false;
static constexpr uint32_t kPerformActionActivateFlag = 0x20u;
static constexpr uint32_t kPerformActionTypeDefault = 0u;

struct PerformActionCommand {
    uintptr_t fn;
    uint32_t action;
    uint32_t flag;
    uint32_t type;
    uintptr_t action_base;
};

__declspec(naked) void BotshubActionCommandStub() {
    __asm {
        mov ecx, dword ptr [eax+16]
        cmp dword ptr [eax+12], 0
        jnz action_type_2
action_type_1:
        mov ecx, dword ptr [ecx+0Ch]
        jmp action_common
action_type_2:
        mov ecx, dword ptr [ecx+4]
action_common:
        add ecx, 0A8h
        push 0
        lea eax, dword ptr [eax+4]
        push eax
        push dword ptr [eax+4]
        xor edx, edx
        call dword ptr [s_doActionFn]
        jmp GWA3BotshubCommandReturnThunk
    }
}

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
    if (Offsets::Action) {
        s_doActionFn = reinterpret_cast<DoActionFn>(Offsets::Action);
    }
    if (uintptr_t frameCacheAddr = Scanner::Find("\x68\x00\x10\x00\x00\x8B\x1C\x98\x8D", "xxxxxxxxx", -4)) {
        s_actionFrameCache = *reinterpret_cast<FrameArrayData**>(frameCacheAddr);
    }
    if (!s_actionFrameCache && Offsets::FrameArray) {
        s_actionFrameCache = reinterpret_cast<FrameArrayData*>(Offsets::FrameArray);
    }
    uintptr_t doActionAddr = 0;
    if (!s_doActionFn) {
        doActionAddr = Scanner::Find("\x83\xFE\x0B\x75\x14\x68\x77\x01\x00\x00", "xxxxxxxxxx", -0x1B);
    }
    if (!s_doActionFn && doActionAddr) {
        s_doActionFn = reinterpret_cast<DoActionFn>(doActionAddr);
    }

    s_initialized = true;
    Log::Info("UIMgr: Initialized (SendFrameUIMsg=0x%08X, UIMessage=0x%08X, DoAction=0x%08X, ActionFrameCache=0x%08X)",
              Offsets::SendFrameUIMsg, Offsets::UIMessage,
              reinterpret_cast<uintptr_t>(s_doActionFn),
              reinterpret_cast<uintptr_t>(s_actionFrameCache));
    return true;
}

struct FrameArrayData {
    uintptr_t* buffer;    // +0x00
    uint32_t capacity;    // +0x04 — GW::Array m_capacity
    uint32_t size;        // +0x08 — GW::Array m_size (actual count of entries)
    uint32_t param;       // +0x0C — GW::Array m_param
};

struct ControlActionPacket {
    uint32_t key = 0;
    uint32_t unk1 = 0x4000;
    uint32_t unk2 = 0;
};

struct FrameKeyActionPacket {
    uint32_t key = 0;
    uint32_t unk0 = 0;
    uint32_t unk1 = 0;
};

static FrameArrayData* GetFrameArray() {
    if (!Offsets::FrameArray) return nullptr;
    return reinterpret_cast<FrameArrayData*>(Offsets::FrameArray);
}

static uintptr_t GetActionFrame() {
    auto* arr = s_actionFrameCache ? s_actionFrameCache : GetFrameArray();
    if (!arr || !arr->buffer || arr->size <= 1) return 0;
    const uintptr_t frame = arr->buffer[1];
    if (frame < 0x10000) return 0;
    return frame;
}

static uintptr_t GetActionContext() {
    const uintptr_t frame = GetActionFrame();
    if (frame < 0x10000) return 0;
    return frame + 0xA0;
}

static uint32_t MapControlActionMessageToFrameMessage(uint32_t msgid) {
    switch (msgid) {
    case 0x1Eu:
        return 0x20u;
    case 0x20u:
        return 0x22u;
    default:
        return msgid;
    }
}

static bool SendControlAction(uint32_t msgid, ControlAction action) {
    uintptr_t frame = 0;
    __try {
        frame = GetActionFrame();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: SendControlAction failed reading action frame msg=0x%X action=0x%X",
                  msgid, static_cast<uint32_t>(action));
        frame = 0;
    }
    if (s_sendFrameUIAddr && frame >= 0x10000) {
        const uint32_t frameMsgId = MapControlActionMessageToFrameMessage(msgid);
        FrameKeyActionPacket packet{};
        packet.key = static_cast<uint32_t>(action);
        Log::Info("UIMgr: SendControlAction frame-dispatch begin frame=0x%08X frameId=%u childOffset=%u msg=0x%X action=0x%X",
                  static_cast<unsigned>(frame),
                  GetFrameId(frame),
                  GetChildOffsetId(frame),
                  frameMsgId,
                  static_cast<uint32_t>(action));
        __try {
            CallSendFrameUI(reinterpret_cast<void*>(frame + 0xA8), frameMsgId, &packet, nullptr);
            Log::Info("UIMgr: SendControlAction frame-dispatch end frame=0x%08X frameId=%u childOffset=%u msg=0x%X action=0x%X",
                      static_cast<unsigned>(frame),
                      GetFrameId(frame),
                      GetChildOffsetId(frame),
                      frameMsgId,
                      static_cast<uint32_t>(action));
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("UIMgr: SendControlAction frame-dispatch fault frame=0x%08X frameId=%u childOffset=%u msg=0x%X action=0x%X; attempting context fallback",
                      static_cast<unsigned>(frame),
                      GetFrameId(frame),
                      GetChildOffsetId(frame),
                      frameMsgId,
                      static_cast<uint32_t>(action));
        }
    }

    if (!s_doActionFn) return false;
    uintptr_t ctx = 0;
    __try {
        ctx = GetActionContext();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: SendControlAction failed reading action context msg=0x%X action=0x%X",
                  msgid, static_cast<uint32_t>(action));
        ctx = 0;
    }
    if (ctx < 0x10000) {
        Log::Warn("UIMgr: SendControlAction missing action context msg=0x%X action=0x%X",
                  msgid, static_cast<uint32_t>(action));
        return false;
    }
    ControlActionPacket packet{};
    packet.key = static_cast<uint32_t>(action);
    __try {
        s_doActionFn(reinterpret_cast<void*>(ctx), nullptr, msgid, &packet, nullptr);
        Log::Info("UIMgr: SendControlAction ctx=0x%08X msg=0x%X action=0x%X",
                  static_cast<unsigned>(ctx), msgid, static_cast<uint32_t>(action));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: SendControlAction context dispatch fault ctx=0x%08X msg=0x%X action=0x%X",
                  static_cast<unsigned>(ctx), msgid, static_cast<uint32_t>(action));
        return false;
    }
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

            uint32_t frameHash = 0u;
            bool readable = true;
            __try {
                frameHash = *reinterpret_cast<uint32_t*>(frame + 0x134);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                readable = false;
            }

            if (!readable) {
                if (!s_loggedScanAv) {
                    Log::Warn("UIMgr: GetFrameByHash encountered volatile frame data during scan");
                    s_loggedScanAv = true;
                }
                continue;
            }

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

// BotsHub-compatible context resolution for UI toggle actions:
//   ctx = *(ActionBase + 0xC) + 0xA8
// (type == 0 branch in BotsHub's CommandAction assembly). The type != 0
// branch uses +0x4 instead; we don't drive that path yet.
// Dump the first 0x40 bytes of ActionBase (as uintptr_t slots) so we can
// find which slot in THIS GW build holds the pointer BotsHub's ASM expects
// at +0xC. The fields themselves appear to have shifted between GW builds,
// so we look for the first slot whose value is a plausible heap pointer.
struct ActionBaseDump {
    uintptr_t slots[16];
};

static bool IsPlausibleUserPointer(uintptr_t ptr) {
    return ptr >= 0x10000 && ptr < 0x80000000;
}

static bool ReadSlotsAt(uintptr_t base, ActionBaseDump* out) {
    if (!out || !IsPlausibleUserPointer(base)) return false;
    __try {
        for (uint32_t i = 0; i < 16; ++i) {
            out->slots[i] = *reinterpret_cast<uintptr_t*>(base + i * 4);
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool HasUsableActionBaseSlots(uintptr_t base, ActionBaseDump* out) {
    ActionBaseDump local{};
    ActionBaseDump* dump = out ? out : &local;
    if (!ReadSlotsAt(base, dump)) return false;
    for (uint32_t i = 0; i < 16; ++i) {
        if (IsPlausibleUserPointer(dump->slots[i])) return true;
    }
    return false;
}

static bool SafeRead32(uintptr_t addr, uintptr_t* out) {
    *out = 0;
    if (addr < 0x10000) return false;
    __try {
        *out = *reinterpret_cast<uintptr_t*>(addr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Resolve BotsHub's shifted ActionBase candidate if it actually points at a
// readable slot table in this GW build. In this repo/build the -3 scan can
// sometimes land one byte into the embedded imm32 and produce garbage such as
// 0x8D0169C4, so treat the shifted scan as optional rather than authoritative.
static uintptr_t GetBotsHubActionBase() {
    static uintptr_t s_result = 0;
    static bool s_resolved = false;
    if (s_resolved) return s_result;
    s_resolved = true;

    uintptr_t scan = Scanner::Find("\x8D\x1C\x87\x89\x9D\xF4", "xxxxxx", -0x3);
    if (scan < 0x10000) {
        Log::Warn("UIMgr: BotsHub ActionBase scan failed");
        return 0;
    }
    uintptr_t ptr = 0;
    if (!SafeRead32(scan, &ptr) || ptr < 0x10000) {
        Log::Warn("UIMgr: BotsHub ActionBase deref failed at 0x%08X",
                  static_cast<unsigned>(scan));
        return 0;
    }
    if (!IsPlausibleUserPointer(ptr)) {
        Log::Warn("UIMgr: BotsHub ActionBase scan unusable scan=0x%08X ptr=0x%08X; falling back to Offsets::ActionBase=0x%08X",
                  static_cast<unsigned>(scan),
                  static_cast<unsigned>(ptr),
                  static_cast<unsigned>(Offsets::ActionBase));
        return 0;
    }
    ActionBaseDump dump{};
    if (!HasUsableActionBaseSlots(ptr, &dump)) {
        Log::Warn("UIMgr: BotsHub ActionBase scan unusable scan=0x%08X ptr=0x%08X; falling back to Offsets::ActionBase=0x%08X",
                  static_cast<unsigned>(scan),
                  static_cast<unsigned>(ptr),
                  static_cast<unsigned>(Offsets::ActionBase));
        return 0;
    }
    s_result = ptr;
    Log::Info("UIMgr: BotsHub ActionBase scan=0x%08X ptr=0x%08X (gwa3 Offsets::ActionBase=0x%08X)",
              static_cast<unsigned>(scan),
              static_cast<unsigned>(ptr),
              static_cast<unsigned>(Offsets::ActionBase));
    return ptr;
}

static uintptr_t GetPreferredActionBase(ActionBaseDump* out, bool* usedBotsHubScan, bool allowBotsHubScan = true) {
    if (usedBotsHubScan) *usedBotsHubScan = false;

    ActionBaseDump local{};
    ActionBaseDump* dump = out ? out : &local;

    uintptr_t botshubBase = 0;
    if (allowBotsHubScan) {
        botshubBase = GetBotsHubActionBase();
        if (HasUsableActionBaseSlots(botshubBase, dump)) {
            if (usedBotsHubScan) *usedBotsHubScan = true;
            return botshubBase;
        }
    }

    if (HasUsableActionBaseSlots(Offsets::ActionBase, dump)) {
        Log::Info("UIMgr: ActionBase fallback using Offsets::ActionBase=0x%08X",
                  static_cast<unsigned>(Offsets::ActionBase));
        return Offsets::ActionBase;
    }

    Log::Warn("UIMgr: no usable ActionBase (BotsHub=0x%08X, Offsets::ActionBase=0x%08X)",
              static_cast<unsigned>(botshubBase),
              static_cast<unsigned>(Offsets::ActionBase));
    return 0;
}

static bool ReadActionBaseDump(ActionBaseDump* out) {
    return ReadSlotsAt(Offsets::ActionBase, out);
}

static bool ReadBotsHubActionBaseDump(ActionBaseDump* out) {
    return ReadSlotsAt(GetBotsHubActionBase(), out);
}

static uintptr_t GetUiActionContextRaw(uintptr_t* outTyped, uintptr_t* outTyped2) {
    *outTyped = 0;
    *outTyped2 = 0;
    if (Offsets::ActionBase < 0x10000) return 0;
    __try {
        *outTyped  = *reinterpret_cast<uintptr_t*>(Offsets::ActionBase + 0xC);
        *outTyped2 = *reinterpret_cast<uintptr_t*>(Offsets::ActionBase + 0x4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (*outTyped < 0x10000) return 0;
    return *outTyped + 0xA8;
}

static uintptr_t GetUiActionContext() {
    uintptr_t t0 = 0, t1 = 0;
    return GetUiActionContextRaw(&t0, &t1);
}

static bool TryDispatchUiAction(uintptr_t ctx, uint32_t action, const char* modeLabel) {
    if (!s_doActionFn || ctx < 0x10000) {
        return false;
    }

    uint32_t payload[3] = { action, kPerformActionActivateFlag, kPerformActionTypeDefault };
    __try {
        s_doActionFn(reinterpret_cast<void*>(ctx), nullptr,
                     payload[1], payload, nullptr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("UIMgr: PerformUiAction %s dispatch fault ctx=0x%08X action=0x%X",
                  modeLabel ? modeLabel : "unknown",
                  static_cast<unsigned>(ctx),
                  action);
        return false;
    }
}

static bool PerformUiActionImpl(uint32_t action, bool preferEngineLane) {
    if (!s_doActionFn) {
        Log::Warn("UIMgr: PerformUiAction no DoActionFn action=0x%X", action);
        return false;
    }

    if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
        GameThread::EnqueuePost([action, preferEngineLane]() { PerformUiActionImpl(action, preferEngineLane); });
        return true;
    }

    ActionBaseDump preferredDump{};
    bool usedBotsHubScan = false;
    const uintptr_t preferredActionBase =
        GetPreferredActionBase(&preferredDump, &usedBotsHubScan, preferEngineLane);
    const bool ctosReady = CtoS::Initialize();
    const bool engineLaneAvailable = ctosReady && CtoS::IsBotshubCommandLaneAvailable();
    if (preferEngineLane && ctosReady && engineLaneAvailable && preferredActionBase >= 0x10000) {
        if (!s_loggedPerformUiActionEngineLane) {
            Log::Info("UIMgr: PerformUiAction using engine command lane");
            s_loggedPerformUiActionEngineLane = true;
        }
        Log::Info("UIMgr: PerformUiAction engine lane action=0x%X base=0x%08X source=%s",
                  action,
                  static_cast<unsigned>(preferredActionBase),
                  usedBotsHubScan ? "botshub-scan" : "offsets");
        PerformActionCommand cmd{};
        cmd.fn = reinterpret_cast<uintptr_t>(&BotshubActionCommandStub);
        cmd.action = action;
        cmd.flag = kPerformActionActivateFlag;
        cmd.type = kPerformActionTypeDefault;
        cmd.action_base = preferredActionBase;
        if (CtoS::EnqueueBotshubCommand(&cmd, sizeof(cmd))) {
            return true;
        }
        Log::Warn("UIMgr: PerformUiAction engine lane rejected action=0x%X", action);
    } else if (preferEngineLane) {
        Log::Warn("UIMgr: PerformUiAction engine lane unavailable action=0x%X ctosReady=%d laneAvailable=%d base=0x%08X",
                  action,
                  ctosReady ? 1 : 0,
                  engineLaneAvailable ? 1 : 0,
                  static_cast<unsigned>(preferredActionBase));
    }

    // BotsHub's Action command dereferences a pointer slot from ActionBase and
    // then adds 0xA8 before calling Action(). In this GW build the usable slot
    // is not consistently at +0xC, so pick the first plausible pointer from
    // the preferred ActionBase dump and still honor the original +0xA8 offset.
    ActionBaseDump dump{};
    const uintptr_t base = GetPreferredActionBase(&dump, &usedBotsHubScan, preferEngineLane);
    if (base < 0x10000) {
        Log::Warn("UIMgr: PerformUiAction ReadActionBaseDump failed action=0x%X", action);
        return false;
    }
    {
        char line[512] = {};
        int off = 0;
        for (uint32_t i = 0; i < 16 && off < (int)sizeof(line) - 16; ++i) {
            off += sprintf_s(line + off, sizeof(line) - off, "+%02X=%08X ",
                             i * 4, static_cast<unsigned>(dump.slots[i]));
        }
        Log::Info("UIMgr: PerformUiAction dump base=0x%08X (source=%s)  %s",
                  static_cast<unsigned>(base),
                  usedBotsHubScan ? "botshub-scan" : "offsets",
                  line);
    }

    // BotsHub's ASM: `mov ecx, dword[ActionBase]; mov ecx, dword[ecx+C]; add ecx, A8`.
    // Use slot +0xC (index 3) with the BotsHub base. Falls back to +0x4
    // (index 1) if that doesn't look like a heap pointer.
    uintptr_t p = dump.slots[3];
    if (!IsPlausibleUserPointer(p)) p = dump.slots[1];
    if (!IsPlausibleUserPointer(p)) {
        // Last resort: first pointer-looking slot.
        for (uint32_t i = 0; i < 16; ++i) {
            uintptr_t q = dump.slots[i];
            if (IsPlausibleUserPointer(q)) { p = q; break; }
        }
    }
    if (!IsPlausibleUserPointer(p)) {
        Log::Warn("UIMgr: PerformUiAction no ActionBase context action=0x%X", action);
        return false;
    }
    const uintptr_t fallbackCtx = p + 0xA8;
    Log::Info("UIMgr: PerformUiAction (ActionBase-slot fallback) ptr=0x%08X ctx=0x%08X action=0x%X",
              static_cast<unsigned>(p),
              static_cast<unsigned>(fallbackCtx), action);
    if (TryDispatchUiAction(fallbackCtx, action, "ActionBase-slot")) {
        return true;
    }

    // Last resort: reuse the existing frame-action context if it is still
    // present in this build.
    const uintptr_t ctx = GetActionContext();
    if (ctx >= 0x10000) {
        if (TryDispatchUiAction(ctx, action, "frame-ctx")) {
            Log::Info("UIMgr: PerformUiAction (frame-ctx) ctx=0x%08X flag=0x%X action=0x%X",
                      static_cast<unsigned>(ctx), kPerformActionActivateFlag, action);
            return true;
        }
    }
    return false;
}

bool PerformUiAction(uint32_t action) {
    return PerformUiActionImpl(action, true);
}

bool PerformUiActionDirect(uint32_t action) {
    return PerformUiActionImpl(action, false);
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

struct ButtonMouseActionPacket {
    uint32_t frame_id;
    uint32_t child_offset_id;
    uint32_t current_state;
    uint32_t internal_ptr;
    uint32_t zero_1;
    uint32_t zero_2;
    uint32_t field_1c4;
    uint32_t zero_3;
};

// POD task for EnqueuePostRaw — no std::function, no heap
struct ClickTask {
    uintptr_t sendFn;
    uintptr_t thisPtr;
    MouseAction action;
};
static_assert(sizeof(ClickTask) <= 64, "ClickTask exceeds InlineTask storage");

struct FullClickTask {
    uintptr_t sendFn;
    uintptr_t thisPtr;
    uint32_t frameId;
    uint32_t childOffsetId;
    uint32_t field1c4;
};
static_assert(sizeof(FullClickTask) <= 64, "FullClickTask exceeds InlineTask storage");

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

static void ExecuteClickTaskFull(void* storage) {
    auto* t = static_cast<FullClickTask*>(storage);
    static ButtonMouseActionPacket s_pkt;
    s_pkt.frame_id = t->frameId;
    s_pkt.child_offset_id = t->childOffsetId;
    s_pkt.internal_ptr = reinterpret_cast<uint32_t>(&s_pkt.zero_2);
    s_pkt.zero_1 = 0;
    s_pkt.zero_2 = 0;
    s_pkt.field_1c4 = t->field1c4;
    s_pkt.zero_3 = 0;
    void* tp = reinterpret_cast<void*>(t->thisPtr);
    uintptr_t fn = t->sendFn;
    void* wParam = &s_pkt;
    s_pkt.current_state = ACTION_MOUSE_DOWN;
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }
    s_pkt.current_state = ACTION_MOUSE_UP;
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }
}

static void ExecuteClickTaskFullMouseClick(void* storage) {
    auto* t = static_cast<FullClickTask*>(storage);
    static ButtonMouseActionPacket s_pkt;
    s_pkt.frame_id = t->frameId;
    s_pkt.child_offset_id = t->childOffsetId;
    s_pkt.internal_ptr = reinterpret_cast<uint32_t>(&s_pkt.zero_2);
    s_pkt.zero_1 = 0;
    s_pkt.zero_2 = 0;
    s_pkt.field_1c4 = t->field1c4;
    s_pkt.zero_3 = 0;
    s_pkt.current_state = ACTION_MOUSE_CLICK;
    void* tp = reinterpret_cast<void*>(t->thisPtr);
    uintptr_t fn = t->sendFn;
    void* wParam = &s_pkt;
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
    Log::Info("UIMgr: ButtonClickImmediate send begin frame=0x%08X frameId=%u childOffset=%u context=0x%08X thisPtr=0x%08X fn=0x%08X msg=0x%X actionState=0x%X",
              static_cast<unsigned>(frame),
              action.frame_id,
              action.child_offset_id,
              static_cast<unsigned>(context),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(tp)),
              static_cast<unsigned>(fn),
              MSG_MOUSE_CLICK2,
              action.action_state);
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }
    Log::Info("UIMgr: ButtonClickImmediate send end frame=0x%08X frameId=%u childOffset=%u",
              static_cast<unsigned>(frame),
              action.frame_id,
              action.child_offset_id);
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

    const uintptr_t parentFrame = GetParentFrame(frame);
    const uintptr_t context = parentFrame > 0x10000 ? parentFrame : GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClickImmediateFull invalid parent/context for frame 0x%08X", frame);
        return false;
    }

    ButtonMouseActionPacket action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.internal_ptr = reinterpret_cast<uint32_t>(&action.zero_2);
    action.zero_1 = 0;
    action.zero_2 = 0;
    __try {
        action.field_1c4 = *reinterpret_cast<uint32_t*>(frame + 0x1C4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        action.field_1c4 = 0;
    }
    action.zero_3 = 0;

    void* tp = reinterpret_cast<void*>(context + 0xA8);
    void* wParam = &action;
    uintptr_t fn = s_sendFrameUIAddr;

    action.current_state = ACTION_MOUSE_DOWN;
    __asm {
        push 0
        push wParam
        push MSG_MOUSE_CLICK2
        mov ecx, tp
        call fn
    }
    action.current_state = ACTION_MOUSE_UP;
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
    // clicks during live consumable runs.
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

    // Match the local GWA2_FrameUI reference: kMouseClick2 with a single
    // MouseClick (0x8) action state.
    action.action_state = ACTION_MOUSE_UP;
    memcpy(reinterpret_cast<void*>(s_frameClickAction), &action, sizeof(action));
    return RenderHook::EnqueueCommand(s_frameClickShellcode);
}

bool ButtonClickMouseClick(uintptr_t frame) {
    if (!s_sendFrameUIAddr || frame < 0x10000) {
        Log::Warn("UIMgr: ButtonClickMouseClick no sendAddr or invalid frame");
        return false;
    }

    const uint32_t state = GetFrameState(frame);
    if (!(state & FRAME_CREATED)) {
        Log::Warn("UIMgr: ButtonClickMouseClick frame 0x%08X not created (state=0x%X)", frame, state);
        return false;
    }

    const uintptr_t context = GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClickMouseClick invalid context for frame 0x%08X", frame);
        return false;
    }

    MouseAction action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.action_state = ACTION_MOUSE_CLICK;
    action.wparam = 0;
    action.lparam = 0;

    void* thisPtr = reinterpret_cast<void*>(context + 0xA8);
    if (GameThread::IsInitialized()) {
        ClickTask ct;
        ct.sendFn = s_sendFrameUIAddr;
        ct.thisPtr = reinterpret_cast<uintptr_t>(thisPtr);
        ct.action = action;
        Log::Info("UIMgr: ButtonClickMouseClick queueing pre-dispatch click frame=0x%08X frameId=%u childOffset=%u context=0x%08X",
                  static_cast<unsigned>(frame),
                  action.frame_id,
                  action.child_offset_id,
                  static_cast<unsigned>(context));
        GameThread::EnqueueRaw(ExecuteClickTask, &ct, sizeof(ct));
        return true;
    }

    if (!RenderHook::IsInitialized() || !EnsureFrameClickShellcode()) {
        Log::Warn("UIMgr: ButtonClickMouseClick no dispatch mechanism available");
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
    action.action_state = ACTION_MOUSE_CLICK;
    memcpy(reinterpret_cast<void*>(s_frameClickAction), &action, sizeof(action));
    return RenderHook::EnqueueCommand(s_frameClickShellcode);
}

bool ButtonClickFull(uintptr_t frame) {
    if (!s_sendFrameUIAddr || frame < 0x10000) {
        Log::Warn("UIMgr: ButtonClickFull no sendAddr or invalid frame");
        return false;
    }

    uint32_t state = GetFrameState(frame);
    if (!(state & FRAME_CREATED)) {
        Log::Warn("UIMgr: ButtonClickFull frame 0x%08X not created (state=0x%X)", frame, state);
        return false;
    }

    const uintptr_t parentFrame = GetParentFrame(frame);
    uintptr_t context = parentFrame > 0x10000 ? parentFrame : GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClickFull invalid parent/context for frame 0x%08X", frame);
        return false;
    }

    const uint32_t frameId = GetFrameId(frame);
    const uint32_t childOffsetId = GetChildOffsetId(frame);
    uint32_t field1c4 = 0;
    __try {
        field1c4 = *reinterpret_cast<uint32_t*>(frame + 0x1C4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        field1c4 = 0;
    }

    void* thisPtr = reinterpret_cast<void*>(context + 0xA8);

    if (GameThread::IsInitialized()) {
        FullClickTask ct;
        ct.sendFn = s_sendFrameUIAddr;
        ct.thisPtr = reinterpret_cast<uintptr_t>(thisPtr);
        ct.frameId = frameId;
        ct.childOffsetId = childOffsetId;
        ct.field1c4 = field1c4;

        Log::Info("UIMgr: ButtonClickFull queueing post-dispatch full click frame=0x%08X frameId=%u childOffset=%u context=0x%08X",
                  static_cast<unsigned>(frame),
                  frameId,
                  childOffsetId,
                  static_cast<unsigned>(context));
        GameThread::EnqueuePostRaw(ExecuteClickTaskFull, &ct, sizeof(ct));
        return true;
    }

    return ButtonClickImmediateFull(frame);
}

bool ButtonClickFullMouseClick(uintptr_t frame) {
    if (!s_sendFrameUIAddr || frame < 0x10000) {
        Log::Warn("UIMgr: ButtonClickFullMouseClick no sendAddr or invalid frame");
        return false;
    }

    const uint32_t state = GetFrameState(frame);
    if (!(state & FRAME_CREATED)) {
        Log::Warn("UIMgr: ButtonClickFullMouseClick frame 0x%08X not created (state=0x%X)", frame, state);
        return false;
    }

    const uintptr_t parentFrame = GetParentFrame(frame);
    const uintptr_t context = parentFrame > 0x10000 ? parentFrame : GetFrameContext(frame);
    if (context < 0x10000) {
        Log::Warn("UIMgr: ButtonClickFullMouseClick invalid parent/context for frame 0x%08X", frame);
        return false;
    }

    ButtonMouseActionPacket action{};
    action.frame_id = GetFrameId(frame);
    action.child_offset_id = GetChildOffsetId(frame);
    action.internal_ptr = reinterpret_cast<uint32_t>(&action.zero_2);
    action.zero_1 = 0;
    action.zero_2 = 0;
    __try {
        action.field_1c4 = *reinterpret_cast<uint32_t*>(frame + 0x1C4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        action.field_1c4 = 0;
    }
    action.zero_3 = 0;
    action.current_state = ACTION_MOUSE_CLICK;

    void* thisPtr = reinterpret_cast<void*>(context + 0xA8);
    if (GameThread::IsInitialized()) {
        FullClickTask ct;
        ct.sendFn = s_sendFrameUIAddr;
        ct.thisPtr = reinterpret_cast<uintptr_t>(thisPtr);
        ct.frameId = action.frame_id;
        ct.childOffsetId = action.child_offset_id;
        ct.field1c4 = action.field_1c4;

        Log::Info("UIMgr: ButtonClickFullMouseClick queueing post-dispatch click frame=0x%08X frameId=%u childOffset=%u context=0x%08X",
                  static_cast<unsigned>(frame),
                  action.frame_id,
                  action.child_offset_id,
                  static_cast<unsigned>(context));
        GameThread::EnqueuePostRaw(ExecuteClickTaskFullMouseClick, &ct, sizeof(ct));
        return true;
    }

    return ButtonClickImmediateFull(frame);
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

bool HasControlActionKeypress() {
    return (s_sendFrameUIAddr != 0 && GetActionFrame() >= 0x10000)
        || (s_doActionFn != nullptr && GetActionContext() >= 0x10000);
}

bool ControlActionKeyDown(ControlAction action) {
    return SendControlAction(0x1E, action);
}

bool ControlActionKeyUp(ControlAction action) {
    return SendControlAction(0x20, action);
}

bool ControlActionKeyPress(ControlAction action) {
    if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
        GameThread::Enqueue([action]() {
            ControlActionKeyPress(action);
        });
        return true;
    }

    const bool downOk = ControlActionKeyDown(action);
    if (!downOk) return false;
    if (GameThread::IsInitialized()) {
        GameThread::Enqueue([action]() {
            ControlActionKeyUp(action);
        });
        return true;
    }
    Sleep(30);
    return ControlActionKeyUp(action);
}

bool ActionKeyDown(uint32_t action) {
    return SendControlAction(0x1E, static_cast<ControlAction>(action));
}

bool ActionKeyUp(uint32_t action) {
    return SendControlAction(0x20, static_cast<ControlAction>(action));
}

bool ActionKeyPress(uint32_t action) {
    if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
        GameThread::Enqueue([action]() {
            ActionKeyPress(action);
        });
        return true;
    }

    const bool downOk = ActionKeyDown(action);
    if (!downOk) return false;
    if (GameThread::IsInitialized()) {
        GameThread::Enqueue([action]() {
            ActionKeyUp(action);
        });
        return true;
    }
    Sleep(30);
    return ActionKeyUp(action);
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
