#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/GameTypes.h>

#include <Windows.h>

namespace GWA3::QuestMgr {

static bool s_initialized = false;
using SetActiveQuestFn = void(__cdecl*)(uint32_t);
using SendDialogFn = void(__cdecl*)(uint32_t);
static SetActiveQuestFn s_setActiveQuestFn = nullptr;
static SendDialogFn s_sendDialogFn = nullptr;
static SendDialogFn s_sendSignpostDialogFn = nullptr;
static constexpr uint32_t kSendSetActiveQuestUiMessage = 0x30000009u;
static bool s_loggedNativeDialog = false;
static bool s_loggedFallbackDialog = false;

// WorldContext: BasePointer → deref → +0x18 → +0x2C
static uintptr_t ResolveWorldContext() {
    if (Offsets::BasePointer <= 0x10000) return 0;
    __try {
        uintptr_t p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (p0 <= 0x10000) return 0;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
        if (p1 <= 0x10000) return 0;
        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
        if (p2 <= 0x10000) return 0;
        return p2;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool Initialize() {
    if (s_initialized) return true;

    uintptr_t questLogUi = Scanner::FindAssertion(
        "P:\\Code\\Gw\\Ui\\Game\\Quest\\QuestLog.cpp",
        "MISSION_MAP_OUTPOST == MissionCliGetMap()",
        -0x128);
    if (questLogUi > 0x10000) {
        uintptr_t fn = Scanner::FunctionFromNearCall(questLogUi + 0x96);
        if (fn > 0x10000) {
            s_setActiveQuestFn = reinterpret_cast<SetActiveQuestFn>(fn);
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
    }

    s_initialized = true;
    Log::Info("QuestMgr: Initialized (SetActiveQuest=0x%08X, SendDialog=0x%08X, SendSignpostDialog=0x%08X)",
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_setActiveQuestFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_sendDialogFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_sendSignpostDialogFn)));
    return true;
}

void Dialog(uint32_t dialogId) {
    auto* target = AgentMgr::GetAgentByID(AgentMgr::GetTargetId());
    const bool useSignpost = target && target->type == 0x200 && s_sendSignpostDialogFn;
    auto fn = useSignpost ? s_sendSignpostDialogFn : s_sendDialogFn;

    if (fn && GameThread::IsInitialized()) {
        if (!s_loggedNativeDialog) {
            Log::Info("QuestMgr: Dialog using native %s path fn=0x%08X",
                      useSignpost ? "signpost" : "dialog",
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(fn)));
            s_loggedNativeDialog = true;
        }
        GameThread::EnqueuePost([fn, dialogId]() {
            fn(dialogId);
        });
        return;
    }

    if (!s_loggedFallbackDialog) {
        Log::Warn("QuestMgr: Dialog falling back to raw packet path");
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
    CtoS::QuestAbandon(questId);
}

void RequestQuestInfo(uint32_t questId) {
    CtoS::SendPacket(2, Packets::QUEST_REQUEST_INFOS, questId);
}

void SkipCinematic() {
    CtoS::SendPacket(1, Packets::CINEMATIC_SKIP);
}

uint32_t GetActiveQuestId() {
    uintptr_t wc = ResolveWorldContext();
    if (!wc) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(wc + 0x528);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t GetQuestLogSize() {
    uintptr_t wc = ResolveWorldContext();
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
    uintptr_t wc = ResolveWorldContext();
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
    uintptr_t wc = ResolveWorldContext();
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
