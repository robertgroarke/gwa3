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

// ===== GWA3-045: Injection Smoke Test =====

int RunSmokeTest() {
    s_passed = 0;
    s_failed = 0;

    s_report = OpenReport("gwa3_smoke_report.txt");
    if (!s_report) {
        Log::Error("[SMOKE] Cannot create report file");
        return -1;
    }

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    Report("=== GWA3 Injection Smoke Test ===");
    Report("Timestamp: %s", timestamp);
    Report("");

    // --- Section 1: Scanner ---
    Report("--- Scanner ---");
    Check("Scanner initialized", Scanner::IsInitialized());
    auto text = Scanner::GetTextSection();
    auto rdata = Scanner::GetRdataSection();
    Check(".text section found", text.size > 0);
    Check(".rdata section found", rdata.size > 0);
    Report(".text: 0x%08X size %u", text.start, text.size);
    Report(".rdata: 0x%08X size %u", rdata.start, rdata.size);
    Report("");

    // --- Section 2: Core Offsets (P0) ---
    Report("--- Core Offsets (P0) ---");
    CheckNonZero("BasePointer", Offsets::BasePointer);
    CheckNonZero("PacketSend", Offsets::PacketSend);
    CheckNonZero("PacketLocation", Offsets::PacketLocation);
    CheckNonZero("AgentBase", Offsets::AgentBase);
    CheckNonZero("MyID", Offsets::MyID);
    CheckNonZero("SkillBase", Offsets::SkillBase);
    CheckNonZero("Move", Offsets::Move);
    CheckNonZero("FrameArray", Offsets::FrameArray);
    CheckNonZero("UIMessage", Offsets::UIMessage);
    CheckNonZero("SendFrameUIMsg", Offsets::SendFrameUIMsg);
    Report("Offsets resolved: %d/%d (failed: %d)",
           Offsets::GetResolvedCount(),
           Offsets::GetResolvedCount() + Offsets::GetFailedCount(),
           Offsets::GetFailedCount());
    Report("");

    // --- Section 3: Game State Reads ---
    Report("--- Game State Reads ---");

    uint32_t myId = AgentMgr::GetMyId();
    Report("Player agent ID: %u", myId);
    Check("MyID > 0", myId > 0);

    uint32_t mapId = MapMgr::GetMapId();
    Report("Map ID: %u", mapId);
    Check("MapID > 0", mapId > 0);

    uint32_t ping = ChatMgr::GetPing();
    Report("Ping: %u ms", ping);
    Check("Ping plausible (0-2000)", ping <= 2000);

    auto* me = AgentMgr::GetMyAgent();
    if (me) {
        Report("Position: (%.1f, %.1f)", me->x, me->y);
        Report("HP: %.1f%%", me->hp * 100.0f);
        Check("Player position non-zero", me->x != 0.0f || me->y != 0.0f);
        Check("Player HP > 0", me->hp > 0.0f);
    } else {
        Report("Player agent: null (may be at char select)");
        // Not a failure if at char select
    }

    // Skillbar
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (bar) {
        Report("Skillbar agent_id: %u", bar->agent_id);
        for (int i = 0; i < 8; i++) {
            Report("  Slot %d: skill %u", i, bar->skills[i].skill_id);
        }
        Check("Skillbar readable", true);
    } else {
        Report("Skillbar: null (may be at char select)");
    }

    // Inventory
    Inventory* inv = ItemMgr::GetInventory();
    if (inv) {
        uint32_t charGold = inv->gold_character;
        uint32_t storGold = inv->gold_storage;
        Report("Gold (char): %u", charGold);
        Report("Gold (storage): %u", storGold);
        Check("Inventory readable", true);

        int totalSlots = 0;
        for (int b = 0; b < 23; b++) {
            if (inv->bags[b]) totalSlots += inv->bags[b]->items_count;
        }
        Report("Total bag slots: %d", totalSlots);
    } else {
        Report("Inventory: null (may be at char select)");
    }
    Report("");

    // --- Section 4: Frame UI ---
    Report("--- Frame UI ---");
    uintptr_t playFrame = UIMgr::GetFrameByHash(UIMgr::Hashes::PlayButton);
    Report("PlayButton frame: 0x%08X", playFrame);
    if (playFrame) {
        uint32_t frameId = UIMgr::GetFrameId(playFrame);
        uint32_t state = UIMgr::GetFrameState(playFrame);
        Report("  frame_id: %u, state: 0x%X", frameId, state);
        Check("PlayButton frame found (at char select)", true);
    } else {
        Report("  Not found (may be in-game, not at char select)");
    }
    bool reconnectVisible = UIMgr::IsFrameVisible(UIMgr::Hashes::ReconnectYes);
    Report("ReconnectYes visible: %s", reconnectVisible ? "yes" : "no");
    Report("");

    // --- Summary ---
    Report("=== SUMMARY: %d passed, %d failed ===", s_passed, s_failed);
    Report("Result: %s", s_failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");

    fclose(s_report);
    s_report = nullptr;

    Log::Info("[SMOKE] Complete: %d passed, %d failed", s_passed, s_failed);
    return s_failed;
}

// ===== GWA3-048: Bot Framework Smoke Test =====

} // namespace GWA3::SmokeTest
