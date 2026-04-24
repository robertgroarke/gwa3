#pragma once

#include <cstdint>

namespace GWA3::Bot::DungeonDialog {

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

} // namespace GWA3::Bot::DungeonDialog
