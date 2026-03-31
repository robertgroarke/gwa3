#include <gwa3/core/SmokeTest.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <atomic>

namespace GWA3::SmokeTest {

static FILE* s_botReport = nullptr;
static int s_botPassed = 0;
static int s_botFailed = 0;

static void BotReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Log::Info("[BOT-TEST] %s", buf);
    if (s_botReport) {
        fprintf(s_botReport, "%s\n", buf);
        fflush(s_botReport);
    }
}

static void BotCheck(const char* name, bool condition) {
    if (condition) {
        s_botPassed++;
        BotReport("[PASS] %s", name);
    } else {
        s_botFailed++;
        BotReport("[FAIL] %s", name);
    }
}

static FILE* OpenBotReport() {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&OpenBotReport), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "gwa3_bot_test_report.txt");

    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    return f;
}

int RunBotFrameworkTest() {
    s_botPassed = 0;
    s_botFailed = 0;

    s_botReport = OpenBotReport();
    if (!s_botReport) {
        Log::Error("[BOT-TEST] Cannot create report file");
        return -1;
    }

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    BotReport("=== GWA3 Bot Framework Smoke Test ===");
    BotReport("Timestamp: %s", timestamp);
    BotReport("");

    // --- Test 1: Initial state ---
    BotReport("--- Test 1: Initial State ---");
    BotCheck("Bot not running initially", !Bot::IsRunning());
    BotCheck("Initial state is Idle", Bot::GetState() == Bot::BotState::Idle);
    BotReport("");

    // --- Test 2: State handler registration ---
    BotReport("--- Test 2: State Handler Registration ---");
    std::atomic<int> transitionCount{0};
    Bot::BotState lastState = Bot::BotState::Idle;

    // Register test handlers that cycle: Idle -> CharSelect -> InTown -> Stopping
    Bot::RegisterStateHandler(Bot::BotState::Idle, [&](Bot::BotConfig&) -> Bot::BotState {
        transitionCount++;
        return Bot::BotState::CharSelect;
    });
    Bot::RegisterStateHandler(Bot::BotState::CharSelect, [&](Bot::BotConfig&) -> Bot::BotState {
        transitionCount++;
        return Bot::BotState::InTown;
    });
    Bot::RegisterStateHandler(Bot::BotState::InTown, [&](Bot::BotConfig&) -> Bot::BotState {
        transitionCount++;
        lastState = Bot::BotState::InTown;
        return Bot::BotState::Stopping;
    });
    BotCheck("Handlers registered without crash", true);
    BotReport("");

    // --- Test 3: Thread lifecycle ---
    BotReport("--- Test 3: Thread Lifecycle ---");
    Bot::Start();
    Sleep(100);
    BotCheck("Bot::IsRunning() after Start()", Bot::IsRunning());

    // Wait for state machine to cycle through
    DWORD start = GetTickCount();
    while (Bot::IsRunning() && (GetTickCount() - start) < 5000) {
        Sleep(100);
    }

    BotCheck("Bot thread exited within 5s", !Bot::IsRunning());
    BotCheck("Transition count >= 3", transitionCount.load() >= 3);
    BotCheck("Final handler reached InTown", lastState == Bot::BotState::InTown);
    BotReport("Transitions fired: %d", transitionCount.load());
    BotReport("");

    // --- Test 4: Config defaults ---
    BotReport("--- Test 4: Config Defaults ---");
    auto& cfg = Bot::GetConfig();
    BotCheck("hard_mode default", cfg.hard_mode == true);
    BotCheck("bot_module_name non-empty", !cfg.bot_module_name.empty());
    BotReport("bot_module_name: %s", cfg.bot_module_name.c_str());
    BotReport("");

    // --- Test 5: Error handler ---
    BotReport("--- Test 5: Error Recovery ---");
    std::atomic<bool> errorReached{false};

    // Register handler that throws, plus error handler
    Bot::RegisterStateHandler(Bot::BotState::Idle, [](Bot::BotConfig&) -> Bot::BotState {
        // Deliberate crash to test SEH — dereference null
        // Actually, we can't safely do this in all builds.
        // Instead test that a handler returning Error transitions correctly.
        return Bot::BotState::Error;
    });
    Bot::RegisterStateHandler(Bot::BotState::Error, [&](Bot::BotConfig&) -> Bot::BotState {
        errorReached = true;
        return Bot::BotState::Stopping;
    });

    Bot::SetState(Bot::BotState::Idle);
    Bot::Start();
    Sleep(500);

    // Wait for it to stop
    start = GetTickCount();
    while (Bot::IsRunning() && (GetTickCount() - start) < 5000) {
        Sleep(100);
    }

    BotCheck("Error handler was reached", errorReached.load());
    BotCheck("Bot stopped after error recovery", !Bot::IsRunning());
    BotReport("");

    // --- Test 6: Stop() is safe to call multiple times ---
    BotReport("--- Test 6: Safe Stop ---");
    Bot::Stop(); // Already stopped
    Bot::Stop(); // Double stop
    BotCheck("Double Stop() does not crash", true);
    BotReport("");

    // --- Summary ---
    BotReport("=== SUMMARY: %d passed, %d failed ===", s_botPassed, s_botFailed);
    BotReport("Result: %s", s_botFailed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");

    fclose(s_botReport);
    s_botReport = nullptr;

    Log::Info("[BOT-TEST] Complete: %d passed, %d failed", s_botPassed, s_botFailed);
    return s_botFailed;
}

} // namespace GWA3::SmokeTest
