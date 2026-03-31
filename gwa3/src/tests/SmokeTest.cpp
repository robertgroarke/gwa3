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

    // --- Section 2: Core Offsets ---
    Report("--- Core Offsets (post-processed) ---");
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

    // --- Section 3: Byte dumps around key offsets for calibration ---
    Report("--- Offset Byte Dumps (for calibration) ---");
    // These dump raw scan results BEFORE post-processing. Since post-processing
    // already ran, we need to re-scan to get the raw values. Instead, dump around
    // the post-processed values to see what's there.

    // Re-scan a few key patterns to get raw (pre-deref) addresses
    uintptr_t rawBasePointer = Scanner::Find("\x50\x6A\x0F\x6A\x00\xFF\x35", "xxxxxxx", 0);
    if (rawBasePointer) {
        Report("BasePointer raw match at 0x%08X (offset 0 from pattern)", rawBasePointer);
        DumpBytes("BasePointer +0..+16", rawBasePointer, 0, 16);
        // The pattern is: 50 6A 0F 6A 00 FF 35 <addr32>
        // FF 35 = PUSH dword ptr [imm32]. The operand is at match+7.
        uintptr_t operand = *reinterpret_cast<uint32_t*>(rawBasePointer + 7);
        Report("  FF 35 operand (BasePointer addr): 0x%08X", operand);
        if (operand > 0x10000) {
            uintptr_t value = *reinterpret_cast<uint32_t*>(operand);
            Report("  *BasePointer = 0x%08X", value);
        }
    }

    uintptr_t rawMyID = Scanner::Find("\x83\xEC\x08\x56\x8B\xF1\x3B\x15", "xxxxxxxx", 0);
    if (rawMyID) {
        Report("MyID raw match at 0x%08X", rawMyID);
        DumpBytes("MyID +0..+16", rawMyID, 0, 16);
        // Pattern: 83 EC 08 56 8B F1 3B 15 <addr32>
        // 3B 15 = CMP EDX, [imm32]. Operand at match+8.
        uintptr_t operand = *reinterpret_cast<uint32_t*>(rawMyID + 8);
        Report("  3B 15 operand (MyID addr): 0x%08X", operand);
        if (operand > 0x10000) {
            uint32_t value = *reinterpret_cast<uint32_t*>(operand);
            Report("  *MyID = %u (0x%08X)", value, value);
        }
    }

    uintptr_t rawPing = Scanner::Find("\x56\x8B\x75\x08\x89\x16\x5E", "xxxxxxx", 0);
    if (rawPing) {
        Report("Ping raw match at 0x%08X", rawPing);
        DumpBytes("Ping -4..+12", rawPing, 4, 12);
        // 89 16 = MOV [ESI], EDX. The address is loaded earlier.
        // Offset -3 from match → the scan offset. Let's check what's there.
        uintptr_t withOffset = rawPing - 3;
        Report("  Ping with offset -3: 0x%08X", withOffset);
        DumpBytes("Ping@offset", withOffset, 0, 8);
        uintptr_t operand = *reinterpret_cast<uint32_t*>(withOffset);
        Report("  Deref at offset: 0x%08X", operand);
    }

    uintptr_t rawInstanceInfo = Scanner::Find("\x6A\x2C\x50\xE8\x00\x00\x00\x00\x83\xC4\x08\xC7", "xxxx????xxxx", 0);
    if (rawInstanceInfo) {
        Report("InstanceInfo raw match at 0x%08X", rawInstanceInfo);
        DumpBytes("InstanceInfo +12..+20", rawInstanceInfo + 12, 0, 12);
        // Offset +0xE from match
        uintptr_t withOffset = rawInstanceInfo + 0xE;
        uintptr_t operand = *reinterpret_cast<uint32_t*>(withOffset);
        Report("  InstanceInfo operand at +0xE: 0x%08X", operand);
        if (operand > 0x10000) {
            uint32_t mapId = *reinterpret_cast<uint32_t*>(operand);
            Report("  *InstanceInfo (MapID?) = %u", mapId);
        }
    }
    Report("");

    // --- Section 4: Game State via post-processed Offsets ---
    Report("--- Game State (via post-processed Offsets::) ---");

    // Read MyID: Offsets::MyID is a pointer to the MyID storage
    if (Offsets::MyID > 0x10000) {
        uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);
        Report("Offsets::MyID -> *0x%08X = %u", Offsets::MyID, myId);
        Check("MyID via Offsets plausible (0-10000)", myId > 0 && myId <= 10000);
    } else {
        Report("Offsets::MyID = 0x%08X (too low)", Offsets::MyID);
    }

    // Also read via re-scan operand for comparison
    if (rawMyID) {
        uintptr_t myIdAddr = *reinterpret_cast<uint32_t*>(rawMyID + 8);
        if (myIdAddr > 0x10000) {
            uint32_t myId2 = *reinterpret_cast<uint32_t*>(myIdAddr);
            Report("Re-scan operand -> *0x%08X = %u", myIdAddr, myId2);
        }
    }

    // MapID: read via BasePointer pointer chain: *BasePointer -> +0x18 -> +0x44 -> +0x198
    if (Offsets::BasePointer > 0x10000) {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx > 0x10000) {
            uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
            if (p1 > 0x10000) {
                uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x44);
                if (p2 > 0x10000) {
                    int32_t mapId = *reinterpret_cast<int32_t*>(p2 + 0x198);
                    Report("MapID via BasePointer chain: [0x%08X]+18->[0x%08X]+44->[0x%08X]+198 = %d",
                           ctx, p1, p2, mapId);
                    Check("MapID plausible (0-2000)", mapId > 0 && mapId <= 2000);
                } else { Report("MapID chain: p2 null"); }
            } else { Report("MapID chain: p1 null"); }
        } else { Report("MapID chain: ctx null"); }
    }

    // Ping: post-processed
    if (Offsets::Ping > 0x10000) {
        uint32_t ping = *reinterpret_cast<uint32_t*>(Offsets::Ping);
        Report("Offsets::Ping -> *0x%08X = %u ms", Offsets::Ping, ping);
        Check("Ping via Offsets plausible (0-5000)", ping <= 5000);
    }

    // BasePointer -> game context
    if (Offsets::BasePointer > 0x10000) {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        Report("Offsets::BasePointer -> *0x%08X = 0x%08X", Offsets::BasePointer, ctx);
        Check("BasePointer deref valid", ctx > 0x10000);
    }

    // AgentBase
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        Report("Offsets::AgentBase -> *0x%08X = 0x%08X", Offsets::AgentBase, agentArr);
        Check("AgentBase deref valid", agentArr > 0x10000);
    }

    // SkillBase
    if (Offsets::SkillBase > 0x10000) {
        uintptr_t skillArr = *reinterpret_cast<uintptr_t*>(Offsets::SkillBase);
        Report("Offsets::SkillBase -> *0x%08X = 0x%08X", Offsets::SkillBase, skillArr);
        Check("SkillBase deref valid", skillArr > 0x10000);
    }
    Report("");

    // --- Section 5: Agent Struct Read ---
    Report("--- Agent Struct Read ---");
    // AutoIt chain: [[*AgentBase] + 4*id]
    // *AgentBase = ptr to agent ptr array (NOT GWArray)
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentPtrArray = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        Report("AgentBase deref: *0x%08X = 0x%08X (agent ptr array)", Offsets::AgentBase, agentPtrArray);

        // Dump first few entries to understand layout
        if (agentPtrArray > 0x10000) {
            Report("  [0]=0x%08X [1]=0x%08X [2]=0x%08X [3]=0x%08X",
                *reinterpret_cast<uint32_t*>(agentPtrArray),
                *reinterpret_cast<uint32_t*>(agentPtrArray + 4),
                *reinterpret_cast<uint32_t*>(agentPtrArray + 8),
                *reinterpret_cast<uint32_t*>(agentPtrArray + 12));

            // MaxAgents is at AgentBase + 8 (from AutoIt: $max_agents = $agent_base_address + 0x8)
            uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
            Report("  MaxAgents (*AgentBase+8): %u", maxAgents);

            uint32_t myId = (Offsets::MyID > 0x10000) ? *reinterpret_cast<uint32_t*>(Offsets::MyID) : 0;
            Report("  MyID = %u", myId);

            if (myId > 0 && myId < 5000 && agentPtrArray > 0x10000) {
                // Agent ptr at array[myId]
                uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentPtrArray + myId * 4);
                Report("  Agent[%u] ptr = 0x%08X", myId, agentPtr);
                if (agentPtr > 0x10000) {
                    float x = *reinterpret_cast<float*>(agentPtr + 0x74);
                    float y = *reinterpret_cast<float*>(agentPtr + 0x78);
                    float hp = *reinterpret_cast<float*>(agentPtr + 0x134);
                    uint32_t agentId = *reinterpret_cast<uint32_t*>(agentPtr + 0x2C);
                    uint32_t type = *reinterpret_cast<uint32_t*>(agentPtr + 0x9C);
                    Report("  agent_id=%u type=0x%X pos=(%.1f, %.1f) hp=%.2f",
                           agentId, type, x, y, hp);
                    Check("Agent X non-zero", x != 0.0f);
                    Check("Agent Y non-zero", y != 0.0f);
                    Check("Agent HP > 0", hp > 0.0f);
                    Check("Agent type is Living (0xDB)", type == 0xDB);
                } else {
                    Report("  Agent ptr null for id %u", myId);
                }
            }
        }
    }
    Report("");

    // --- Section 6: Frame UI ---
    Report("--- Frame UI ---");
    uintptr_t playFrame = UIMgr::GetFrameByHash(UIMgr::Hashes::PlayButton);
    Report("PlayButton frame: 0x%08X", playFrame);
    if (playFrame) {
        uint32_t frameId = UIMgr::GetFrameId(playFrame);
        uint32_t state = UIMgr::GetFrameState(playFrame);
        Report("  frame_id: %u, state: 0x%X", frameId, state);
        Check("PlayButton frame found", true);
        Check("PlayButton is created", (state & UIMgr::FRAME_CREATED) != 0);
    } else {
        Report("  Not found (may not be at char select)");
    }
    Report("");

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
