// IntegrationTestEpic14.cpp — Phased Froggy feature tests
// Run via: injector.exe --test-froggy
//
// Self-setting-up: travels to outpost, adds heroes, opens merchant,
// enters explorable, finds enemies, then returns. No manual setup needed.

#include <gwa3/core/Log.h>
#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/bot/FroggyHM.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/game/Agent.h>

#include <Windows.h>

namespace GWA3::SmokeTest {

static int s_passed = 0;
static int s_failed = 0;
static int s_skipped = 0;

static constexpr uint32_t MAP_GADDS = 638;
static constexpr uint32_t MAP_SPARKFLY = 558;

static void IntReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log::Info("[FROGGY-TEST] %s", buf);
}

static void IntCheck(const char* name, bool cond) {
    if (cond) {
        s_passed++;
        IntReport("  [PASS] %s", name);
    } else {
        s_failed++;
        IntReport("  [FAIL] %s", name);
    }
}

static void IntSkip(const char* name, const char* reason) {
    s_skipped++;
    IntReport("  [SKIP] %s — %s", name, reason);
}

// Wait for a condition with timeout. Returns true if condition met.
static bool WaitFor(const char* label, DWORD timeoutMs, bool(*check)()) {
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (check()) return true;
        Sleep(500);
    }
    IntReport("  WaitFor '%s' timed out after %ums", label, timeoutMs);
    return false;
}

static uint32_t FindNearestNpc(float x, float y, float maxDist) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 6) continue; // NPC
        if (living->hp <= 0.0f) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (d < bestDist) { bestDist = d; bestId = living->agent_id; }
    }
    return bestId;
}

static uint32_t FindNearestFoe(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = maxRange * maxRange;
    uint32_t bestId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 3) continue;
        if (living->hp <= 0.0f) continue;
        float d = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (d < bestDist) { bestDist = d; bestId = living->agent_id; }
    }
    return bestId;
}

int RunFroggyFeatureTest() {
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;

    // ===== PHASE 0: Pure logic unit tests =====
    IntReport("=== PHASE 0: Pure Logic Unit Tests ===");
    int unitFailures = Bot::Froggy::RunFroggyUnitTests();
    IntReport("Unit tests: %d failures", unitFailures);
    s_failed += unitFailures;

    // ===== PHASE 1: Travel to Gadd's Encampment =====
    IntReport("=== PHASE 1: Travel to Gadd's Encampment ===");

    if (MapMgr::GetMapId() != MAP_GADDS) {
        IntReport("  Not at Gadd's (map=%u) — traveling...", MapMgr::GetMapId());
        MapMgr::Travel(MAP_GADDS);
        bool arrived = WaitFor("MapID == 638", 60000, []() {
            return MapMgr::GetMapId() == MAP_GADDS;
        });
        if (!arrived) {
            IntReport("  ABORT: Failed to travel to Gadd's Encampment");
            IntReport("=== FROGGY TESTS ABORTED (no outpost) ===");
            return s_failed + 1;
        }
    }

    bool agentReady = WaitFor("MyID > 0", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    if (!agentReady) {
        IntReport("  ABORT: Agent not ready after travel");
        return s_failed + 1;
    }
    Sleep(3000); // let the map fully hydrate
    IntCheck("Phase 1: In Gadd's Encampment", MapMgr::GetMapId() == MAP_GADDS);

    // ===== PHASE 2: Outpost Tests (party, inventory, skillbar) =====
    IntReport("=== PHASE 2: Outpost Tests ===");

    // Add heroes
    IntReport("  Adding heroes...");
    PartyMgr::KickAllHeroes();
    Sleep(500);
    uint32_t heroIds[] = {25, 14, 21, 4, 24, 15, 1}; // Standard lineup
    for (int i = 0; i < 7; i++) {
        PartyMgr::AddHero(heroIds[i]);
        Sleep(300);
    }
    for (int i = 0; i < 7; i++) {
        PartyMgr::SetHeroBehavior(i + 1, 1); // Guard
        Sleep(100);
    }
    Sleep(1000); // let party settle

    // Skillbar validation
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (bar) {
        int nonZero = 0;
        for (int i = 0; i < 8; i++) {
            if (bar->skills[i].skill_id != 0) nonZero++;
        }
        IntCheck("Skillbar has skills loaded", nonZero > 0);
        if (nonZero > 0) {
            for (int i = 0; i < 8; i++) {
                if (bar->skills[i].skill_id != 0) {
                    const auto* data = SkillMgr::GetSkillConstantData(bar->skills[i].skill_id);
                    IntCheck("Skill constant data exists", data != nullptr);
                    if (data) {
                        IntCheck("Skill profession in range", data->profession <= 10);
                        IntCheck("Skill type in range", data->type <= 24);
                    }
                    break;
                }
            }
        }
    } else {
        IntSkip("Skillbar validation", "Skillbar not available");
    }

    // Inventory
    auto* inv = ItemMgr::GetInventory();
    if (inv) {
        IntCheck("Gold character plausible", inv->gold_character < 1000000);
        IntCheck("Gold storage plausible", inv->gold_storage < 10000000);
        int bagsFound = 0;
        for (int i = 1; i <= 4; i++) {
            auto* bag = ItemMgr::GetBag(i);
            if (bag && bag->items.buffer) bagsFound++;
        }
        IntCheck("At least 1 backpack bag", bagsFound >= 1);
    }

    // Effects
    uint32_t myId = AgentMgr::GetMyId();
    if (myId > 0) {
        IntCheck("HasEffect(bogus 9999)=false", !EffectMgr::HasEffect(myId, 9999));
        auto* effects = EffectMgr::GetPlayerEffects();
        IntCheck("GetPlayerEffects returns non-null", effects != nullptr);
        if (effects) {
            IntCheck("Player effects agent_id matches self", effects->agent_id == myId);
        }
    }

    // ===== PHASE 3: Merchant Tests =====
    IntReport("=== PHASE 3: Merchant Tests ===");

    // Move to merchant
    static constexpr float kMerchX = -8374.0f;
    static constexpr float kMerchY = -22491.0f;
    IntReport("  Moving to merchant (%.0f, %.0f)...", kMerchX, kMerchY);
    AgentMgr::Move(kMerchX, kMerchY);
    {
        DWORD moveStart = GetTickCount();
        auto* me = AgentMgr::GetMyAgent();
        while (me && AgentMgr::GetDistance(me->x, me->y, kMerchX, kMerchY) > 350.0f &&
               (GetTickCount() - moveStart) < 15000) {
            Sleep(250);
            me = AgentMgr::GetMyAgent();
        }
    }
    Sleep(500);

    // Find and interact with merchant
    uint32_t merchantId = FindNearestNpc(kMerchX, kMerchY, 900.0f);
    if (merchantId) {
        IntReport("  Found merchant NPC (agent=%u), interacting...", merchantId);
        auto* npc = AgentMgr::GetAgentByID(merchantId);
        if (npc) {
            AgentMgr::Move(npc->x, npc->y);
            Sleep(2000);
        }
        AgentMgr::ChangeTarget(merchantId);
        Sleep(250);
        CtoS::SendPacket(2, 0x38u, merchantId);
        Sleep(750);
        CtoS::SendPacket(2, 0x3Bu, merchantId);
        Sleep(1000);

        // Check if merchant opened
        bool merchantOpen = WaitFor("Merchant window open", 5000, []() {
            return TradeMgr::GetMerchantItemCount() > 0;
        });
        IntCheck("Merchant window opened", merchantOpen);

        if (merchantOpen) {
            uint32_t itemCount = TradeMgr::GetMerchantItemCount();
            IntCheck("Merchant has items", itemCount > 0);
            IntReport("  Merchant has %u items", itemCount);
        }

        // Close merchant by moving away
        AgentMgr::CancelAction();
        Sleep(500);
    } else {
        IntSkip("Merchant tests", "Merchant NPC not found near target coords");
    }

    // ===== PHASE 4: Enter Explorable =====
    IntReport("=== PHASE 4: Enter Sparkfly Swamp ===");

    // Walk to exit portal
    IntReport("  Walking to Gadd's exit...");
    AgentMgr::Move(-10018.0f, -21892.0f);
    {
        DWORD moveStart = GetTickCount();
        auto* me = AgentMgr::GetMyAgent();
        while (me && AgentMgr::GetDistance(me->x, me->y, -10018.0f, -21892.0f) > 350.0f &&
               (GetTickCount() - moveStart) < 30000) {
            Sleep(500);
            me = AgentMgr::GetMyAgent();
        }
    }
    AgentMgr::Move(-9550.0f, -20400.0f);
    {
        DWORD moveStart = GetTickCount();
        auto* me = AgentMgr::GetMyAgent();
        while (me && AgentMgr::GetDistance(me->x, me->y, -9550.0f, -20400.0f) > 350.0f &&
               (GetTickCount() - moveStart) < 30000) {
            Sleep(500);
            me = AgentMgr::GetMyAgent();
        }
    }

    // Push toward explorable zone exit
    IntReport("  Pushing toward Sparkfly...");
    DWORD zoneStart = GetTickCount();
    bool leftOutpost = false;
    while ((GetTickCount() - zoneStart) < 30000) {
        if (MapMgr::GetMapId() != MAP_GADDS) { leftOutpost = true; break; }
        AgentMgr::Move(-9451.0f, -19766.0f);
        Sleep(500);
    }

    if (leftOutpost) {
        // Wait for Sparkfly to load
        bool inSparkfly = WaitFor("MapID == Sparkfly", 30000, []() {
            return MapMgr::GetMapId() == MAP_SPARKFLY;
        });
        if (inSparkfly) {
            bool agentOk = WaitFor("MyID in explorable", 30000, []() {
                return AgentMgr::GetMyId() > 0;
            });
            Sleep(5000); // stability wait
            IntCheck("Phase 4: Entered Sparkfly Swamp", agentOk);

            // ===== PHASE 5: Explorable Tests =====
            IntReport("=== PHASE 5: Explorable Tests ===");

            // Look for enemies
            uint32_t foeId = FindNearestFoe(5000.0f);
            if (foeId) {
                IntCheck("Found enemy in explorable", true);
                IntReport("  Enemy agent=%u found", foeId);

                // Verify target selection functions return valid results
                auto* foeAgent = AgentMgr::GetAgentByID(foeId);
                IntCheck("Enemy agent readable", foeAgent != nullptr);
                if (foeAgent) {
                    auto* foeLiving = static_cast<AgentLiving*>(foeAgent);
                    IntCheck("Enemy is alive", foeLiving->hp > 0.0f);
                    IntCheck("Enemy is foe allegiance", foeLiving->allegiance == 3);
                }

                // Target the enemy
                AgentMgr::ChangeTarget(foeId);
                Sleep(500);
                IntCheck("Target changed to enemy", AgentMgr::GetTargetId() == foeId);
            } else {
                IntSkip("Enemy targeting tests", "No enemies within 5000 range");
                // Move toward first Sparkfly waypoint to find enemies
                IntReport("  Moving toward enemies...");
                AgentMgr::Move(-4559.0f, -14406.0f); // First Sparkfly waypoint
                Sleep(5000);
                foeId = FindNearestFoe(5000.0f);
                if (foeId) {
                    IntCheck("Found enemy after moving", true);
                } else {
                    IntSkip("Enemy found after move", "Still no enemies — area may be cleared");
                }
            }

            // ===== PHASE 6: Return to Outpost =====
            IntReport("=== PHASE 6: Return to Outpost ===");
            MapMgr::ReturnToOutpost();
            bool returned = WaitFor("MapID == Gadd's after return", 60000, []() {
                return MapMgr::GetMapId() == MAP_GADDS;
            });
            IntCheck("Returned to Gadd's Encampment", returned);

        } else {
            IntSkip("Explorable tests", "Failed to enter Sparkfly Swamp");
            IntSkip("Return to outpost", "Never left outpost");
        }
    } else {
        IntSkip("Explorable entry", "Failed to leave Gadd's within 30s");
        IntSkip("Explorable tests", "Never entered explorable");
        IntSkip("Return to outpost", "Never left outpost");
    }

    IntReport("=== FROGGY FEATURE TESTS COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return s_failed;
}

} // namespace GWA3::SmokeTest
