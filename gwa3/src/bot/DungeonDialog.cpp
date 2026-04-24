#include <gwa3/bot/DungeonDialog.h>

#include <gwa3/managers/QuestMgr.h>

#include <Windows.h>

namespace GWA3::Bot::DungeonDialog {

bool SendDialogWithRetry(uint32_t dialogId, int maxRetries, uint32_t delayMs) {
    if (dialogId == 0u || maxRetries <= 0) {
        return false;
    }

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        QuestMgr::Dialog(dialogId);
        Sleep(delayMs);
    }

    return true;
}

bool SendDialogSequence(
    const uint32_t* dialogIds,
    int count,
    uint32_t delayMs,
    int maxRetriesPerDialog) {
    if (dialogIds == nullptr || count <= 0 || maxRetriesPerDialog <= 0) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        if (!SendDialogWithRetry(dialogIds[i], maxRetriesPerDialog, delayMs)) {
            return false;
        }
    }

    return true;
}

bool SendDialogSequenceRepeated(
    const uint32_t* dialogIds,
    int count,
    int sequenceRepeats,
    uint32_t delayMs,
    uint32_t repeatDelayMs,
    int maxRetriesPerDialog) {
    if (sequenceRepeats <= 0) {
        return false;
    }

    for (int repeat = 0; repeat < sequenceRepeats; ++repeat) {
        if (!SendDialogSequence(dialogIds, count, delayMs, maxRetriesPerDialog)) {
            return false;
        }
        if (repeat + 1 < sequenceRepeats) {
            Sleep(repeatDelayMs);
        }
    }

    return true;
}

} // namespace GWA3::Bot::DungeonDialog
