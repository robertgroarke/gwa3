#include <Windows.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/FriendListMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/bot/FroggyHM.h>
#include <gwa3/core/SmokeTest.h>

static HMODULE g_hModule = nullptr;

DWORD WINAPI InitThread(LPVOID hModule) {
    GWA3::Log::Initialize();
    GWA3::Log::Info("gwa3.dll loaded at 0x%08X", (uintptr_t)hModule);

    // Step 1: Initialize scanner (parse GW.exe PE headers)
    HMODULE gwModule = GetModuleHandleA(nullptr);
    if (!GWA3::Scanner::Initialize(gwModule)) {
        GWA3::Log::Error("Scanner initialization failed — aborting");
        return 1;
    }

    // Step 2: Resolve all scan patterns
    if (!GWA3::Offsets::ResolveAll()) {
        GWA3::Log::Warn("Some offsets failed to resolve — continuing with partial coverage");
    }

    // Step 3: Install game thread hook
    if (!GWA3::GameThread::Initialize()) {
        GWA3::Log::Error("GameThread initialization failed — aborting");
        return 1;
    }

    // Verify game thread hook by enqueuing a test message
    GWA3::GameThread::Enqueue([]() {
        GWA3::Log::Info("Hello from game thread! Hook is working.");
    });

    // Step 4: Initialize managers
    GWA3::CtoS::Initialize();
    GWA3::AgentMgr::Initialize();
    GWA3::SkillMgr::Initialize();
    GWA3::ItemMgr::Initialize();
    GWA3::MapMgr::Initialize();
    GWA3::PartyMgr::Initialize();
    GWA3::QuestMgr::Initialize();
    GWA3::ChatMgr::Initialize();
    GWA3::TradeMgr::Initialize();
    GWA3::FriendListMgr::Initialize();
    GWA3::UIMgr::Initialize();

    // Step 5: Check for test mode flags (env vars or flag files next to DLL)
    auto CheckFlag = [](const char* envVar, const char* flagFile) -> bool {
        char envBuf[16] = {};
        if (GetEnvironmentVariableA(envVar, envBuf, sizeof(envBuf)) > 0) return true;
        // Check flag file next to DLL
        char path[MAX_PATH];
        HMODULE hSelf = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&GWA3::Log::Initialize), &hSelf);
        GetModuleFileNameA(hSelf, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        if (slash) *(slash + 1) = '\0';
        strcat_s(path, flagFile);
        DWORD attr = GetFileAttributesA(path);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            DeleteFileA(path); // consume flag
            return true;
        }
        return false;
    };

    if (CheckFlag("GWA3_SMOKE_TEST", "gwa3_smoke_test.flag")) {
        GWA3::Log::Info("=== SMOKE TEST MODE ===");
        int failures = GWA3::SmokeTest::RunSmokeTest();
        GWA3::Log::Info("Smoke test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    if (CheckFlag("GWA3_TEST_BOT", "gwa3_test_bot.flag")) {
        GWA3::Log::Info("=== BOT FRAMEWORK TEST MODE ===");
        int failures = GWA3::SmokeTest::RunBotFrameworkTest();
        GWA3::Log::Info("Bot framework test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    // Step 6: Normal mode — register and start Froggy HM bot
    GWA3::Bot::Froggy::Register();
    GWA3::Bot::Start();

    GWA3::Log::Info("gwa3.dll initialization complete — bot started");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &InitThread, hModule, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        GWA3::GameThread::Shutdown();
        GWA3::Log::Shutdown();
    }
    return TRUE;
}
