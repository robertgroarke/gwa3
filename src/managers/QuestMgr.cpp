#include <gwa3/managers/QuestMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/GameTypes.h>

#include <Windows.h>

namespace GWA3::QuestMgr {

static bool s_initialized = false;

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
    s_initialized = true;
    Log::Info("QuestMgr: Initialized");
    return true;
}

void Dialog(uint32_t dialogId) {
    CtoS::Dialog(dialogId);
}

void SetActiveQuest(uint32_t questId) {
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
