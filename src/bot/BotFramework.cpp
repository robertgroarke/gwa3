#include <gwa3/bot/BotFramework.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <map>

namespace GWA3::Bot {

static HANDLE s_thread = nullptr;
static volatile bool s_running = false;
static volatile bool s_stopRequested = false;
static BotState s_state = BotState::Idle;
static BotConfig s_config{};
static std::mutex s_mutex;
static FILE* s_logFile = nullptr;

static std::map<BotState, StateHandler> s_handlers;

static const char* StateToString(BotState state) {
    switch (state) {
        case BotState::Idle:        return "Idle";
        case BotState::CharSelect:  return "CharSelect";
        case BotState::InTown:      return "InTown";
        case BotState::Traveling:   return "Traveling";
        case BotState::InDungeon:   return "InDungeon";
        case BotState::Looting:     return "Looting";
        case BotState::Merchant:    return "Merchant";
        case BotState::Maintenance: return "Maintenance";
        case BotState::Error:       return "Error";
        case BotState::Stopping:      return "Stopping";
        case BotState::LLMControlled: return "LLMControlled";
        default:                      return "Unknown";
    }
}

static void InitBotLog() {
    if (s_logFile) return;
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&InitBotLog), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "gwa3_bot.log");
    fopen_s(&s_logFile, path, "a");
}

void LogBot(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(s_mutex);

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tm_buf);

    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    char line[2200];
    snprintf(line, sizeof(line), "[%s] [BOT] %s\n", timestamp, message);

    if (s_logFile) {
        fputs(line, s_logFile);
        fflush(s_logFile);
    }
    printf("%s", line);
    OutputDebugStringA(line);
}

// Exception-safe handler invocation using C++ try/catch
static BotState InvokeHandlerSEH(StateHandler* handler, BotConfig* config, BotState current) {
    BotState next = BotState::Error;
    try {
        next = (*handler)(*config);
    } catch (...) {
        LogBot("EXCEPTION in state %s handler", StateToString(current));
        next = BotState::Error;
    }
    return next;
}

static DWORD WINAPI BotThreadProc(LPVOID) {
    InitBotLog();
    LogBot("Bot thread started, state=%s", StateToString(s_state));

    while (!s_stopRequested) {
        BotState current = s_state;

        if (current == BotState::Stopping) break;

        // Find handler for current state
        StateHandler handler = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            auto it = s_handlers.find(current);
            if (it != s_handlers.end()) {
                handler = it->second;
            }
        }

        if (handler) {
            BotState next = InvokeHandlerSEH(&handler, &s_config, current);

            if (next != current) {
                LogBot("State transition: %s -> %s", StateToString(current), StateToString(next));
                s_state = next;
            }
        } else {
            // No handler registered — sleep briefly and retry
            Sleep(100);
        }

        // Yield between iterations
        Sleep(50);
    }

    LogBot("Bot thread exiting");
    s_running = false;
    return 0;
}

void Start() {
    if (s_running) return;

    s_stopRequested = false;
    s_state = BotState::Idle;
    s_running = true;

    s_thread = CreateThread(nullptr, 0, &BotThreadProc, nullptr, 0, nullptr);
    if (!s_thread) {
        s_running = false;
        Log::Error("Bot: Failed to create bot thread");
    } else {
        Log::Info("Bot: Started");
    }
}

void Stop() {
    if (!s_running) return;

    LogBot("Stop requested");
    s_stopRequested = true;
    s_state = BotState::Stopping;

    if (s_thread) {
        WaitForSingleObject(s_thread, 5000);
        CloseHandle(s_thread);
        s_thread = nullptr;
    }

    s_running = false;

    if (s_logFile) {
        fclose(s_logFile);
        s_logFile = nullptr;
    }

    Log::Info("Bot: Stopped");
}

bool IsRunning()   { return s_running; }
BotState GetState() { return s_state; }

void SetState(BotState state) {
    if (state != s_state) {
        LogBot("State set: %s -> %s", StateToString(s_state), StateToString(state));
        s_state = state;
    }
}

void RegisterStateHandler(BotState state, StateHandler handler) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_handlers[state] = std::move(handler);
}

BotConfig& GetConfig() { return s_config; }

void LoadConfigFromIni(const char* iniPath) {
    // Load basic settings from INI
    s_config.hard_mode = (GetPrivateProfileIntA("Settings", "HardMode", 1, iniPath) != 0);
    s_config.use_consets = (GetPrivateProfileIntA("Settings", "Consets", 0, iniPath) != 0);
    s_config.use_stones = (GetPrivateProfileIntA("Settings", "Stones", 0, iniPath) != 0);
    s_config.disable_rendering = (GetPrivateProfileIntA("Settings", "DisableRendering", 0, iniPath) != 0);
    s_config.open_chests = (GetPrivateProfileIntA("Settings", "OpenChests", 1, iniPath) != 0);
    s_config.pickup_golds = (GetPrivateProfileIntA("Settings", "PickUpGolds", 1, iniPath) != 0);
    s_config.auto_salvage = (GetPrivateProfileIntA("Settings", "AutoSalvage", 1, iniPath) != 0);
    s_config.use_experimental_autoit_merchant_lane =
        (GetPrivateProfileIntA("Settings", "ExperimentalAutoItMerchantLane", 0, iniPath) != 0);
    s_config.target_map_id = static_cast<uint32_t>(GetPrivateProfileIntA("Settings", "TargetMapID", 0, iniPath));
    s_config.outpost_map_id = static_cast<uint32_t>(GetPrivateProfileIntA("Settings", "OutpostMapID", 0, iniPath));

    char buf[256];
    GetPrivateProfileStringA("Settings", "HeroConfig", "Standard.txt", buf, sizeof(buf), iniPath);
    s_config.hero_config_file = buf;
    GetPrivateProfileStringA("Settings", "BotModule", "FroggyHM", buf, sizeof(buf), iniPath);
    s_config.bot_module_name = buf;

    // Load hero IDs
    for (int i = 0; i < 7; i++) {
        char key[32];
        snprintf(key, sizeof(key), "Hero%d", i + 1);
        s_config.hero_ids[i] = static_cast<uint32_t>(GetPrivateProfileIntA("Heroes", key, 0, iniPath));
    }

    LogBot("Config loaded from %s (module=%s, hardmode=%d, consets=%d)",
           iniPath, s_config.bot_module_name.c_str(), s_config.hard_mode, s_config.use_consets);
}

} // namespace GWA3::Bot
