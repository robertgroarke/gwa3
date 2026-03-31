#include <Windows.h>
#include <gwa3/core/Log.h>

static HMODULE g_hModule = nullptr;

DWORD WINAPI InitThread(LPVOID hModule) {
    GWA3::Log::Initialize();
    GWA3::Log::Info("gwa3.dll loaded at 0x%08X", (uintptr_t)hModule);
    GWA3::Log::Info("Waiting for scanner and hooks to be implemented...");
    // Future: Scanner::Initialize(), GameThread::Initialize(), Bot::Start()
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &InitThread, hModule, 0, nullptr);
    }
    return TRUE;
}
