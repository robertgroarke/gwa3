#pragma once

#include <cstdint>

namespace GWA3::DungeonDialog {

struct DialogSnapshot {
    bool dialog_open = false;
    uint32_t sender_agent_id = 0u;
    uint32_t button_count = 0u;
    uint32_t last_dialog_id = 0u;
};

struct DialogAdvanceOptions {
    uint32_t sender_agent_id = 0u;
    uint32_t request_quest_id_after_dialog = 0u;
    uint32_t post_target_wait_ms = 150u;
    uint32_t post_dialog_wait_ms = 750u;
    uint32_t post_request_wait_ms = 250u;
    uint32_t max_buttons_per_pass = 16u;
    int max_passes = 4;
    bool change_target_before_dialog = false;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

DialogSnapshot CaptureDialogSnapshot();
bool HasDialogButton(uint32_t dialogId);
bool IsDialogOpenFromSenderWithButton(uint32_t senderAgentId, uint32_t dialogId);
void LogDialogButtons(const char* log_prefix = nullptr, const char* label = nullptr);
bool AdvanceDialogToButton(uint32_t targetDialogId, const DialogAdvanceOptions& options = {});
bool SendDialogWithRetry(uint32_t dialogId, int maxRetries = 2, uint32_t delayMs = 500u);
bool SendDialogSequence(
    const uint32_t* dialogIds,
    int count,
    uint32_t delayMs = 500u,
    int maxRetriesPerDialog = 1);
bool SendDialogSequenceRepeated(
    const uint32_t* dialogIds,
    int count,
    int sequenceRepeats,
    uint32_t delayMs = 500u,
    uint32_t repeatDelayMs = 1000u,
    int maxRetriesPerDialog = 1);

} // namespace GWA3::DungeonDialog
