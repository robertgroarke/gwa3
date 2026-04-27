#pragma once

#include <cstdint>

namespace GWA3::DialogHook {

constexpr uint32_t UIMSG_DIALOG = 0x100000A4u;

bool Initialize();
void Shutdown();
bool IsInitialized();

void SetNativeDialogFunctions(uintptr_t dialogFn, uintptr_t signpostDialogFn);
void RecordDialogSend(uint32_t dialogId);

void StartUIHook(uint32_t messageId = UIMSG_DIALOG);
bool EndUIHook(uint32_t messageId, uint32_t timeoutMs = 2000u);
bool WaitForUIMessage(uint32_t messageId, uint32_t timeoutMs = 2000u);
bool WaitForDialogUIMessage(uint32_t timeoutMs = 2000u);

uint32_t GetLastUIMessageId();
uint32_t GetArmedUIMessageId();
uint32_t GetObservedUIMessageId();
uint32_t GetLastDialogId();
void ResetRecentUITrace();
uint32_t GetRecentUITrace(uint32_t* outMessages, uint32_t maxCount);
void Reset();

} // namespace GWA3::DialogHook
