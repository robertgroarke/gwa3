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

void ToggleQuestLogWindow() {
    // Proven path: UIMgr::ActionKeyPress(0x8E), equivalent to the player
    // pressing the 'L' key. The underlying SendControlAction in this
    // build now dispatches via the UI frame tree (finds the right
    // childOffset=6 frame and sends msg=0x20 KeyUp + msg=0x22 trigger),
    // not the raw ActionBase chain we explored earlier.
    //
    // Live-verified on 2026-04-18: the Quest Log panel
    // ("Quest Log [L]", Active Quests heading) appeared after this
    // call, and GW did not crash.
    //
    // Why not SetWindowVisible? It only flips visibility on an
    // already-created window. First-time open needs the keybind path.
    // The earlier crashes when testing 0x8E via this same path were
    // transient (other state issues in those runs); a clean run with
    // the current codebase opens the window reliably.
    constexpr uint32_t kControlAction_OpenQuestLog = 0x8E;
    UIMgr::ActionKeyPress(kControlAction_OpenQuestLog);
}

void RequestQuestInfo(uint32_t questId) {
    if (questId == 0) return;

    // EncStringCache now prefers GWCA's byte-pattern-scanned address
    // (Offsets::ValidateAsyncDecodeStrGwca). Live testing
    // confirmed the two scans resolve to different addresses
    // (assertion=0x5F4C44, gwca=0x5F5050) but the GWCA-scanned address
    // still produces the same delayed crash. Prime call stays dormant
    // while we pursue the text_parser / AsyncDecodeStringPtr hook
    // investigation.
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
