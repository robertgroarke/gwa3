#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/UIMgr.h>

#include <Windows.h>
#include <cstdio>
#include <ctime>

namespace GWA3::SmokeTest {

static FILE* s_report = nullptr;
static int s_passed = 0;
static int s_failed = 0;

static void Report(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Log::Info("[SMOKE] %s", buf);
    if (s_report) {
        fprintf(s_report, "%s\n", buf);
        fflush(s_report);
    }
}

static void Check(const char* name, bool condition) {
    if (condition) {
        s_passed++;
        Report("[PASS] %s", name);
    } else {
        s_failed++;
        Report("[FAIL] %s", name);
    }
}

static void CheckNonZero(const char* name, uintptr_t value) {
    if (value != 0) {
        s_passed++;
        Report("[PASS] %s = 0x%08X", name, value);
    } else {
        s_failed++;
        Report("[FAIL] %s = 0 (expected non-zero)", name);
    }
}

static FILE* OpenReport(const char* filename) {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&OpenReport), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, filename);

    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    return f;
}

// Dump hex bytes around an address for debugging
static void DumpBytes(const char* label, uintptr_t addr, int before, int after) {
    if (addr < 0x10000) {
        Report("  %s: address 0x%08X too low to dump", label, addr);
        return;
    }
    const uint8_t* p = reinterpret_cast<const uint8_t*>(addr - before);
    char hex[512] = {};
    int pos = 0;
    for (int i = 0; i < before + after; i++) {
        if (i == before) {
            pos += snprintf(hex + pos, sizeof(hex) - pos, "[%02X]", p[i]);
        } else {
            pos += snprintf(hex + pos, sizeof(hex) - pos, " %02X", p[i]);
        }
    }
    Report("  %s @ 0x%08X: %s", label, addr, hex);
}

#include "SmokeTestCoreSections.h"
#include "SmokeTestOffsetCalibration.h"
#include "SmokeTestGameStateSection.h"
#include "SmokeTestAgentStructSection.h"
#include "SmokeTestGameThreadAssertionSection.h"

int RunSmokeTest() {
    s_passed = 0;
    s_failed = 0;

    s_report = OpenReport("gwa3_smoke_report.txt");

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    Report("=== GWA3 Injection Smoke Test ===");
    Report("Timestamp: %s", timestamp);
    Report("");

    RunScannerSmokeSection();
    RunCoreOffsetsSmokeSection();

    const auto rawMatches = RunOffsetCalibrationSmokeSection();
    RunGameStateSmokeSection(rawMatches);
    RunAgentStructSmokeSection();

    RunFrameUiSmokeSection();

    RunGameThreadAssertionSmokeSection();

    // --- Summary ---
    Report("=== SUMMARY: %d passed, %d failed ===", s_passed, s_failed);

    if (s_report) {
        fclose(s_report);
        s_report = nullptr;
    }

    Log::Info("[SMOKE] Complete: %d passed, %d failed", s_passed, s_failed);
    return s_failed;
}

} // namespace GWA3::SmokeTest
