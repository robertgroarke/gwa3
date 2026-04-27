#include <Windows.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/CrashDiag.h>
#include <gwa3/core/HookMarker.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/RuntimeWatchdog.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/core/Memory.h>
#include <gwa3/packets/CtoSHook.h>
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
#include <bots/common/BotFramework.h>
#include <bots/froggy/FroggyHM.h>
#include <gwa3/utils/StringEncoding.h>
#include <gwa3/utils/EncStringCache.h>
#include <gwa3/llm/LlmBridge.h>
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/GameSnapshot.h>

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

static bool ClickPlayButtonMouseFallback() {
    HWND hwnd = static_cast<HWND>(GWA3::MemoryMgr::GetGWWindowHandle());
    if (!hwnd || !IsWindow(hwnd)) {
        GWA3::Log::Warn("Bootstrap: mouse Play fallback has no valid GW hwnd");
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        GWA3::Log::Warn("Bootstrap: mouse Play fallback failed to read window rect");
        return false;
    }

    const LONG width = rect.right - rect.left;
    const LONG height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        GWA3::Log::Warn("Bootstrap: mouse Play fallback invalid window size %ldx%ld", width, height);
        return false;
    }

    const LONG clickX = rect.left + static_cast<LONG>(width * 0.78f);
    const LONG clickY = rect.top + static_cast<LONG>(height * 0.96f);

    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);
    Sleep(100);

    POINT oldPos{};
    GetCursorPos(&oldPos);
    SetCursorPos(clickX, clickY);
    Sleep(50);

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    const UINT sent = SendInput(2, inputs, sizeof(INPUT));

    Sleep(50);
    SetCursorPos(oldPos.x, oldPos.y);

    GWA3::Log::Info("Bootstrap: mouse-clicking Play fallback at (%ld,%ld), sent=%u", clickX, clickY, sent);
    return sent == 2;
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
            const uintptr_t playGreyFrame = GWA3::UIMgr::GetFrameByHash(GWA3::UIMgr::Hashes::PlayButtonGreyed);
            const uint32_t playState = playFrame ? GWA3::UIMgr::GetFrameState(playFrame) : 0;
            const uint32_t playGreyState = playGreyFrame ? GWA3::UIMgr::GetFrameState(playGreyFrame) : 0;
            GWA3::Log::Info("Bootstrap: waiting (MapID=%u MyID=%u PlayFrame=0x%08X state=0x%X GreyFrame=0x%08X grey=0x%X hb=%u)",
                            mapId, myId, playFrame, playState, playGreyFrame, playGreyState,
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
            const uintptr_t playCandidates[] = {
                GWA3::UIMgr::GetFrameByHash(GWA3::UIMgr::Hashes::PlayButton),
                GWA3::UIMgr::GetFrameByHash(GWA3::UIMgr::Hashes::PlayButtonGreyed)
            };
            const uint32_t playHashes[] = {
                GWA3::UIMgr::Hashes::PlayButton,
                GWA3::UIMgr::Hashes::PlayButtonGreyed
            };
            for (int i = 0; i < 2; ++i) {
                const uintptr_t playFrame = playCandidates[i];
                if (!playFrame) continue;
                const uint32_t state = GWA3::UIMgr::GetFrameState(playFrame);
                const bool created = (state & GWA3::UIMgr::FRAME_CREATED) != 0;
                const bool hidden = (state & GWA3::UIMgr::FRAME_HIDDEN) != 0;
                const bool disabled = (state & GWA3::UIMgr::FRAME_DISABLED) != 0;
                if (created && !hidden && !disabled) {
                    GWA3::Log::Info("Bootstrap: clicking Play hash=0x%X (state=0x%X)", playHashes[i], state);
                    GWA3::UIMgr::ButtonClickByHash(playHashes[i]);
                    lastPlayAttempt = now;
                    break;
                }
                // Some char-select states expose only a "greyed" or hidden-tagged Play frame
                // even though clicking it still advances to map load. Fall back to trying it.
                if (created && (playHashes[i] == GWA3::UIMgr::Hashes::PlayButtonGreyed || state == 0x4B14 || state == 0x4314)) {
                    GWA3::Log::Info("Bootstrap: force-clicking Play hash=0x%X (state=0x%X)", playHashes[i], state);
                    GWA3::UIMgr::ButtonClickByHash(playHashes[i]);
                    Sleep(250);
                    if (!IsInGame()) {
                        ClickPlayButtonMouseFallback();
                    }
                    lastPlayAttempt = now;
                    break;
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

    // Install the top-level unhandled-exception filter and VEH *first* so
    // that any crash during subsequent hook installation or later runtime
    // writes a real exception-stream minidump (EIP, stack, registers, and
    // the faulting module offset) to the log. Without this, the watchdog's
    // post-dialog CaptureProcessState captures only the process state
    // while GW's crash dialog is already up â€” no exception data.
    GWA3::CrashDiag::Initialize();
    GWA3::HookMarker::SelfTest();

    bool llmMode = CheckFlag("GWA3_LLM_MODE", "gwa3_llm_mode.flag");
    bool llmAdvisory = CheckFlag("GWA3_LLM_ADVISORY", "gwa3_llm_advisory.flag");
    GWA3::Log::Info("Mode flags: llm=%d advisory=%d", llmMode, llmAdvisory);

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

    bool gameThreadOk = false;

    if (!GWA3::RenderHook::Initialize()) {
        GWA3::Log::Error("RenderHook failed - aborting");
        return 1;
    }
    if (!RunCharSelectBootstrap(90000)) {
        GWA3::Log::Error("Bootstrap failed - aborting startup");
        return 1;
    }
    GWA3::RenderHook::SetMapLoaded(true);
    WaitForPlayerHydration(45000);

    gameThreadOk = GWA3::GameThread::Initialize();

    GWA3::CtoS::Initialize();
    GWA3::TraderHook::Initialize();
    GWA3::TargetLogHook::Initialize();
    GWA3::DialogHook::Initialize();

    if (!gameThreadOk) {
        GWA3::Log::Warn("GameThread initialization failed");
        if (!llmMode && !llmAdvisory) {
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
        // EncStringCache installs a passive MinHook detour on
        // ValidateAsyncDecodeStr. That hook is currently unstable on the
        // Reforged client and has been the direct cause of advisory/LLM
        // runtime crashes during normal UI decode traffic. Nothing in the
        // live bridge currently requires this cache to function, so keep it
        // disabled by default until the hook path is repaired.
        if (llmMode || llmAdvisory) {
            GWA3::Log::Warn("EncStringCache disabled: ValidateAsyncDecodeStr hook is crash-prone on this client");
        }
    }

    if (llmAdvisory) {
        GWA3::Log::Info("=== LLM ADVISORY MODE (Froggy + LLM) ===");
        GWA3::RuntimeWatchdog::Start();
        GWA3::Bot::Froggy::Register();
        GWA3::Bot::Start();
        GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::InTown
                                       : GWA3::Bot::BotState::CharSelect);
        if (!GWA3::LLM::Initialize()) {
            GWA3::Log::Error("LLM bridge initialization failed; Froggy running solo");
        } else {
            GWA3::Log::Info("gwa3.dll initialization complete - advisory mode active");
        }
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

        // Run everything on the init thread: snapshot serialization + action dispatch.
        // The bridge thread was causing game crashes when any game action was dispatched
        // while a pipe client was connected â€” even with EventPush disabled and snapshots
        // paused. Running all work on the init thread (proven safe) avoids the issue.
        // Run actions from init thread. Bridge thread handles snapshots normally.
        GWA3::Log::Info("[LLM] Init thread polling for actions");
        while (GWA3::LLM::IsRunning()) {
            GWA3::LLM::DrainInboundActions();
            Sleep(50);
        }
        return 0;
    }

    GWA3::Bot::Froggy::Register();
    GWA3::RuntimeWatchdog::Start();
    GWA3::Bot::Start();
    GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::InTown
                                   : GWA3::Bot::BotState::CharSelect);

    GWA3::Log::Info("gwa3.dll initialization complete - bot started");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &InitThread, hModule, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        // During process termination, Windows may tear down CRT/static state
        // while we are still under the loader lock. Running full hook/thread/
        // STL cleanup here has been causing detach-time AVs inside gwa3.dll.
        // Let the OS reclaim our process resources instead.
        if (reserved != nullptr) {
            return TRUE;
        }
        GWA3::LLM::Shutdown();
        GWA3::Bot::Stop();
        GWA3::RuntimeWatchdog::Stop(false);
        GWA3::EncStringCache::Shutdown();
        GWA3::ChatLogMgr::Shutdown();
        GWA3::DialogMgr::Shutdown();
        GWA3::DialogHook::Shutdown();
        GWA3::StoC::Shutdown();
        GWA3::CtoSHook::Shutdown();
        GWA3::TraderHook::Shutdown();
        GWA3::TargetLogHook::Shutdown();
        GWA3::RenderHook::Shutdown();
        GWA3::GameThread::Shutdown();
        GWA3::CrashDiag::Shutdown();
        GWA3::Log::Shutdown();
    }
    return TRUE;
}
