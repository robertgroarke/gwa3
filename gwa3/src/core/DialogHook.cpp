#include <gwa3/core/DialogHook.h>

#include <gwa3/core/CallbackRegistry.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>
#include <gwa3/core/HookMarker.h>
#include <gwa3/core/Offsets.h>

#include <MinHook.h>
#include <Windows.h>

namespace GWA3::DialogHook {

namespace {

using UIMessageFn = void(__cdecl*)(uint32_t messageId, void* wParam, void* lParam);
using NativeDialogFn = void(__cdecl*)(uint32_t dialogId);

static bool s_initialized = false;

static UIMessageFn s_uiMessageOriginal = nullptr;
static NativeDialogFn s_dialogOriginal = nullptr;
static NativeDialogFn s_signpostDialogOriginal = nullptr;

static uintptr_t s_dialogFnAddr = 0;
static uintptr_t s_signpostDialogFnAddr = 0;

static volatile LONG s_lastUiMessageId = 0;
static volatile LONG s_watchMessageId = 0;
static volatile LONG s_watchHitMessageId = 0;
static volatile LONG s_lastDialogId = 0;
static constexpr uint32_t kRecentUiTraceCapacity = 32u;
static volatile LONG s_recentUiMessages[kRecentUiTraceCapacity] = {};
static volatile LONG s_recentUiWriteCount = 0;

static bool IsDialogWatchAlias(uint32_t watchMessageId, uint32_t messageId) {
    if (watchMessageId != UIMSG_DIALOG) {
        return false;
    }
    // AutoIt watched 0x100000A4. On the current client path, dialog-related
    // UI traffic consistently surfaces as 0x1000005B/0x10000057 instead.
    return messageId == 0x1000005Bu || messageId == 0x10000057u;
}

static bool EnsureMinHook() {
    const MH_STATUS status = MH_Initialize();
    if (status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED) {
        return true;
    }
    Log::Warn("DialogHook: MH_Initialize failed: %s", MH_StatusToString(status));
    return false;
}

static void __cdecl UIMessageDetour(uint32_t messageId, void* wParam, void* lParam) {
    HookMarker::HookScope _hookScope(HookMarker::HookId::UIMessageDetour);
    InterlockedExchange(&s_lastUiMessageId, static_cast<LONG>(messageId));
    const LONG writeCount = InterlockedIncrement(&s_recentUiWriteCount);
    s_recentUiMessages[(writeCount - 1) % kRecentUiTraceCapacity] = static_cast<LONG>(messageId);
    const uint32_t watchMessageId = static_cast<uint32_t>(s_watchMessageId);
    if (watchMessageId == messageId || IsDialogWatchAlias(watchMessageId, messageId)) {
        InterlockedExchange(&s_watchHitMessageId, static_cast<LONG>(messageId));
    }
    CallbackRegistry::DispatchUIMessage(messageId, wParam, lParam);
    if (s_uiMessageOriginal) {
        s_uiMessageOriginal(messageId, wParam, lParam);
    }
}

static void __cdecl NativeDialogDetour(uint32_t dialogId) {
    HookMarker::HookScope _hookScope(HookMarker::HookId::SendDialogDetour);
    InterlockedExchange(&s_lastDialogId, static_cast<LONG>(dialogId));
    if (s_dialogOriginal) {
        s_dialogOriginal(dialogId);
    }
}

static void __cdecl NativeSignpostDialogDetour(uint32_t dialogId) {
    HookMarker::HookScope _hookScope(HookMarker::HookId::SendSignpostDetour);
    InterlockedExchange(&s_lastDialogId, static_cast<LONG>(dialogId));
    if (s_signpostDialogOriginal) {
        s_signpostDialogOriginal(dialogId);
    }
}

static void InstallNativeDialogHook(uintptr_t addr, void* detour, void** original, const char* label) {
    if (addr <= 0x10000 || !detour || !original) {
        return;
    }
    if (*original) {
        return;
    }
    MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(addr), detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Log::Warn("DialogHook: MH_CreateHook(%s) failed: %s", label, MH_StatusToString(status));
        return;
    }
    status = MH_EnableHook(reinterpret_cast<void*>(addr));
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Log::Warn("DialogHook: MH_EnableHook(%s) failed: %s", label, MH_StatusToString(status));
        return;
    }
    Log::Info("DialogHook: %s hook installed at 0x%08X trampoline=0x%08X",
              label,
              static_cast<unsigned>(addr),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(*original)));
}

static void RemoveHook(uintptr_t addr, void** original) {
    if (addr <= 0x10000) {
        return;
    }
    MH_DisableHook(reinterpret_cast<void*>(addr));
    MH_RemoveHook(reinterpret_cast<void*>(addr));
    if (original) {
        *original = nullptr;
    }
}

} // namespace

bool Initialize() {
    if (s_initialized) {
        return true;
    }
    CallbackRegistry::Initialize();
    if (!EnsureMinHook()) {
        return false;
    }
    if (Offsets::UIMessage <= 0x10000) {
        Log::Warn("DialogHook: UIMessage offset not resolved");
        return false;
    }

    MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(Offsets::UIMessage),
                                     reinterpret_cast<void*>(&UIMessageDetour),
                                     reinterpret_cast<void**>(&s_uiMessageOriginal));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Log::Warn("DialogHook: MH_CreateHook(UIMessage) failed: %s", MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(reinterpret_cast<void*>(Offsets::UIMessage));
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Log::Warn("DialogHook: MH_EnableHook(UIMessage) failed: %s", MH_StatusToString(status));
        return false;
    }

    InstallNativeDialogHook(s_dialogFnAddr,
                            reinterpret_cast<void*>(&NativeDialogDetour),
                            reinterpret_cast<void**>(&s_dialogOriginal),
                            "SendDialog");
    InstallNativeDialogHook(s_signpostDialogFnAddr,
                            reinterpret_cast<void*>(&NativeSignpostDialogDetour),
                            reinterpret_cast<void**>(&s_signpostDialogOriginal),
                            "SendSignpostDialog");

    Reset();
    s_initialized = true;
    Log::Info("DialogHook: Initialized (UIMessage=0x%08X)", static_cast<unsigned>(Offsets::UIMessage));
    return true;
}

void Shutdown() {
    if (!s_initialized) {
        return;
    }
    RemoveHook(Offsets::UIMessage, reinterpret_cast<void**>(&s_uiMessageOriginal));
    RemoveHook(s_dialogFnAddr, reinterpret_cast<void**>(&s_dialogOriginal));
    RemoveHook(s_signpostDialogFnAddr, reinterpret_cast<void**>(&s_signpostDialogOriginal));
    Reset();
    s_initialized = false;
    Log::Info("DialogHook: Shutdown");
}

bool IsInitialized() {
    return s_initialized;
}

void SetNativeDialogFunctions(uintptr_t dialogFn, uintptr_t signpostDialogFn) {
    s_dialogFnAddr = dialogFn;
    s_signpostDialogFnAddr = signpostDialogFn;
    if (!s_initialized) {
        return;
    }
    if (!EnsureMinHook()) {
        return;
    }
    InstallNativeDialogHook(s_dialogFnAddr,
                            reinterpret_cast<void*>(&NativeDialogDetour),
                            reinterpret_cast<void**>(&s_dialogOriginal),
                            "SendDialog");
    InstallNativeDialogHook(s_signpostDialogFnAddr,
                            reinterpret_cast<void*>(&NativeSignpostDialogDetour),
                            reinterpret_cast<void**>(&s_signpostDialogOriginal),
                            "SendSignpostDialog");
}

void RecordDialogSend(uint32_t dialogId) {
    InterlockedExchange(&s_lastDialogId, static_cast<LONG>(dialogId));
}

void StartUIHook(uint32_t messageId) {
    InterlockedExchange(&s_watchHitMessageId, 0);
    InterlockedExchange(&s_watchMessageId, static_cast<LONG>(messageId));
}

bool EndUIHook(uint32_t messageId, uint32_t timeoutMs) {
    const DWORD start = GetTickCount();
    do {
        const uint32_t hitMessageId = static_cast<uint32_t>(s_watchHitMessageId);
        if (hitMessageId == messageId || IsDialogWatchAlias(messageId, hitMessageId)) {
            return true;
        }
        Sleep(10);
    } while (GetTickCount() - start <= timeoutMs);
    return false;
}

bool WaitForUIMessage(uint32_t messageId, uint32_t timeoutMs) {
    StartUIHook(messageId);
    return EndUIHook(messageId, timeoutMs);
}

bool WaitForDialogUIMessage(uint32_t timeoutMs) {
    return WaitForUIMessage(UIMSG_DIALOG, timeoutMs);
}

uint32_t GetLastUIMessageId() {
    return static_cast<uint32_t>(s_lastUiMessageId);
}

uint32_t GetArmedUIMessageId() {
    return static_cast<uint32_t>(s_watchMessageId);
}

uint32_t GetObservedUIMessageId() {
    return static_cast<uint32_t>(s_watchHitMessageId);
}

uint32_t GetLastDialogId() {
    return static_cast<uint32_t>(s_lastDialogId);
}

void ResetRecentUITrace() {
    for (uint32_t i = 0; i < kRecentUiTraceCapacity; ++i) {
        InterlockedExchange(&s_recentUiMessages[i], 0);
    }
    InterlockedExchange(&s_recentUiWriteCount, 0);
}

uint32_t GetRecentUITrace(uint32_t* outMessages, uint32_t maxCount) {
    if (!outMessages || maxCount == 0) {
        return 0;
    }

    const LONG writeCount = InterlockedCompareExchange(&s_recentUiWriteCount, 0, 0);
    if (writeCount <= 0) {
        return 0;
    }

    const uint32_t available = writeCount < static_cast<LONG>(kRecentUiTraceCapacity)
        ? static_cast<uint32_t>(writeCount)
        : kRecentUiTraceCapacity;
    const uint32_t emit = available < maxCount ? available : maxCount;
    const uint32_t start = writeCount > static_cast<LONG>(kRecentUiTraceCapacity)
        ? static_cast<uint32_t>(writeCount - kRecentUiTraceCapacity)
        : 0u;
    for (uint32_t i = 0; i < emit; ++i) {
        const uint32_t slot = (start + i) % kRecentUiTraceCapacity;
        outMessages[i] = static_cast<uint32_t>(s_recentUiMessages[slot]);
    }
    return emit;
}

void Reset() {
    InterlockedExchange(&s_lastUiMessageId, 0);
    InterlockedExchange(&s_watchMessageId, 0);
    InterlockedExchange(&s_watchHitMessageId, 0);
    InterlockedExchange(&s_lastDialogId, 0);
    ResetRecentUITrace();
}

} // namespace GWA3::DialogHook


