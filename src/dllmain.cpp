#include <Windows.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/Memory.h>
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
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/bot/FroggyHM.h>
#include <gwa3/core/SmokeTest.h>
#include <gwa3/utils/StringEncoding.h>
#include <gwa3/llm/LlmBridge.h>

static HMODULE g_hModule = nullptr;

static bool CheckFlag(const char* envVar, const char* flagFile) {
    char envBuf[16] = {};
    if (GetEnvironmentVariableA(envVar, envBuf, sizeof(envBuf)) > 0) return true;
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&CheckFlag), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, flagFile);
    DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        DeleteFileA(path);
        return true;
    }
    return false;
}

static uint32_t ReadMyIdRaw() {
    if (GWA3::Offsets::MyID < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(GWA3::Offsets::MyID);
}

static bool IsInGame() {
    return GWA3::MapMgr::GetMapId() > 0 && ReadMyIdRaw() > 0;
}

static bool RunCharSelectBootstrap(DWORD timeoutMs) {
    if (IsInGame()) {
        GWA3::Log::Info("Bootstrap: Already in game (MapID=%u MyID=%u)",
                        GWA3::MapMgr::GetMapId(), ReadMyIdRaw());
        return true;
    }

    const DWORD start = GetTickCount();
    DWORD lastLog = 0;
    DWORD lastPlayAttempt = 0;

    GWA3::Log::Info("Bootstrap: Entering pre-game phase");

    while (GetTickCount() - start < timeoutMs) {
        const uint32_t mapId = GWA3::MapMgr::GetMapId();
        const uint32_t myId = ReadMyIdRaw();
        if (mapId > 0 && myId > 0) {
            GWA3::Log::Info("Bootstrap: Map loaded (MapID=%u MyID=%u)", mapId, myId);
            return true;
        }

        const DWORD now = GetTickCount();
        if (now - lastLog >= 3000) {
            const uintptr_t playFrame = GWA3::UIMgr::GetFrameByHash(GWA3::UIMgr::Hashes::PlayButton);
            const uint32_t playState = playFrame ? GWA3::UIMgr::GetFrameState(playFrame) : 0;
            GWA3::Log::Info("Bootstrap: waiting (MapID=%u MyID=%u PlayFrame=0x%08X state=0x%X hb=%u)",
                            mapId, myId, playFrame, playState,
                            GWA3::RenderHook::GetHeartbeat());
            lastLog = now;
        }

        if (GWA3::UIMgr::IsFrameVisible(GWA3::UIMgr::Hashes::ReconnectYes)) {
            GWA3::Log::Info("Bootstrap: reconnect dialog visible, clicking NO (return to outpost)");
            GWA3::UIMgr::ButtonClickByHash(GWA3::UIMgr::Hashes::ReconnectNo);
            Sleep(1000);
            continue;
        }

        if (now - lastPlayAttempt >= 2500) {
            const uintptr_t playFrame = GWA3::UIMgr::GetFrameByHash(GWA3::UIMgr::Hashes::PlayButton);
            if (playFrame) {
                const uint32_t state = GWA3::UIMgr::GetFrameState(playFrame);
                const bool created = (state & GWA3::UIMgr::FRAME_CREATED) != 0;
                const bool hidden = (state & GWA3::UIMgr::FRAME_HIDDEN) != 0;
                const bool disabled = (state & GWA3::UIMgr::FRAME_DISABLED) != 0;
                if (created && !hidden && !disabled) {
                    GWA3::Log::Info("Bootstrap: clicking Play (state=0x%X)", state);
                    GWA3::UIMgr::ButtonClickByHash(GWA3::UIMgr::Hashes::PlayButton);
                    lastPlayAttempt = now;
                }
            }
        }

        Sleep(250);
    }

    GWA3::Log::Error("Bootstrap: timed out waiting for map load");
    return false;
}

static bool WaitForPlayerHydration(DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    DWORD lastLog = 0;

    while (GetTickCount() - start < timeoutMs) {
        const uint32_t mapId = GWA3::MapMgr::GetMapId();
        const uint32_t myId = ReadMyIdRaw();
        auto* me = GWA3::AgentMgr::GetMyAgent();

        float x = 0.0f;
        float y = 0.0f;
        bool havePos = false;
        uint32_t typeMap = 0;
        uint32_t modelState = 0;

        if (me) {
            x = me->x;
            y = me->y;
            havePos = !(x == 0.0f && y == 0.0f);
            typeMap = me->type_map;
            modelState = me->model_state;
        }

        const bool hydrated =
            mapId > 0 &&
            myId > 0 &&
            me != nullptr &&
            me->hp > 0.0f &&
            havePos &&
            (typeMap & 0x400000) != 0;
        if (hydrated) {
            GWA3::Log::Info("Bootstrap: Player hydrated (MapID=%u MyID=%u TypeMap=0x%X ModelState=%u Pos=(%.1f, %.1f))",
                            mapId, myId, typeMap, modelState, x, y);
            return true;
        }

        const DWORD now = GetTickCount();
        if (now - lastLog >= 2000) {
            GWA3::Log::Info("Bootstrap: waiting for player hydration (MapID=%u MyID=%u TypeMap=0x%X ModelState=%u Pos=(%.1f, %.1f))",
                            mapId, myId, typeMap, modelState, x, y);
            lastLog = now;
        }

        Sleep(250);
    }

    GWA3::Log::Warn("Bootstrap: player hydration timed out; continuing with partial readiness");
    return false;
}

DWORD WINAPI InitThread(LPVOID hModule) {
    GWA3::Log::Initialize();
    GWA3::Log::Info("gwa3.dll loaded at 0x%08X", static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(hModule)));

    bool smokeTest = CheckFlag("GWA3_SMOKE_TEST", "gwa3_smoke_test.flag");
    bool botTest = CheckFlag("GWA3_TEST_BOT", "gwa3_test_bot.flag");
    bool cmdTest = CheckFlag("GWA3_TEST_COMMANDS", "gwa3_test_commands.flag");
    bool integrationTest = CheckFlag("GWA3_TEST_INTEGRATION", "gwa3_test_integration.flag");
    bool npcDialogTest = CheckFlag("GWA3_TEST_NPC_DIALOG", "gwa3_test_npc_dialog.flag");
    bool merchantQuoteTest = CheckFlag("GWA3_TEST_MERCHANT_QUOTE", "gwa3_test_merchant_quote.flag");
    bool advancedTest = CheckFlag("GWA3_TEST_ADVANCED", "gwa3_test_advanced.flag");
    bool workflowTest = CheckFlag("GWA3_TEST_WORKFLOW", "gwa3_test_workflow.flag");
    bool froggyTest = CheckFlag("GWA3_TEST_FROGGY", "gwa3_test_froggy.flag");
    bool llmMode = CheckFlag("GWA3_LLM_MODE", "gwa3_llm_mode.flag");
    bool llmAdvisory = CheckFlag("GWA3_LLM_ADVISORY", "gwa3_llm_advisory.flag");
    bool anyTest = smokeTest || botTest || cmdTest || integrationTest || npcDialogTest || merchantQuoteTest || advancedTest || workflowTest || froggyTest;
    GWA3::Log::Info("Test flags: smoke=%d bot=%d cmd=%d integ=%d npc=%d merchant=%d advanced=%d workflow=%d froggy=%d llm=%d advisory=%d",
                    smokeTest, botTest, cmdTest, integrationTest, npcDialogTest, merchantQuoteTest, advancedTest, workflowTest, froggyTest, llmMode, llmAdvisory);

    HMODULE gwModule = GetModuleHandleA(nullptr);
    if (!GWA3::Scanner::Initialize(gwModule)) {
        GWA3::Log::Error("Scanner initialization failed - aborting");
        return 1;
    }

    if (!GWA3::Offsets::ResolveAll()) {
        GWA3::Log::Warn("Some offsets failed to resolve - continuing with partial coverage");
    }

    // Stage memory patches using resolved offsets
    if (GWA3::Offsets::CameraUpdateBypass > 0x10000) {
        const uint8_t patch[2] = {0xEB, 0x0C}; // JMP +12 (skip float copy-back)
        GWA3::Memory::GetCameraUnlockPatch().SetPatch(GWA3::Offsets::CameraUpdateBypass, patch, 2);
        GWA3::Log::Info("CameraUpdateBypass patch staged at 0x%08X", GWA3::Offsets::CameraUpdateBypass);
    }
    if (GWA3::Offsets::LevelDataBypass > 0x10000) {
        const uint8_t patch = 0xEB; // JMP (unconditional)
        GWA3::Memory::GetLevelDataBypassPatch().SetPatch(GWA3::Offsets::LevelDataBypass, &patch, 1);
        GWA3::Log::Info("LevelDataBypass patch staged at 0x%08X", GWA3::Offsets::LevelDataBypass);
    }
    if (GWA3::Offsets::MapPortBypass > 0x10000) {
        const uint8_t patch[2] = {0x90, 0x90}; // NOP NOP
        GWA3::Memory::GetMapPortBypassPatch().SetPatch(GWA3::Offsets::MapPortBypass, patch, 2);
        GWA3::Log::Info("MapPortBypass patch staged at 0x%08X", GWA3::Offsets::MapPortBypass);
    }

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
    GWA3::MemoryMgr::Initialize();
    GWA3::PlayerMgr::Initialize();
    GWA3::CameraMgr::Initialize();
    GWA3::EffectMgr::Initialize();

    if (smokeTest) {
        GWA3::Log::Info("=== SMOKE TEST MODE ===");
        int failures = GWA3::SmokeTest::RunSmokeTest();
        GWA3::Log::Info("Smoke test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    if (botTest) {
        GWA3::Log::Info("=== BOT FRAMEWORK TEST MODE ===");
        int failures = GWA3::SmokeTest::RunBotFrameworkTest();
        GWA3::Log::Info("Bot framework test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    bool gameThreadOk = GWA3::GameThread::Initialize();
    if (!gameThreadOk) {
        GWA3::Log::Warn("GameThread initialization failed — trying RenderHook fallback");
    }

    if (integrationTest || npcDialogTest || merchantQuoteTest || advancedTest || workflowTest || froggyTest || !anyTest) {
        if (!gameThreadOk) {
            // Fallback: use old render hook if GameThread failed
            if (!GWA3::RenderHook::Initialize()) {
                GWA3::Log::Error("Both GameThread and RenderHook failed - aborting");
                return 1;
            }
        }
        if (!RunCharSelectBootstrap(90000)) {
            GWA3::Log::Error("Bootstrap failed - aborting startup");
            return 1;
        }
        GWA3::RenderHook::SetMapLoaded(true);
        WaitForPlayerHydration(45000);
        GWA3::CtoS::Initialize();
        GWA3::TraderHook::Initialize();
        GWA3::TargetLogHook::Initialize();
    }

    if (!gameThreadOk) {
        GWA3::Log::Warn("GameThread initialization failed");
        if (!anyTest) {
            GWA3::Log::Error("GameThread required for bot mode - aborting");
            return 1;
        }
    } else {
        GWA3::GameThread::Enqueue([]() {
            GWA3::Log::Info("Hello from game thread! Hook is working.");
        });
        GWA3::StoC::Initialize();
        GWA3::DialogMgr::Initialize();
        GWA3::ChatLogMgr::Initialize();
        GWA3::StringEncoding::Initialize();
    }

    if (cmdTest) {
        GWA3::Log::Info("=== BEHAVIORAL COMMAND TEST MODE ===");
        int failures = GWA3::SmokeTest::RunBehavioralTest();
        GWA3::Log::Info("Behavioral test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    if (integrationTest) {
        GWA3::Log::Info("=== INTEGRATION TEST MODE ===");
        int failures = GWA3::SmokeTest::RunIntegrationTest();
        GWA3::Log::Info("Integration test complete: %d failures", failures);
#if CRASH_TEST > 0
        // In crash test modes, wait 60s after test to observe render hook stability
        GWA3::Log::Info("Test finished — CRASH_TEST=%d: waiting 60s before termination (hb=%u)...",
                        CRASH_TEST, GWA3::RenderHook::GetHeartbeat());
        Sleep(60000);
        GWA3::Log::Info("Post-test wait done (hb=%u)", GWA3::RenderHook::GetHeartbeat());
#endif
        // Terminate immediately — don't unhook (race condition with render thread)
        GWA3::Log::Info("Test finished — terminating GW process");
        GWA3::Log::Shutdown();
        Sleep(100);
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(failures));
        return static_cast<DWORD>(failures);
    }

    if (npcDialogTest) {
        GWA3::Log::Info("=== NPC/DIALOG TEST MODE ===");
        int failures = GWA3::SmokeTest::RunNpcDialogTest();
        GWA3::Log::Info("NPC/dialog test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    if (merchantQuoteTest) {
        GWA3::Log::Info("=== MERCHANT/QUOTE TEST MODE ===");
        int failures = GWA3::SmokeTest::RunMerchantQuoteTest();
        GWA3::Log::Info("Merchant/quote test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    if (advancedTest) {
        GWA3::Log::Info("=== ADVANCED INTEGRATION TEST MODE ===");
        int failures = GWA3::SmokeTest::RunAdvancedTest();
        GWA3::Log::Info("Advanced test complete: %d failures", failures);
        return static_cast<DWORD>(failures);
    }

    if (workflowTest) {
        GWA3::Log::Info("=== ADVANCED WORKFLOW TEST MODE ===");
        int failures = GWA3::SmokeTest::RunAdvancedWorkflowTest();
        GWA3::Log::Info("Workflow test complete: %d failures", failures);
        GWA3::Log::Info("Workflow test finished — terminating GW process");
        GWA3::Log::Shutdown();
        Sleep(100);
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(failures));
        return static_cast<DWORD>(failures);
    }

    if (froggyTest) {
        GWA3::Log::Info("=== FROGGY FEATURE TEST MODE ===");
        int failures = GWA3::SmokeTest::RunFroggyFeatureTest();
        GWA3::Log::Info("Froggy feature test complete: %d failures", failures);
        GWA3::Log::Info("Froggy test finished — terminating GW process");
        GWA3::Log::Shutdown();
        Sleep(100);
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(failures));
        return static_cast<DWORD>(failures);
    }

    if (llmAdvisory) {
        GWA3::Log::Info("=== LLM ADVISORY MODE (Froggy + LLM) ===");
        // Start Froggy bot first
        GWA3::Bot::Froggy::Register();
        GWA3::Bot::Start();
        // Then start LLM bridge alongside
        if (!GWA3::LLM::Initialize()) {
            GWA3::Log::Error("LLM bridge initialization failed — Froggy running solo");
        } else {
            GWA3::Log::Info("gwa3.dll initialization complete - advisory mode active");
        }
        // Keep alive while either is running
        while (GWA3::Bot::IsRunning() || GWA3::LLM::IsRunning()) {
            Sleep(1000);
        }
        return 0;
    }

    if (llmMode) {
        GWA3::Log::Info("=== LLM AGENT MODE ===");
        if (!GWA3::LLM::Initialize()) {
            GWA3::Log::Error("LLM bridge initialization failed");
            return 1;
        }
        GWA3::Log::Info("gwa3.dll initialization complete - LLM bridge active");
        while (GWA3::LLM::IsRunning()) {
            Sleep(1000);
        }
        return 0;
    }

    GWA3::Bot::Froggy::Register();
    GWA3::Bot::Start();

    GWA3::Log::Info("gwa3.dll initialization complete - bot started");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &InitThread, hModule, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        GWA3::LLM::Shutdown();
        GWA3::Bot::Stop();
        GWA3::ChatLogMgr::Shutdown();
        GWA3::DialogMgr::Shutdown();
        GWA3::StoC::Shutdown();
        GWA3::TraderHook::Shutdown();
        GWA3::TargetLogHook::Shutdown();
        GWA3::RenderHook::Shutdown();
        GWA3::GameThread::Shutdown();
        GWA3::Log::Shutdown();
    }
    return TRUE;
}
