#include <gwa3/managers/QuestMgr.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/GameTypes.h>
#include <gwa3/utils/EncStringCache.h>

#include <Windows.h>

namespace GWA3::QuestMgr {

static bool s_initialized = false;
using QuestActionFn = void(__cdecl*)(uint32_t);
using SetActiveQuestFn = QuestActionFn;
using AbandonQuestFn = QuestActionFn;
using RequestQuestInfoFn = QuestActionFn;
using SendDialogFn = void(__cdecl*)(uint32_t);
static SetActiveQuestFn s_setActiveQuestFn = nullptr;
static AbandonQuestFn s_abandonQuestFn = nullptr;
static RequestQuestInfoFn s_requestQuestInfoFn = nullptr;
static SendDialogFn s_sendDialogFn = nullptr;
static SendDialogFn s_sendSignpostDialogFn = nullptr;
// UIMessage IDs for the quest subsystem. These are GWCA-documented constants
// for the SetActiveQuest / AbandonQuest hooks that GW itself raises when the
// player clicks the in-game quest log. We piggyback on them when the native
// function pointer is unavailable.
static constexpr uint32_t kSendSetActiveQuestUiMessage = 0x30000009u;
static constexpr uint32_t kSendAbandonQuestUiMessage   = 0x3000000Au;
static bool s_loggedNativeDialog = false;
static bool s_loggedFallbackDialog = false;

// WorldContext resolution delegated to Offsets::ResolveWorldContext()

bool Initialize() {
    if (s_initialized) return true;

    // GWCA QuestMgr::Init() — the UI callback that sits behind the in-game
    // quest log window contains near-calls into SetActiveQuest (+0x96) and
    // AbandonQuest (+0x100). See GWA Censured/GWCA-master/Source/QuestMgr.cpp.
    uintptr_t questLogUi = Scanner::FindAssertion(
        "P:\\Code\\Gw\\Ui\\Game\\Quest\\QuestLog.cpp",
        "MISSION_MAP_OUTPOST == MissionCliGetMap()",
        -0x128);
    if (questLogUi > 0x10000) {
        uintptr_t setFn = Scanner::FunctionFromNearCall(questLogUi + 0x96);
        if (setFn > 0x10000) {
            s_setActiveQuestFn = reinterpret_cast<SetActiveQuestFn>(setFn);
        }
        uintptr_t abandonFn = Scanner::FunctionFromNearCall(questLogUi + 0x100);
        if (abandonFn > 0x10000) {
            s_abandonQuestFn = reinterpret_cast<AbandonQuestFn>(abandonFn);
        }
    }

    // RequestQuestInfo lives elsewhere — GWCA locates it via a distinctive
    // byte pattern (PUSH 0x1000014A / PUSH [EDI+4]) then a +0x7A near-call.
    uintptr_t requestInfoAnchor = Scanner::Find(
        "\x68\x4a\x01\x00\x10\xff\x77\x04", "xxxxxxxx", 0x7a);
    if (requestInfoAnchor > 0x10000) {
        uintptr_t fn = Scanner::FunctionFromNearCall(requestInfoAnchor);
        if (fn > 0x10000) {
            s_requestQuestInfoFn = reinterpret_cast<RequestQuestInfoFn>(fn);
        }
    }

    uintptr_t dialogAnchor = Scanner::Find("\x89\x4B\x24\x8B\x4B\x28\x83\xE9\x00", "xxxxxxxxx");
    if (dialogAnchor > 0x10000) {
        uintptr_t sendDialog = Scanner::FunctionFromNearCall(dialogAnchor + 0x15);
        uintptr_t sendSignpostDialog = Scanner::FunctionFromNearCall(dialogAnchor + 0x25);
        if (sendDialog > 0x10000) {
            s_sendDialogFn = reinterpret_cast<SendDialogFn>(sendDialog);
        }
        if (sendSignpostDialog > 0x10000) {
            s_sendSignpostDialogFn = reinterpret_cast<SendDialogFn>(sendSignpostDialog);
        }
        DialogHook::SetNativeDialogFunctions(
            reinterpret_cast<uintptr_t>(s_sendDialogFn),
            reinterpret_cast<uintptr_t>(s_sendSignpostDialogFn));
    }

    s_initialized = true;
    Log::Info("QuestMgr: Initialized (SetActiveQuest=0x%08X, AbandonQuest=0x%08X, RequestQuestInfo=0x%08X, SendDialog=0x%08X, SendSignpostDialog=0x%08X)",
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_setActiveQuestFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_abandonQuestFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_requestQuestInfoFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_sendDialogFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_sendSignpostDialogFn)));
    return true;
}

void Dialog(uint32_t dialogId) {
    auto* target = AgentMgr::GetAgentByID(AgentMgr::GetTargetId());
    const bool useSignpost = target && target->type == 0x200 && s_sendSignpostDialogFn;
    const bool useNpcDialog = target && target->type != 0x200 && s_sendDialogFn;
    DialogHook::RecordDialogSend(dialogId);

    if (useNpcDialog && GameThread::IsInitialized()) {
        auto fn = s_sendDialogFn;
        if (!s_loggedNativeDialog) {
            Log::Info("QuestMgr: Dialog using native %s path fn=0x%08X",
                      "npc",
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(fn)));
            s_loggedNativeDialog = true;
        }
        GameThread::EnqueuePost([fn, dialogId]() {
            fn(dialogId);
        });
        return;
    }

    if (useSignpost && GameThread::IsInitialized()) {
        auto fn = s_sendSignpostDialogFn;
        if (!s_loggedNativeDialog) {
            Log::Info("QuestMgr: Dialog using native %s path fn=0x%08X",
                      "signpost",
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(fn)));
            s_loggedNativeDialog = true;
        }
        GameThread::EnqueuePost([fn, dialogId]() {
            fn(dialogId);
        });
        return;
    }

    if (!s_loggedFallbackDialog) {
        Log::Info("QuestMgr: Dialog using AutoIt packet path hdr=0x%X", Packets::DIALOG_SEND);
        s_loggedFallbackDialog = true;
    }
    CtoS::Dialog(dialogId);
}

void SetActiveQuest(uint32_t questId) {
    if (s_setActiveQuestFn && GameThread::IsInitialized()) {
        auto fn = s_setActiveQuestFn;
        GameThread::EnqueuePost([fn, questId]() {
            fn(questId);
        });
        return;
    }

    if (GameThread::IsInitialized()) {
        GameThread::EnqueuePost([questId]() {
            UIMgr::SendUIMessage(
                kSendSetActiveQuestUiMessage,
                reinterpret_cast<void*>(static_cast<uintptr_t>(questId)),
                nullptr);
        });
        return;
    }

    if (Offsets::UIMessage > 0x10000) {
        UIMgr::SendUIMessage(
            kSendSetActiveQuestUiMessage,
            reinterpret_cast<void*>(static_cast<uintptr_t>(questId)),
            nullptr);
        return;
    }

    CtoS::QuestSetActive(questId);
}

void AbandonQuest(uint32_t questId) {
    if (questId == 0) return;
    if (s_abandonQuestFn && GameThread::IsInitialized()) {
        auto fn = s_abandonQuestFn;
        GameThread::EnqueuePost([fn, questId]() {
            fn(questId);
        });
        return;
    }

    if (GameThread::IsInitialized()) {
        GameThread::EnqueuePost([questId]() {
            UIMgr::SendUIMessage(
                kSendAbandonQuestUiMessage,
                reinterpret_cast<void*>(static_cast<uintptr_t>(questId)),
                nullptr);
        });
        return;
    }

    if (Offsets::UIMessage > 0x10000) {
        UIMgr::SendUIMessage(
            kSendAbandonQuestUiMessage,
            reinterpret_cast<void*>(static_cast<uintptr_t>(questId)),
            nullptr);
        return;
    }

    CtoS::QuestAbandon(questId);
}

// --- Label-frame walker ---------------------------------------------------
//
// Reads GWCA_UIMessage_Research.md 3644-3656's "encoded | '\0' | decoded |
// '\0'" layout out of TextLabelFrame / MultiLineTextLabelFrame contexts.
// The key insight is that the decoded form lives in the SAME heap buffer
// as the encoded form, immediately past its null terminator — no game-
// function call required to read it.
//
// We don't know the exact frame type at runtime, so we probe a small set
// of candidate context offsets that GWCA's frame decompilations show as
// likely "string_base" slots (0x04 for ButtonFrame, varies for the label
// frames). For each candidate wchar_t*, we gate on:
//
//   - pointer value looks like heap (> 0x10000)
//   - first wchar is a GW encoded-string sentinel (0x2... / 0x8101 / 0x8102)
//   - after the null terminator, there is at least one more wchar
//   - that follow-on run is mostly printable (ASCII 0x20..0x7E, or common
//     latin/accented ranges) — i.e. it's a plausible decoded sibling, not
//     another encoded string or heap metadata
//
// When the encoded content matches one of the current quest log's five
// encoded pointers (by content, not pointer identity — the label's copy
// lives in a different allocation), we populate EncStringCache keyed on
// the quest-struct wide string so Lookup() via the snapshot path surfaces
// the decoded text.

namespace {

constexpr uintptr_t kCandidateOffsets[] = {
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20
};

struct EncStringView {
    const wchar_t* ptr;   // start of the encoded wchars (valid until null)
    size_t length;        // length excluding null terminator
};

static bool IsEncodedSentinel(wchar_t w) {
    // GW encoded strings start either with a 0x8101/0x8102 database
    // reference (our observed quest-name pattern) or a 0x2xxx formatted
    // string header (our observed quest-objectives pattern). Some also
    // start in the 0x1xxx range when the enc is a literal. Other wchars
    // are very unlikely as the first char of a real enc string.
    return w == 0x8101 || w == 0x8102 || (w >= 0x2000 && w < 0x3000)
        || (w >= 0x1000 && w < 0x2000);
}

__declspec(noinline) static bool LooksLikeReadableSibling(const wchar_t* s, size_t maxLen, size_t* outLen) {
    if (!s) return false;
    size_t i = 0;
    int readable = 0;
    for (; i < maxLen; ++i) {
        wchar_t c = 0;
        __try { c = s[i]; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (c == 0) break;
        // ASCII printable, latin-1 supplement + extended-A (for non-English
        // clients), and whitespace. Explicitly reject CJK and high PUA —
        // those ranges are where encoded-string body bytes and heap
        // metadata show up.
        bool ok = (c >= 0x20 && c < 0x7F)
               || (c >= 0xA0 && c <= 0x017F)
               || c == L'\n' || c == L'\t';
        if (!ok) return false;
        if (c != L'\n' && c != L'\t') ++readable;
    }
    if (outLen) *outLen = i;
    return i >= 3 && readable >= 3 && readable * 10 >= static_cast<int>(i) * 8;
}

struct WalkerCtx {
    uint32_t          pairsSeen = 0;
    uint32_t          pairsMatched = 0;
    uint32_t          logSampleBudget = 6;  // cap log noise
    // Quest strings we care about, keyed by encoded content. We keep a
    // vector of {wchar_t* from quest struct, its length} for O(logCount)
    // comparison per candidate.
    struct Target {
        const wchar_t* encPtr;
        size_t         encLen;
    };
    Target            targets[64];
    uint32_t          targetCount = 0;
};

__declspec(noinline) static bool WcsEqualsBounded(const wchar_t* a, size_t aLen, const wchar_t* b, size_t bLen) {
    if (aLen != bLen) return false;
    __try {
        for (size_t i = 0; i < aLen; ++i) {
            if (a[i] != b[i]) return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

__declspec(noinline) static bool TryReadEncodedView(const wchar_t* p, EncStringView* out) {
    if (reinterpret_cast<uintptr_t>(p) <= 0x10000) return false;
    __try {
        wchar_t first = p[0];
        if (!IsEncodedSentinel(first)) return false;
        size_t i = 1;
        // Bound the scan — real enc strings are well under 64 wchars.
        for (; i < 64 && p[i] != 0; ++i) {}
        if (i == 64) return false;
        out->ptr = p;
        out->length = i;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

__declspec(noinline) static void CacheDecodedQuestString(const wchar_t* encPtr, const char* utf8) {
    if (!encPtr || !utf8 || !utf8[0]) {
        return;
    }
    EncStringCache::InsertDecoded(encPtr, std::string(utf8));
}

// SEH-only helpers. These must not hold any C++ object that requires
// destruction; the caller owns all RAII.

__declspec(noinline) static bool SafeReadPtr(uintptr_t addr, const wchar_t** out) {
    *out = nullptr;
    __try {
        *out = *reinterpret_cast<wchar_t**>(addr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

__declspec(noinline) static bool SafeCopyWchars(const wchar_t* src, size_t n, wchar_t* dst) {
    __try {
        for (size_t j = 0; j < n; ++j) dst[j] = src[j];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void ProbeFrameContext(uintptr_t frame, void* userdata) {
    auto* wc = static_cast<WalkerCtx*>(userdata);
    const uintptr_t ctx = UIMgr::GetFrameContext(frame);
    if (ctx <= 0x10000) return;

    for (uintptr_t off : kCandidateOffsets) {
        const wchar_t* encPtr = nullptr;
        if (!SafeReadPtr(ctx + off, &encPtr)) continue;

        EncStringView enc{};
        if (!TryReadEncodedView(encPtr, &enc)) continue;

        const wchar_t* after = encPtr + enc.length + 1;
        size_t decLen = 0;
        if (!LooksLikeReadableSibling(after, 256, &decLen)) continue;

        ++wc->pairsSeen;
        if (wc->logSampleBudget > 0) {
            --wc->logSampleBudget;
            char decUtf8[512] = {};
            WideCharToMultiByte(CP_UTF8, 0, after, static_cast<int>(decLen),
                                decUtf8, sizeof(decUtf8) - 1, nullptr, nullptr);
            Log::Info("QuestMgr::ScanLabelFrames: frame=0x%08X ctx=0x%08X +0x%X enc[%zu] decoded=\"%s\"",
                      static_cast<unsigned>(frame),
                      static_cast<unsigned>(ctx),
                      static_cast<unsigned>(off),
                      enc.length, decUtf8);
        }

        // Content-match against quest struct encoded strings. The label
        // frame allocates its own copy, so pointer identity won't work.
        for (uint32_t i = 0; i < wc->targetCount; ++i) {
            const auto& t = wc->targets[i];
            if (!WcsEqualsBounded(enc.ptr, enc.length, t.encPtr, t.encLen)) continue;

            wchar_t decBuf[256] = {};
            size_t copyLen = decLen < 255 ? decLen : 255;
            if (!SafeCopyWchars(after, copyLen, decBuf)) break;
            decBuf[copyLen] = 0;

            char utf8[1024] = {};
            int n = WideCharToMultiByte(CP_UTF8, 0, decBuf, -1, utf8,
                                        sizeof(utf8) - 1, nullptr, nullptr);
            if (n > 0) {
                CacheDecodedQuestString(t.encPtr, utf8);
                ++wc->pairsMatched;
            }
            break;
        }
    }
}

} // namespace

uint32_t ScanLabelFramesForQuestStrings() {
    WalkerCtx wc{};

    // Build the target set from the current quest log.
    const uint32_t logSize = GetQuestLogSize();
    uint32_t pushed = 0;
    for (uint32_t i = 0; i < logSize && pushed < 64; ++i) {
        Quest* q = GetQuestByIndex(i);
        if (!q) continue;
        const wchar_t* const fields[] = {
            q->name, q->location, q->npc, q->description, q->objectives
        };
        for (const wchar_t* f : fields) {
            if (pushed >= 64) break;
            if (!f || !f[0]) continue;
            EncStringView v{};
            if (!TryReadEncodedView(f, &v)) continue;
            wc.targets[pushed].encPtr = v.ptr;
            wc.targets[pushed].encLen = v.length;
            ++pushed;
        }
    }
    wc.targetCount = pushed;
    Log::Info("QuestMgr::ScanLabelFrames: scanning %u quest-string targets", pushed);

    UIMgr::ForEachFrame(ProbeFrameContext, &wc);

    Log::Info("QuestMgr::ScanLabelFrames: seen=%u matched=%u",
              wc.pairsSeen, wc.pairsMatched);
    return wc.pairsMatched;
}

void ToggleQuestLogWindow() {
    // Path chosen: call GWCA's `SetWindowVisible(WindowID_QuestLog=0x4F, 1)`.
    // This is a cleaner dedicated UI function (not a key-action), and is
    // what GWCA/GWToolbox use for window toggling. We scan it from a
    // distinctive prologue pattern (same pattern GWCA uses upstream).
    //
    // An earlier attempt via `UIMgr::ActionKeyPress(0x8E)` — which maps
    // to the "press 'L'" key-action binding — crashed GW the first time
    // it fired on BISCUIT (the action-context packet layout is wrong
    // for UI actions vs. the skill-slot actions that codepath was built
    // for). The SetWindowVisible path avoids that codepath entirely.
    using SetWindowVisibleFn = void(__cdecl*)(uint32_t windowId, uint32_t isVisible,
                                              void* wParam, void* lParam);
    static SetWindowVisibleFn s_fn = nullptr;
    static bool s_resolveAttempted = false;
    if (!s_resolveAttempted) {
        s_resolveAttempted = true;
        // GWCA pattern uses literal 0x66 (window-array size) in the cmp.
        // That number may shift across GW builds, so we also try the
        // pattern with that byte wildcarded.
        uintptr_t addr = Scanner::Find(
            "\x8B\x75\x08\x83\xFE\x66\x7C\x19\x68", "xxxxxxxxx", -0x7);
        if (addr <= 0x10000) {
            addr = Scanner::Find(
                "\x8B\x75\x08\x83\xFE\x66\x7C\x19\x68", "xxxxx?xxx", -0x7);
        }
        if (addr > 0x10000) {
            s_fn = reinterpret_cast<SetWindowVisibleFn>(addr);
        }
        Log::Info("QuestMgr: SetWindowVisible_Func=0x%08X",
                  static_cast<unsigned>(addr));
    }
    if (!s_fn) {
        Log::Warn("QuestMgr: ToggleQuestLogWindow has no SetWindowVisible_Func");
        return;
    }
    constexpr uint32_t kWindowIdQuestLog = 0x4F;
    auto fn = s_fn;
    const uint32_t windowId = kWindowIdQuestLog;
    if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
        GameThread::Enqueue([fn, windowId]() {
            fn(windowId, 1, nullptr, nullptr);
        });
    } else {
        fn(windowId, 1, nullptr, nullptr);
    }
}

void RequestQuestInfo(uint32_t questId) {
    if (questId == 0) return;

    // EncStringCache now prefers GWCA's byte-pattern-scanned address
    // (Offsets::ValidateAsyncDecodeStrGwca). Live testing on BISCUIT
    // confirmed the two scans resolve to different addresses
    // (assertion=0x5F4C44, gwca=0x5F5050) but the GWCA-scanned address
    // still produces the same delayed crash. Prime call stays dormant
    // while we pursue the text_parser / AsyncDecodeStringPtr hook
    // investigation. See QUEST_LOG_RESEARCH.md.
    //
    // if (Quest* q = GetQuestById(questId)) {
    //     EncStringCache::Prime(q->name);
    //     EncStringCache::Prime(q->location);
    //     EncStringCache::Prime(q->npc);
    //     EncStringCache::Prime(q->description);
    //     EncStringCache::Prime(q->objectives);
    // }

    if (s_requestQuestInfoFn && GameThread::IsInitialized()) {
        auto fn = s_requestQuestInfoFn;
        GameThread::EnqueuePost([fn, questId]() {
            fn(questId);
        });
        return;
    }
    CtoS::SendPacket(2, Packets::QUEST_REQUEST_INFOS, questId);
}

void SkipCinematic() {
    CtoS::SendPacket(1, Packets::CINEMATIC_SKIP);
}

uint32_t GetActiveQuestId() {
    uintptr_t wc = Offsets::ResolveWorldContext();
    if (!wc) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(wc + 0x528);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t GetQuestLogSize() {
    uintptr_t wc = Offsets::ResolveWorldContext();
    if (!wc) return 0;
    __try {
        // GWArray<Quest> at WorldContext + 0x52C: buffer at +0, size at +8
        auto* arr = reinterpret_cast<GWArray<Quest>*>(wc + 0x52C);
        if (!arr || !arr->buffer || arr->size > 256) return 0;
        return arr->size;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

Quest* GetQuestByIndex(uint32_t index) {
    uintptr_t wc = Offsets::ResolveWorldContext();
    if (!wc) return nullptr;
    __try {
        auto* arr = reinterpret_cast<GWArray<Quest>*>(wc + 0x52C);
        if (!arr || !arr->buffer || index >= arr->size || arr->size > 256) return nullptr;
        return &arr->buffer[index];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

Quest* GetQuestById(uint32_t questId) {
    if (questId == 0) return nullptr;
    uintptr_t wc = Offsets::ResolveWorldContext();
    if (!wc) return nullptr;
    __try {
        auto* arr = reinterpret_cast<GWArray<Quest>*>(wc + 0x52C);
        if (!arr || !arr->buffer || arr->size == 0 || arr->size > 256) return nullptr;
        for (uint32_t i = 0; i < arr->size; i++) {
            if (arr->buffer[i].quest_id == questId) {
                return &arr->buffer[i];
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

} // namespace GWA3::QuestMgr
