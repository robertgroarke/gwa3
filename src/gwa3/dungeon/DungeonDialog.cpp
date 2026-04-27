#include <gwa3/dungeon/DungeonDialog.h>

#include <gwa3/core/DialogHook.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/QuestMgr.h>

#include <Windows.h>

namespace GWA3::DungeonDialog {

DialogSnapshot CaptureDialogSnapshot() {
    DialogSnapshot snapshot = {};
    snapshot.dialog_open = DialogMgr::IsDialogOpen();
    snapshot.sender_agent_id = DialogMgr::GetDialogSenderAgentId();
    snapshot.button_count = DialogMgr::GetButtonCount();
    snapshot.last_dialog_id = DialogMgr::GetLastDialogId();
    return snapshot;
}

bool HasDialogButton(uint32_t dialogId) {
    if (dialogId == 0u) {
        return false;
    }

    const uint32_t buttonCount = DialogMgr::GetButtonCount();
    for (uint32_t i = 0; i < buttonCount; ++i) {
        const auto* button = DialogMgr::GetButton(i);
        if (button != nullptr && button->dialog_id == dialogId) {
            return true;
        }
    }
    return false;
}

bool IsDialogOpenFromSenderWithButton(uint32_t senderAgentId, uint32_t dialogId) {
    if (senderAgentId == 0u || dialogId == 0u) {
        return false;
    }
    return DialogMgr::IsDialogOpen() &&
           DialogMgr::GetDialogSenderAgentId() == senderAgentId &&
           HasDialogButton(dialogId);
}

void LogDialogButtons(const char* log_prefix, const char* label) {
    const char* prefix = log_prefix ? log_prefix : "DungeonDialog";
    const char* logLabel = label ? label : "Dialog buttons";
    const DialogSnapshot snapshot = CaptureDialogSnapshot();
    Log::Info("%s: %s dialogOpen=%d sender=%u buttons=%u lastDialog=0x%X",
              prefix,
              logLabel,
              snapshot.dialog_open ? 1 : 0,
              snapshot.sender_agent_id,
              snapshot.button_count,
              snapshot.last_dialog_id);
    for (uint32_t idx = 0; idx < snapshot.button_count; ++idx) {
        const auto* button = DialogMgr::GetButton(idx);
        if (!button) continue;
        Log::Info("%s: %s button[%u] dialog_id=0x%X icon=%u",
                  prefix,
                  logLabel,
                  idx,
                  button->dialog_id,
                  button->button_icon);
    }
}

bool AdvanceDialogToButton(uint32_t targetDialogId, const DialogAdvanceOptions& options) {
    if (targetDialogId == 0u || options.max_passes <= 0 || options.max_buttons_per_pass == 0u) {
        return false;
    }

    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "DungeonDialog";
    const char* label = options.label != nullptr ? options.label : "AdvanceDialogToButton";

    for (int pass = 0; pass < options.max_passes; ++pass) {
        if (!DialogMgr::IsDialogOpen()) {
            return false;
        }
        if (options.sender_agent_id != 0u &&
            DialogMgr::GetDialogSenderAgentId() != options.sender_agent_id) {
            return false;
        }
        if (HasDialogButton(targetDialogId)) {
            return true;
        }

        const uint32_t buttonCount = DialogMgr::GetButtonCount();
        const uint32_t scanCount = buttonCount < options.max_buttons_per_pass
            ? buttonCount
            : options.max_buttons_per_pass;
        bool advanced = false;
        for (uint32_t idx = 0; idx < scanCount; ++idx) {
            const auto* button = DialogMgr::GetButton(idx);
            if (button == nullptr || button->dialog_id == 0u || button->dialog_id == targetDialogId) {
                continue;
            }

            Log::Info("%s: %s advancing dialog pass=%d button[%u]=0x%X icon=%u target=0x%X",
                      prefix,
                      label,
                      pass + 1,
                      idx,
                      button->dialog_id,
                      button->button_icon,
                      targetDialogId);
            if (options.change_target_before_dialog && options.sender_agent_id != 0u) {
                AgentMgr::ChangeTarget(options.sender_agent_id);
                Sleep(options.post_target_wait_ms);
            }
            QuestMgr::Dialog(button->dialog_id);
            Sleep(options.post_dialog_wait_ms);
            if (options.request_quest_id_after_dialog != 0u) {
                QuestMgr::RequestQuestInfo(options.request_quest_id_after_dialog);
                Sleep(options.post_request_wait_ms);
            }
            advanced = true;
            break;
        }
        if (!advanced) {
            break;
        }
    }

    if (!DialogMgr::IsDialogOpen()) {
        return false;
    }
    if (options.sender_agent_id != 0u &&
        DialogMgr::GetDialogSenderAgentId() != options.sender_agent_id) {
        return false;
    }
    return HasDialogButton(targetDialogId);
}

bool SendDialogWithRetry(uint32_t dialogId, int maxRetries, uint32_t delayMs) {
    if (dialogId == 0u || maxRetries <= 0) {
        return false;
    }

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        DialogHook::RecordDialogSend(dialogId);
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

} // namespace GWA3::DungeonDialog
