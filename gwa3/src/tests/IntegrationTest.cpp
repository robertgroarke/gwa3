// Integration test coordinator.
// Runs the full test session in one DLL injection and delegates individual slices
// to the extracted IntegrationTest*.cpp modules.

#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include "IntegrationTestInternal.h"

#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <atomic>
#include <functional>

namespace GWA3::SmokeTest {

// Forward declare
int RunIntegrationTest();
static FILE* s_intReport = nullptr;
static int s_intPassed = 0;
static int s_intFailed = 0;
static int s_intSkipped = 0;

// --- Crash detection watchdog ---
static volatile bool s_watchdogRunning = false;
static volatile bool s_crashDetected = false;
static HANDLE s_watchdogThread = nullptr;

static volatile bool s_disconnectDetected = false;
static uint32_t s_watchdogLastMapId = 0;
static constexpr bool kBisectWorkflowStopAfterEarlyPhase = false;
static constexpr bool kBisectWorkflowStopAfter074c = false;
static constexpr bool kBisectWorkflowSkipPostStoCTail = false;
static constexpr bool kBisectWorkflowOnlyQuestTail = false;
static constexpr bool kBisectWorkflowOnlyUiTail = false;
static constexpr bool kBisectWorkflowOnlyAgentTail = false;

static DWORD WINAPI WatchdogThread(LPVOID) {
    uint32_t lastHeartbeat = RenderHook::GetHeartbeat();
    int stallCount = 0;
    s_watchdogLastMapId = MapMgr::GetMapId();

    while (s_watchdogRunning) {
        Sleep(1000);

        // --- Crash detection: render heartbeat stall ---
        uint32_t hb = RenderHook::GetHeartbeat();
        if (hb == lastHeartbeat && hb > 0) {
            stallCount++;
            if (stallCount >= 3) {
                bool hookIntact = RenderHook::IsHookIntact();
                Log::Error("[WATCHDOG] !!! RENDER FROZEN — heartbeat stuck at %u for >3s. GW likely crashed !!!", hb);
                Log::Error("[WATCHDOG] Hook JMP intact: %s", hookIntact ? "YES" : "NO — OVERWRITTEN!");
                Log::Error("[WATCHDOG] Last known test state: %d passed, %d failed, %d skipped",
                           s_intPassed, s_intFailed, s_intSkipped);
                s_crashDetected = true;
#if CRASH_TEST == 0
                if (s_intReport) { fflush(s_intReport); }
                Log::Error("[WATCHDOG] Terminating GW process...");
                TerminateProcess(GetCurrentProcess(), 0xDEAD);
#else
                Log::Error("[WATCHDOG] (CRASH_TEST mode — NOT killing, continuing observation)");
                stallCount = 0;
#endif
            }
        } else {
            stallCount = 0;
        }
        lastHeartbeat = hb;

        // --- Crash dialog detection: GW window stops responding ---
        {
            HWND gwHwnd = static_cast<HWND>(MemoryMgr::GetGWWindowHandle());
            if (gwHwnd) {
                DWORD_PTR result = 0;
                LRESULT lr = SendMessageTimeoutA(gwHwnd, WM_NULL, 0, 0,
                                                  SMTO_ABORTIFHUNG, 2000, &result);
                if (lr == 0 && GetLastError() != 0) {
                    // Window is hung — likely crash dialog
                    Log::Error("[WATCHDOG] !!! GW WINDOW NOT RESPONDING — crash dialog likely visible !!!");
                    Log::Error("[WATCHDOG] Test state: %d passed, %d failed, %d skipped",
                               s_intPassed, s_intFailed, s_intSkipped);
                    s_crashDetected = true;
                    if (s_intReport) { fflush(s_intReport); }
                    Log::Error("[WATCHDOG] Terminating hung GW process...");
                    Log::Shutdown();
                    Sleep(100);
                    TerminateProcess(GetCurrentProcess(), 0xDEAD);
                }
            }
        }

        // --- Disconnect detection: MapID drops to 0 or charselect appears ---
        uint32_t currentMapId = MapMgr::GetMapId();
        if (s_watchdogLastMapId > 0 && currentMapId == 0) {
            Log::Error("[WATCHDOG] !!! DISCONNECT DETECTED — MapID dropped from %u to 0 !!!",
                       s_watchdogLastMapId);
            Log::Error("[WATCHDOG] Test state at disconnect: %d passed, %d failed, %d skipped",
                       s_intPassed, s_intFailed, s_intSkipped);
            s_disconnectDetected = true;

            if (s_intReport) { fflush(s_intReport); }
            Log::Error("[WATCHDOG] Terminating GW process after disconnect...");
            Log::Shutdown();
            Sleep(100);
            TerminateProcess(GetCurrentProcess(), 0xDC);
        }
        if (currentMapId > 0) {
            s_watchdogLastMapId = currentMapId;
        }
    }
    return 0;
}

static void StartWatchdog() {
    s_watchdogRunning = true;
    s_crashDetected = false;
    s_disconnectDetected = false;
    s_watchdogLastMapId = 0;
    s_watchdogThread = CreateThread(nullptr, 0, WatchdogThread, nullptr, 0, nullptr);
}

static void StopWatchdog() {
    s_watchdogRunning = false;
    if (s_watchdogThread) {
        WaitForSingleObject(s_watchdogThread, 5000);
        CloseHandle(s_watchdogThread);
        s_watchdogThread = nullptr;
    }
}

static bool ShouldAbortForRuntimeFailure() {
    return s_crashDetected || s_disconnectDetected;
}

static bool AbortWorkflowIfRuntimeFailed(const char* afterStep) {
    if (!ShouldAbortForRuntimeFailure()) {
        return false;
    }

    const char* reason = s_disconnectDetected ? "disconnect detected" : "crash detected";
    IntReport("[FAIL] Workflow aborted after %s — %s", afterStep, reason);
    s_intFailed++;
    return true;
}

void IntReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log::Info("[INTG] %s", buf);
    if (s_intReport) {
        fprintf(s_intReport, "%s\n", buf);
        fflush(s_intReport);
    }
}

void IntCheck(const char* name, bool condition) {
    if (condition) {
        s_intPassed++;
        IntReport("[PASS] %s", name);
    } else {
        s_intFailed++;
        IntReport("[FAIL] %s", name);
    }
}

void IntSkip(const char* name, const char* reason) {
    s_intSkipped++;
    IntReport("[SKIP] %s — %s", name, reason);
}

static FILE* OpenIntReport() {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&OpenIntReport), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "gwa3_integration_report.txt");
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    return f;
}

static uint32_t NormalizeSkillEnergyCost(uint8_t rawCost) {
    switch (rawCost) {
    case 11: return 15;
    case 12: return 25;
    default: return rawCost;
    }
}

uint32_t GetCurrentEnergyPoints() {
    auto* me = GetAgentLivingRaw(ReadMyId());
    if (!me) return 0;

    const float clampedEnergy = (me->energy < 0.0f) ? 0.0f : me->energy;
    return static_cast<uint32_t>(clampedEnergy * static_cast<float>(me->max_energy) + 0.5f);
}

static uint32_t FindNearbyAllyAgent(uint32_t selfId, float maxDistance) {
    if (Offsets::AgentBase <= 0x10000 || selfId == 0) return 0;

    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(selfId, myX, myY)) return 0;

    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return 0;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    const float maxDistSq = maxDistance * maxDistance;
    float bestDistSq = maxDistSq;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
        if (i == selfId) continue;

        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if (base->type != 0xDB) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->hp <= 0.0f) continue;
        if (living->allegiance == 3) continue;

        const float distSq = AgentMgr::GetSquaredDistance(myX, myY, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = i;
        }
    }

    return bestId;
}

bool MovePlayerNear(float x, float y, float threshold, int timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        GameThread::EnqueuePost([x, y]() {
            AgentMgr::Move(x, y);
        });
        Sleep(500);

        float px = 0.0f;
        float py = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), px, py)) continue;

        if (AgentMgr::GetDistance(px, py, x, y) <= threshold) {
            return true;
        }
    }
    return false;
}

static size_t CollectNearbyFoeAgents(float maxDistance, uint32_t* outIds, size_t capacity) {
    if (!outIds || capacity == 0) return 0;

    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return 0;

    if (Offsets::AgentBase <= 0x10000) return 0;

    struct Candidate {
        uint32_t id;
        float distSq;
    };

    Candidate best[8] = {};
    size_t bestCount = 0;

    static bool s_loggedAgentScan = false;
    uint32_t totalAgents = 0, livingCount = 0, foeCount = 0;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr <= 0x10000 || maxAgents == 0) return 0;

        const float maxDistSq = maxDistance * maxDistance;

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            if (i == myId) continue;

            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;
            totalAgents++;

            auto* base = reinterpret_cast<Agent*>(agentPtr);
            if (base->type != 0xDB) continue;
            livingCount++;

            auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
            if (living->allegiance != 3) continue;
            if (living->hp <= 0.0f) continue;
            foeCount++;

            const float distSq = AgentMgr::GetSquaredDistance(myX, myY, living->x, living->y);
            if (distSq > maxDistSq) continue;

            const size_t limit = (capacity < 8) ? capacity : 8;
            size_t insertAt = bestCount;
            if (bestCount < limit) {
                best[bestCount++] = {i, distSq};
            } else if (distSq >= best[bestCount - 1].distSq) {
                continue;
            } else {
                insertAt = bestCount - 1;
                best[insertAt] = {i, distSq};
            }

            while (insertAt > 0 && best[insertAt].distSq < best[insertAt - 1].distSq) {
                Candidate tmp = best[insertAt - 1];
                best[insertAt - 1] = best[insertAt];
                best[insertAt] = tmp;
                --insertAt;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    if (!s_loggedAgentScan) {
        Log::Info("[INTG] Agent scan: total=%u living=%u foes=%u nearby=%u (maxAgents=%u)",
                  totalAgents, livingCount, foeCount, static_cast<uint32_t>(bestCount),
                  *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8));
        s_loggedAgentScan = true;
    }

    for (size_t i = 0; i < bestCount; ++i) {
        outIds[i] = best[i].id;
    }
    return bestCount;
}

static bool FindNearestFoeAgent(float maxDistance, uint32_t& outId, float& outX, float& outY, float& outDistance) {
    outId = 0;
    outX = 0.0f;
    outY = 0.0f;
    outDistance = 0.0f;

    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return false;
    if (Offsets::AgentBase <= 0x10000) return false;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr <= 0x10000 || maxAgents == 0) return false;

        const float maxDistSq = maxDistance * maxDistance;
        float bestDistSq = maxDistSq;

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            if (i == myId) continue;

            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;

            auto* base = reinterpret_cast<Agent*>(agentPtr);
            if (base->type != 0xDB) continue;

            auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
            if (living->allegiance != 3) continue;
            if (living->hp <= 0.0f) continue;

            const float distSq = AgentMgr::GetSquaredDistance(myX, myY, living->x, living->y);
            if (distSq >= bestDistSq) continue;

            bestDistSq = distSq;
            outId = i;
            outX = living->x;
            outY = living->y;
        }

        if (outId == 0) return false;
        outDistance = sqrtf(bestDistSq);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool WaitForStablePlayerState(int timeoutMs) {
    const DWORD start = GetTickCount();
    int consecutiveReady = 0;
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        if (IsPlayerRuntimeReady(true)) {
            if (++consecutiveReady >= 4) return true;
        } else {
            consecutiveReady = 0;
        }
        Sleep(500);
    }
    IntReport("  Timeout waiting for: player runtime state ready (%d ms)", timeoutMs);
    return false;
}

bool WaitForPlayerWorldReady(int timeoutMs) {
    const DWORD start = GetTickCount();
    int consecutiveReady = 0;
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        if (IsPlayerRuntimeReady(false)) {
            if (++consecutiveReady >= 4) return true;
        } else {
            consecutiveReady = 0;
        }
        Sleep(500);
    }
    IntReport("  Timeout waiting for: player world state ready (%d ms)", timeoutMs);
    return false;
}

bool WaitForSessionHydrationIfNeeded() {
    const uint32_t mapId = ReadMapId();
    if (mapId == 0) return false;

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area) {
        IntReport("  Session hydration: AreaInfo unavailable for map %u", mapId);
        return false;
    }

    IntReport("  Session hydration check: Map %u regionType=%u (%s)",
              mapId,
              area->type,
              DescribeMapRegionType(area->type));

    // Outpost-like maps are already covered well enough by bootstrap;
    // the long-tail flakiness is specific to sessions that reconnect
    // directly into an explorable instance.
    if (!IsSkillCastMapType(area->type)) {
        return true;
    }

    IntReport("  Session hydration: waiting for explorable runtime state...");
    const DWORD start = GetTickCount();
    int consecutiveReady = 0;
    bool ready = false;
    while ((GetTickCount() - start) < 45000) {
        if (IsPlayerRuntimeReady(true)) {
            if (++consecutiveReady >= 6) {
                ready = true;
                break;
            }
        } else {
            consecutiveReady = 0;
        }
        Sleep(500);
    }
    if (!ready) {
        IntReport("  Session hydration after wait: TypeMap=0x%X ModelState=%u",
                  GetPlayerTypeMap(),
                  GetPlayerModelState());
    } else {
        IntReport("  Session hydration complete: TypeMap=0x%X ModelState=%u",
                  GetPlayerTypeMap(),
                  GetPlayerModelState());
    }
    return ready;
}

InventorySnapshot CaptureInventorySnapshot() {
    InventorySnapshot snapshot{};

    __try {
        Inventory* inv = ItemMgr::GetInventory();
        if (!inv) return snapshot;

        snapshot.goldCharacter = inv->gold_character;
        snapshot.goldStorage = inv->gold_storage;

        for (uint32_t bagIndex = 0; bagIndex < 23; ++bagIndex) {
            Bag* bag = inv->bags[bagIndex];
            if (!bag || !bag->items.buffer) continue;

            for (uint32_t i = 0; i < bag->items.size; ++i) {
                Item* item = bag->items.buffer[i];
                if (!item) continue;

                snapshot.count++;
                snapshot.itemIdSum += item->item_id;
                snapshot.modelIdSum += item->model_id;
                snapshot.quantitySum += item->quantity;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return InventorySnapshot{};
    }

    return snapshot;
}

bool InventoryChangedMeaningfully(const InventorySnapshot& before, const InventorySnapshot& after) {
    return after.goldCharacter != before.goldCharacter ||
           after.goldStorage != before.goldStorage ||
           after.count != before.count ||
           after.itemIdSum != before.itemIdSum ||
           after.modelIdSum != before.modelIdSum ||
           after.quantitySum != before.quantitySum;
}

AgentItem* FindGroundItemByAgentId(uint32_t agentId) {
    if (agentId == 0 || Offsets::AgentBase <= 0x10000) return nullptr;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        if (agentArr <= 0x10000) return nullptr;

        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + agentId * 4);
        if (agentPtr <= 0x10000) return nullptr;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if ((base->type & 0x400) == 0) return nullptr;
        return reinterpret_cast<AgentItem*>(agentPtr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

AgentItem* FindNearbyGroundItem(float maxDistance) {
    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return nullptr;

    if (Offsets::AgentBase <= 0x10000) return nullptr;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr <= 0x10000 || maxAgents == 0) return nullptr;

        const float maxDistSq = maxDistance * maxDistance;
        float bestDistSq = maxDistSq;
        AgentItem* bestItem = nullptr;
        static bool s_loggedItemTypes = false;
        uint32_t typeCounts[8] = {}; // bucket agent types for diagnostics

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;

            auto* base = reinterpret_cast<Agent*>(agentPtr);
            // Log type distribution once
            uint32_t t = base->type;
            if (t == 0xDB) typeCounts[0]++;       // Living
            else if (t == 0x400) typeCounts[1]++;  // Item (classic)
            else if (t & 0x400) typeCounts[2]++;   // Item-like
            else if (t == 0x200) typeCounts[3]++;  // Gadget
            else typeCounts[4]++;                   // Other

            if ((base->type & 0x400) == 0) continue;

            auto* item = reinterpret_cast<AgentItem*>(agentPtr);
            const float distSq = AgentMgr::GetSquaredDistance(myX, myY, item->x, item->y);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestItem = item;
            }
        }

        if (!s_loggedItemTypes || bestItem) {
            Log::Info("[INTG] FindItem types: living(0xDB)=%u item(0x400)=%u itemLike=%u gadget(0x200)=%u other=%u bestItem=%p",
                      typeCounts[0], typeCounts[1], typeCounts[2], typeCounts[3], typeCounts[4], bestItem);
            if (bestItem) {
                Log::Info("[INTG] Found item agent: type=0x%X agent_id=%u item_id=%u pos=(%.0f, %.0f)",
                          reinterpret_cast<Agent*>(bestItem)->type,
                          bestItem->agent_id, bestItem->item_id, bestItem->x, bestItem->y);
            }
            s_loggedItemTypes = true;
        }

        return bestItem;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

const char* DescribeMapRegionType(uint32_t type) {
    switch (static_cast<MapRegionType>(type)) {
    case MapRegionType::ExplorableZone: return "ExplorableZone";
    case MapRegionType::MissionOutpost: return "MissionOutpost";
    case MapRegionType::CooperativeMission: return "CooperativeMission";
    case MapRegionType::EliteMission: return "EliteMission";
    case MapRegionType::Challenge: return "Challenge";
    case MapRegionType::Outpost: return "Outpost";
    case MapRegionType::City: return "City";
    case MapRegionType::MissionArea: return "MissionArea";
    case MapRegionType::EotnMission: return "EotnMission";
    case MapRegionType::Dungeon: return "Dungeon";
    case MapRegionType::Marketplace: return "Marketplace";
    default: return "Other";
    }
}

bool IsSkillCastMapType(uint32_t type) {
    switch (static_cast<MapRegionType>(type)) {
    case MapRegionType::ExplorableZone:
    case MapRegionType::CooperativeMission:
    case MapRegionType::CompetitiveMission:
    case MapRegionType::EliteMission:
    case MapRegionType::Challenge:
    case MapRegionType::MissionArea:
    case MapRegionType::HeroBattleArea:
    case MapRegionType::EotnMission:
    case MapRegionType::Dungeon:
        return true;
    default:
        return false;
    }
}

bool IsCurrentMapSkillCastable() {
    const uint32_t mapId = ReadMapId();
    if (mapId == 0) return false;

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    return area && IsSkillCastMapType(area->type);
}

bool TryChooseOffensiveSkillCandidate(uint32_t targetId, SkillTestCandidate& out) {
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar || !targetId) return false;

    const uint32_t currentEnergy = GetCurrentEnergyPoints();
    bool found = false;
    int bestObservability = -1;
    SkillTestCandidate best{};

    auto observabilityScore = [](const Skill* skill) -> int {
        if (!skill) return 0;
        int score = 0;
        if (skill->recharge > 0) score += 100;
        if (skill->activation > 0.0f) score += 20;
        if (skill->aftercast > 0.0f) score += 10;
        return score;
    };

    for (uint32_t slot = 1; slot <= 8; ++slot) {
        SkillbarSkill* sb = &bar->skills[slot - 1];
        if (!sb || sb->skill_id == 0 || sb->recharge != 0) continue;

        const Skill* skill = SkillMgr::GetSkillConstantData(sb->skill_id);
        if (!skill) continue;
        if (skill->target != 5) continue;
        if (skill->adrenaline != 0) continue;

        const uint32_t energyCost = NormalizeSkillEnergyCost(skill->energy_cost);
        if (energyCost > currentEnergy) continue;

        const int observability = observabilityScore(skill);
        if (observability <= 0) continue;

        if (!found || observability > bestObservability) {
            best.slot = slot;
            best.skillId = sb->skill_id;
            best.targetId = targetId;
            best.targetType = skill->target;
            best.energyCost = skill->energy_cost;
            best.type = skill->type;
            best.baseRecharge = skill->recharge;
            best.activation = skill->activation;
            bestObservability = observability;
            found = true;
        }
    }

    if (found) out = best;
    return found;
}

bool TryForceNearbyLootDrop() {
    const struct SparkflyProbeStep {
        float x;
        float y;
    } kProbeSteps[] = {
        {-4559.0f, -14406.0f},
        {-5204.0f,  -9831.0f},
        {-7520.0f, -14166.0f},
        { -928.0f,  -8699.0f},
        {-6374.0f, -13639.0f},
    };

    auto waitForDropAfterCombat = []() {
        return WaitFor("ground item appears after nearby combat", 5000, []() {
            return FindNearbyGroundItem(5000.0f) != nullptr;
        });
    };

    int foesDefeated = 0;
    int foesDamaged = 0;

    for (const auto& step : kProbeSteps) {
        if (FindNearbyGroundItem(5000.0f)) return true;

        IntReport("  Probing Sparkfly combat route at (%.0f, %.0f)...", step.x, step.y);
        MovePlayerNear(step.x, step.y, 350.0f, 15000);
        Sleep(1000);

        uint32_t foeIds[8] = {};
        const size_t foeCount = CollectNearbyFoeAgents(5000.0f, foeIds, 8);
        if (foeCount == 0) {
            uint32_t nearestFoeId = 0;
            float nearestFoeX = 0.0f;
            float nearestFoeY = 0.0f;
            float nearestFoeDist = 0.0f;
            if (FindNearestFoeAgent(25000.0f, nearestFoeId, nearestFoeX, nearestFoeY, nearestFoeDist)) {
                IntReport("  No nearby foes at probe point; moving toward nearest foe %u at (%.0f, %.0f), dist=%.0f...",
                          nearestFoeId, nearestFoeX, nearestFoeY, nearestFoeDist);
                MovePlayerNear(nearestFoeX, nearestFoeY, 1200.0f, 25000);
                Sleep(1000);
                const size_t retriedFoeCount = CollectNearbyFoeAgents(5000.0f, foeIds, 8);
                if (retriedFoeCount == 0) continue;
                IntReport("  Found %u nearby foe(s) after nearest-foe chase", static_cast<unsigned>(retriedFoeCount));
                for (size_t i = retriedFoeCount; i < 8; ++i) foeIds[i] = 0;
                // Reuse the same loop body below with the refreshed foeIds set.
                const size_t foeCountAfterChase = retriedFoeCount;
                IntReport("  Found %u nearby foe(s) for loot setup", static_cast<unsigned>(foeCountAfterChase));
                for (size_t i = 0; i < foeCountAfterChase; ++i) {
                    const uint32_t foeId = foeIds[i];
                    AgentLiving* foe = GetAgentLivingRaw(foeId);
                    if (!foe || foe->hp <= 0.0f) continue;

                    const float hpBefore = foe->hp;
                    IntReport("  Engaging nearby foe %u to create a loot drop opportunity (hp=%.2f)...", foeId, hpBefore);

                    SkillTestCandidate offensiveSkill{};
                    const bool haveOffensiveSkill = TryChooseOffensiveSkillCandidate(foeId, offensiveSkill);
                    if (haveOffensiveSkill) {
                        IntReport("  Offensive skill available for loot setup: slot %u skill %u", offensiveSkill.slot, offensiveSkill.skillId);
                    }

                    bool foeDefeated = false;
                    bool foeDamaged = false;
                    const DWORD fightStart = GetTickCount();
                    while ((GetTickCount() - fightStart) < 30000) {
                        foe = GetAgentLivingRaw(foeId);
                        if (!foe || foe->hp <= 0.0f) {
                            foeDefeated = true;
                            ++foesDefeated;
                            break;
                        }

                        float px = 0.0f;
                        float py = 0.0f;
                        if (TryReadAgentPosition(ReadMyId(), px, py)) {
                            const float dist = AgentMgr::GetDistance(px, py, foe->x, foe->y);
                            if (dist > 250.0f) {
                                float fx = foe->x, fy = foe->y;
                                GameThread::EnqueuePost([fx, fy]() { AgentMgr::Move(fx, fy); });
                                Sleep(dist > 1200.0f ? 750 : 350);
                            }
                        }

                        AgentMgr::ChangeTarget(foeId);

                        // Keep loot combat stable: combat-time hero flagging
                        // can crash Sparkfly in this client build.

                        if (haveOffensiveSkill) {
                            Skillbar* liveBar = SkillMgr::GetPlayerSkillbar();
                            if (liveBar && liveBar->skills[offensiveSkill.slot - 1].recharge == 0) {
                                SkillMgr::UseSkill(offensiveSkill.slot, foeId, 0);
                            }
                        }
                        Sleep(1500);

                        foe = GetAgentLivingRaw(foeId);
                        if (foe) {
                            if (foe->hp < hpBefore && foe->hp > 0.0f) {
                                IntReport("  Foe %u HP: %.2f (taking damage)", foeId, foe->hp);
                            }
                        }

                        if (FindNearbyGroundItem(5000.0f)) return true;

                        foe = GetAgentLivingRaw(foeId);
                        if (foe && foe->hp < hpBefore) {
                            foeDamaged = true;
                        }
                        if (!foe || foe->hp <= 0.0f) {
                            foeDefeated = true;
                            ++foesDefeated;
                            break;
                        }
                    }

                    AgentMgr::CancelAction();
                    Sleep(250);

                    if (foeDamaged) {
                        ++foesDamaged;
                    } else {
                        IntReport("  Loot setup combat did not reduce foe %u HP", foeId);
                    }

                    if (foeDefeated && waitForDropAfterCombat()) {
                        return true;
                    }

                    if (FindNearbyGroundItem(5000.0f)) return true;
                    if (foesDefeated >= 8) break;
                }
                if (FindNearbyGroundItem(5000.0f)) return true;
                if (foesDefeated >= 8) break;
                continue;
            }
            continue;
        }

        IntReport("  Found %u nearby foe(s) for loot setup", static_cast<unsigned>(foeCount));

        for (size_t i = 0; i < foeCount; ++i) {
            const uint32_t foeId = foeIds[i];
            AgentLiving* foe = GetAgentLivingRaw(foeId);
            if (!foe || foe->hp <= 0.0f) continue;

            const float hpBefore = foe->hp;
            IntReport("  Engaging nearby foe %u to create a loot drop opportunity (hp=%.2f)...", foeId, hpBefore);

            SkillTestCandidate offensiveSkill{};
            const bool haveOffensiveSkill = TryChooseOffensiveSkillCandidate(foeId, offensiveSkill);
            if (haveOffensiveSkill) {
                IntReport("  Offensive skill available for loot setup: slot %u skill %u", offensiveSkill.slot, offensiveSkill.skillId);
            }

            bool foeDefeated = false;
            bool foeDamaged = false;
            const DWORD fightStart = GetTickCount();
            while ((GetTickCount() - fightStart) < 30000) {
                foe = GetAgentLivingRaw(foeId);
                if (!foe || foe->hp <= 0.0f) {
                    foeDefeated = true;
                    ++foesDefeated;
                    break;
                }

                float px = 0.0f;
                float py = 0.0f;
                if (TryReadAgentPosition(ReadMyId(), px, py)) {
                    const float dist = AgentMgr::GetDistance(px, py, foe->x, foe->y);
                    if (dist > 250.0f) {
                        float fx = foe->x, fy = foe->y;
                        GameThread::EnqueuePost([fx, fy]() { AgentMgr::Move(fx, fy); });
                        Sleep(dist > 1200.0f ? 750 : 350);
                    }
                }

                AgentMgr::ChangeTarget(foeId);

                // Keep loot combat stable: combat-time hero flagging can
                // crash Sparkfly in this client build.

                if (haveOffensiveSkill) {
                    Skillbar* liveBar = SkillMgr::GetPlayerSkillbar();
                    if (liveBar && liveBar->skills[offensiveSkill.slot - 1].recharge == 0) {
                        SkillMgr::UseSkill(offensiveSkill.slot, foeId, 0);
                    }
                }
                Sleep(1500);

                // Log combat progress
                foe = GetAgentLivingRaw(foeId);
                if (foe) {
                    if (foe->hp < hpBefore && foe->hp > 0.0f) {
                        IntReport("  Foe %u HP: %.2f (taking damage)", foeId, foe->hp);
                    }
                }

                if (FindNearbyGroundItem(5000.0f)) return true;

                foe = GetAgentLivingRaw(foeId);
                if (foe && foe->hp < hpBefore) {
                    foeDamaged = true;
                }
                if (!foe || foe->hp <= 0.0f) {
                    foeDefeated = true;
                    ++foesDefeated;
                    break;
                }
            }

            AgentMgr::CancelAction();
            Sleep(250);

            if (foeDamaged) {
                ++foesDamaged;
            } else {
                IntReport("  Loot setup combat did not reduce foe %u HP", foeId);
            }

            if (foeDefeated && waitForDropAfterCombat()) {
                return true;
            }

            if (FindNearbyGroundItem(5000.0f)) return true;
            if (foesDefeated >= 8) break;
        }

        if (FindNearbyGroundItem(5000.0f)) return true;
        if (foesDefeated >= 8) break;
    }

    IntReport("  Loot setup summary: foesDamaged=%d foesDefeated=%d",
              foesDamaged,
              foesDefeated);
    return FindNearbyGroundItem(5000.0f) != nullptr;
}

void DumpSkillbarForSkillTest() {
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = GetAgentLivingRaw(ReadMyId());
    if (!bar || !me) {
        IntReport("  Skill dump unavailable (skillbar=%p me=%p)", bar, me);
        return;
    }

    IntReport("  Skill dump: energy=%u target=%u active=%u",
              GetCurrentEnergyPoints(),
              AgentMgr::GetTargetId(),
              me->skill);
    for (uint32_t slot = 1; slot <= 8; ++slot) {
        const SkillbarSkill& sb = bar->skills[slot - 1];
        if (sb.skill_id == 0) continue;
        const Skill* skill = SkillMgr::GetSkillConstantData(sb.skill_id);
        IntReport("    Slot %u skill=%u targetType=%u type=%u energy=%u adrenaline=%u recharge=%u event=%u",
                  slot,
                  sb.skill_id,
                  skill ? skill->target : 0xFF,
                  skill ? skill->type : 0xFFFFFFFFu,
                  skill ? NormalizeSkillEnergyCost(skill->energy_cost) : 0,
                  skill ? skill->adrenaline : 0,
                  sb.recharge,
                  sb.event);
    }
}

bool TryChooseSkillTestCandidate(SkillTestCandidate& out) {
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = GetAgentLivingRaw(ReadMyId());
    const uint32_t myId = ReadMyId();
    if (!bar || !me || myId == 0) return false;

    const uint32_t allyId = FindNearbyAllyAgent(myId, 5000.0f);
    const uint32_t currentEnergy = GetCurrentEnergyPoints();

    auto slotPriority = [](uint8_t targetType) -> int {
        switch (targetType) {
        case 0: return 0; // self
        case 3: return 1; // ally/self
        case 4: return 2; // other ally
        case 5: return 3; // enemy
        default: return 99;
        }
    };

    auto observabilityScore = [](const Skill* skill) -> int {
        int score = 0;
        if (!skill) return score;
        if (skill->recharge > 0) score += 100;
        if (skill->activation > 0.0f) score += 20;
        if (skill->aftercast > 0.0f) score += 10;
        return score;
    };

    bool found = false;
    int bestPriority = 100;
    int bestObservability = -1;
    SkillTestCandidate best{};

    for (uint32_t slot = 1; slot <= 8; ++slot) {
        SkillbarSkill* skillbarSkill = &bar->skills[slot - 1];
        if (!skillbarSkill || skillbarSkill->skill_id == 0) continue;
        if (skillbarSkill->recharge != 0) continue;

        const Skill* skill = SkillMgr::GetSkillConstantData(skillbarSkill->skill_id);
        if (!skill) continue;
        if (skill->adrenaline != 0) continue;

        const uint32_t energyCost = NormalizeSkillEnergyCost(skill->energy_cost);
        if (energyCost > currentEnergy) continue;

        uint32_t targetId = 0;
        switch (skill->target) {
        case 0:
        case 3:
            targetId = myId;
            break;
        case 4:
            targetId = allyId;
            break;
        case 5:
            // Keep the main regression lane deterministic: offensive skills are
            // used in the dedicated loot/combat micro-phase, while the core
            // skill assertion prefers self/ally casts.
            continue;
        default:
            continue;
        }

        if (targetId == 0) continue;

        const int priority = slotPriority(skill->target);
        const int observability = observabilityScore(skill);
        if (observability <= 0) continue;

        if (!found ||
            observability > bestObservability ||
            (observability == bestObservability && priority < bestPriority)) {
            best.slot = slot;
            best.skillId = skillbarSkill->skill_id;
            best.targetId = targetId;
            best.targetType = skill->target;
            best.energyCost = skill->energy_cost;
            best.type = skill->type;
            best.baseRecharge = skill->recharge;
            best.activation = skill->activation;
            bestPriority = priority;
            bestObservability = observability;
            found = true;
        }
    }

    if (found) out = best;
    return found;
}

// Gameplay slices 028-031 live in IntegrationTestGameplay.cpp.

// Session/trader slices 032-033 live in IntegrationTestSession.cpp.

// World-action and session-state slices 035 and 043-049 live in IntegrationTestWorld.cpp.

// System/state slices 050-057 and offset/patch slices 056,059-073 live in IntegrationTestSystems.cpp and IntegrationTestOffsets.cpp.
// ===== Advanced Integration Runner =====

int RunAdvancedTest() {
    s_intPassed = 0;
    s_intFailed = 0;
    s_intSkipped = 0;

    s_intReport = OpenIntReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== GWA3 Advanced Integration Test Suite ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    const bool inGame = TestCharSelectLogin();

    if (inGame) {
        WaitForSessionHydrationIfNeeded();

        // Read-only introspection tests (safe in any map type)
        TestPlayerData();
        TestCameraIntrospection();
        TestClientInfo();
        TestInventoryIntrospection();
        TestAgentArrayEnumeration();
        TestUIFrameValidation();
        TestAreaInfoValidation();
        TestSkillbarDataValidation();
        TestPartyState();
        TestTargetLogHook();
        TestGuildData();
        TestMapStateQueries();
        TestPingStability();
        TestWeaponSetValidation();
        TestAgentDistanceCrossCheck();
        TestCameraControls();
        TestPostProcessEffectOffset();
        TestGwEndSceneOffset();
        TestItemClickOffset();
        TestSendChatOffset();
        TestAddToChatLogOffset();
        TestSkipCinematicOffset();
        TestRequestQuestInfoOffset();
        TestFriendListOffsets();
        TestDrawOnCompassOffset();
        TestChatColorOffsets();
        TestCameraUpdateBypassPatch();
        TestTradeOffsets();
        TestLevelDataBypassPatch();
        TestMapPortBypassPatch();
        TestEffectArray();
        TestStoCHook();
        TestRenderingToggle();

        // Chat write (local only, safe anywhere)
        TestChatWriteLocal();

        // Outpost-only tests
        const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
        const bool inOutpost = area && !IsSkillCastMapType(area->type);

        if (inOutpost) {
            IntSkip("Hero Flagging (043)", "Outpost session — hero commands only valid in explorable");
            TestHardModeToggle();
        } else {
            IntSkip("Hero Flagging (043)", "Not in outpost");
            IntSkip("Hard Mode Toggle (046)", "Not in outpost");
        }
    } else {
        IntSkip("All advanced tests", "Login failed");
    }

    IntReport("");
    IntReport("=== SUMMARY: %d passed, %d failed, %d skipped ===",
              s_intPassed, s_intFailed, s_intSkipped);

    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }

    Log::Info("[INTG] Advanced complete: %d passed, %d failed, %d skipped",
              s_intPassed, s_intFailed, s_intSkipped);
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), s_crashDetected ? 1 : 0);
    return s_intFailed;
}

// ===== Advanced Workflow Runner (GWA3-074..089) =====

int RunAdvancedWorkflowTest() {
    s_intPassed = 0;
    s_intFailed = 0;
    s_intSkipped = 0;
    StartWatchdog();

    s_intReport = OpenIntReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== GWA3 Advanced Workflow Test Suite (074-089) ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    bool inGame = TestCharSelectLogin();

    if (inGame) {
        WaitForPlayerWorldReady(10000);
        if (AbortWorkflowIfRuntimeFailed("bootstrap")) goto workflow_done;

        // 074: Item workflows
        TestItemMove();
        if (AbortWorkflowIfRuntimeFailed("TestItemMove")) goto workflow_done;
        TestGoldTransfer();
        if (AbortWorkflowIfRuntimeFailed("TestGoldTransfer")) goto workflow_done;
        TestMemAllocFree();
        if (AbortWorkflowIfRuntimeFailed("TestMemAllocFree")) goto workflow_done;
        if (kBisectWorkflowStopAfter074c) {
            IntSkip("Workflow after 074c (076-089)", "Temporarily skipped while bisecting early workflow crash");
            goto workflow_done;
        }

        // 076: Skillbar management
        TestLoadSkillbar();
        if (AbortWorkflowIfRuntimeFailed("TestLoadSkillbar")) goto workflow_done;

        // 077: Party management
        TestPartyManagement();
        if (AbortWorkflowIfRuntimeFailed("TestPartyManagement")) goto workflow_done;

        // 078: Title management
        TestTitleManagement();
        if (AbortWorkflowIfRuntimeFailed("TestTitleManagement")) goto workflow_done;
        if (kBisectWorkflowStopAfterEarlyPhase) {
            IntSkip("Workflow tail (080-089)", "Temporarily skipped while bisecting late workflow crash");
            goto workflow_done;
        }

        // 080: Callback registry
        TestCallbackRegistry();
        if (AbortWorkflowIfRuntimeFailed("TestCallbackRegistry")) goto workflow_done;

        // 081: GameThread persistent callbacks
        TestGameThreadCallbacks();
        if (AbortWorkflowIfRuntimeFailed("TestGameThreadCallbacks")) goto workflow_done;

        // 082: StoC packet type coverage
        TestStoCPacketTypes();
        if (AbortWorkflowIfRuntimeFailed("TestStoCPacketTypes")) goto workflow_done;
        if (kBisectWorkflowSkipPostStoCTail) {
            IntSkip("Workflow post-StoC tail (083-089)", "Temporarily skipped while bisecting late workflow crash");
            goto workflow_done;
        }

        // 083: Quest management
        TestQuestManagement();
        if (AbortWorkflowIfRuntimeFailed("TestQuestManagement")) goto workflow_done;
        if (kBisectWorkflowOnlyQuestTail) {
            IntSkip("Workflow tail after quest (085-089)", "Temporarily skipped while bisecting late workflow crash");
            goto workflow_done;
        }

        // 085: UI frame interaction
        TestUIFrameInteraction();
        if (AbortWorkflowIfRuntimeFailed("TestUIFrameInteraction")) goto workflow_done;
        if (kBisectWorkflowOnlyUiTail) {
            IntSkip("Workflow tail after UI frame interaction (086-089)", "Temporarily skipped while bisecting late workflow crash");
            goto workflow_done;
        }

        // 086: Agent interaction
        TestAgentInteraction();
        if (AbortWorkflowIfRuntimeFailed("TestAgentInteraction")) goto workflow_done;

        // 087: Camera FOV
        TestCameraFOV();
        if (AbortWorkflowIfRuntimeFailed("TestCameraFOV")) goto workflow_done;

        // 089: Memory personal dir
        TestPersonalDir();
        if (AbortWorkflowIfRuntimeFailed("TestPersonalDir")) goto workflow_done;

        // 086b: CallTarget in explorable (walks into Sparkfly Swamp)
        TestExplorableCallTarget();
        if (AbortWorkflowIfRuntimeFailed("TestExplorableCallTarget")) goto workflow_done;
    } else {
        IntSkip("All advanced workflow tests", "Login failed");
    }

workflow_done:
    IntReport("");
    IntReport("=== SUMMARY: %d passed, %d failed, %d skipped ===",
              s_intPassed, s_intFailed, s_intSkipped);

    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }

    StopWatchdog();
    Log::Info("[INTG] Advanced workflow complete: %d passed, %d failed, %d skipped",
              s_intPassed, s_intFailed, s_intSkipped);
    Log::Info("[INTG] Advanced workflow exit state: crashDetected=%d disconnectDetected=%d",
              s_crashDetected ? 1 : 0, s_disconnectDetected ? 1 : 0);
    return s_intFailed;
}

// ===== Main Integration Runner =====

int RunIntegrationTest() {
    s_intPassed = 0;
    s_intFailed = 0;
    s_intSkipped = 0;
    StartWatchdog();

    s_intReport = OpenIntReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== GWA3 Integration Test Suite ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

        // Phase 1: character select and login
    bool inGame = TestCharSelectLogin();

    if (inGame) {
        const bool startedExplorable = IsCurrentMapSkillCastable();
        const bool sessionHydrated = WaitForSessionHydrationIfNeeded();

        if (startedExplorable) {
            IntReport("  Branching integration flow: explorable-start session");
            IntSkip("Outpost Travel (033)", "Bootstrap landed in explorable instance");
            IntSkip("Explorable Entry (035)", "Already in explorable instance");

            if (!sessionHydrated) {
                IntSkip("Movement (030)", "Explorable session never reached stable runtime readiness");
                IntSkip("Targeting (030)", "Explorable session never reached stable runtime readiness");
                IntSkip("Skill activation (034)", "Explorable session never reached stable runtime readiness");
                IntSkip("Loot pickup (031)", "Explorable session never reached stable runtime readiness");
                IntSkip("Hero Setup (029)", "Deferred hero setup because explorable session never stabilized");
                IntSkip("Advanced tests (036-049)", "Explorable session never stabilized");
            } else {
                // Keep the explorable-start branch focused on world-action
                // coverage first; hero setup appears to destabilize this session class.
                TestMovement();
                TestTargeting();
                TestSkillActivation();
                TestLootPickup();

                IntReport("  Running deferred hero setup after explorable actions...");
                TestHeroSetup();

                // Advanced introspection tests (read-only, safe in any map type)
                IntReport("  Running advanced introspection tests...");
                TestPlayerData();
                TestCameraIntrospection();
                TestClientInfo();
                TestInventoryIntrospection();
                TestAgentArrayEnumeration();
                TestUIFrameValidation();
                TestAreaInfoValidation();
                TestSkillbarDataValidation();
                TestPartyState();
                TestTargetLogHook();
                TestGuildData();
                TestMapStateQueries();
                TestPingStability();
                TestWeaponSetValidation();
                TestAgentDistanceCrossCheck();
                TestCameraControls();
                TestChatWriteLocal();

                // Return to outpost from explorable
                TestReturnToOutpost();
            }
        } else {
            IntReport("  Branching integration flow: outpost-start session");

            // Phase 2: hero setup
            TestHeroSetup();

            // Phase 3: movement
            TestMovement();

            // Phase 4: Targeting
            TestTargeting();

            TestHardModeToggle();

            // Phase 5: explorable bootstrap
            IntSkip("Outpost Travel (033)", "Session reserved for explorable skill coverage");
            const bool inExplorable = TestExplorableEntry();

            if (inExplorable) {
                // Wait for explorable to fully load (agents, navmesh, etc.)
                IntReport("  Waiting for explorable runtime to stabilize...");
                WaitFor("explorable map loaded + agents available", 15000, []() {
                    if (!MapMgr::GetIsMapLoaded()) return false;
                    if (ReadMyId() == 0) return false;
                    AgentLiving* me = AgentMgr::GetMyAgent();
                    if (!me || me->hp <= 0.0f) return false;
                    return AgentMgr::GetMaxAgents() > 10;
                });

                // Keep explorable coverage stable: post-load hero flagging is
                // disabled until Sparkfly hero-command timing is understood.
                IntSkip("Hero Flagging (043)", "Disabled in explorable pending Sparkfly hero-command stabilization");

                TestSkillActivation();
                TestLootPickup();

                // Return to outpost from explorable
                TestReturnToOutpost();
            } else {
                IntSkip("Skill activation (034)", "Explorable entry failed");
                IntSkip("Loot pickup (031)", "Explorable entry failed");
                IntSkip("Return to outpost (047)", "Explorable entry failed");
            }
        }
    } else {
        IntSkip("Hero Setup (029)", "Login failed");
        IntSkip("Movement (030)", "Login failed");
        IntSkip("Targeting (030)", "Login failed");
        IntSkip("Skill activation (034)", "Login failed");
        IntSkip("NPC + Dialog (032)", "Login failed");
        IntSkip("Outpost Travel (033)", "Login failed");
        IntSkip("Explorable Entry (035)", "Login failed");
        IntSkip("Loot (031)", "Login failed");
        IntSkip("Merchant (032)", "Login failed");
        IntSkip("Advanced tests (036-049)", "Login failed");
    }

    IntReport("");
    IntReport("=== SUMMARY: %d passed, %d failed, %d skipped ===",
              s_intPassed, s_intFailed, s_intSkipped);

    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }

    Log::Info("[INTG] Complete: %d passed, %d failed, %d skipped",
              s_intPassed, s_intFailed, s_intSkipped);
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), s_crashDetected ? 1 : 0);
    return s_intFailed;
}

int RunNpcDialogTest() {
    s_intPassed = 0;
    s_intFailed = 0;
    s_intSkipped = 0;

    s_intReport = OpenIntReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== GWA3 NPC/Dialog Test ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    const bool inGame = TestCharSelectLogin();
    if (inGame) {
        WaitForSessionHydrationIfNeeded();
        TestNpcDialog();
    } else {
        IntSkip("NPC + Dialog (032)", "Login failed");
    }

    IntReport("");
    IntReport("=== SUMMARY: %d passed, %d failed, %d skipped ===",
              s_intPassed, s_intFailed, s_intSkipped);

    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }

    Log::Info("[INTG] NPC/Dialog complete: %d passed, %d failed, %d skipped",
              s_intPassed, s_intFailed, s_intSkipped);
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), s_crashDetected ? 1 : 0);
    return s_intFailed;
}

int RunMerchantQuoteTest() {
    s_intPassed = 0;
    s_intFailed = 0;
    s_intSkipped = 0;

    s_intReport = OpenIntReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== GWA3 Merchant/Quote Test ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    const bool inGame = TestCharSelectLogin();
    if (inGame) {
        WaitForSessionHydrationIfNeeded();
        TestMerchantQuote();
    } else {
        IntSkip("Merchant + Quote (032)", "Login failed");
    }

    IntReport("");
    IntReport("=== SUMMARY: %d passed, %d failed, %d skipped ===",
              s_intPassed, s_intFailed, s_intSkipped);

    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }

    Log::Info("[INTG] Merchant/Quote complete: %d passed, %d failed, %d skipped",
              s_intPassed, s_intFailed, s_intSkipped);
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), s_crashDetected ? 1 : 0);
    return s_intFailed;
}

} // namespace GWA3::SmokeTest
