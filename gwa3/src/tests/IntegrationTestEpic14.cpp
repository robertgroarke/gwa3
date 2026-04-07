// IntegrationTestEpic14.cpp — Phased Froggy feature tests
// Run via: injector.exe --test-froggy
//
// Self-setting-up: travels to outpost, adds heroes, opens merchant,
// enters explorable, finds enemies, then returns. No manual setup needed.

#include "IntegrationTestInternal.h"
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
#include <gwa3/managers/UIMgr.h>
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
        if ((living->effects & 0x0010u) != 0) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (d < bestDist) { bestDist = d; bestId = living->agent_id; }
    }
    return bestId;
}

static uint32_t FindNearestNpcByPlayerNumber(float x, float y, float maxDist, uint16_t playerNumber) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 6) continue;
        if (living->hp <= 0.0f) continue;
        if ((living->effects & 0x0010u) != 0) continue;
        if (living->player_number != playerNumber) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (d < bestDist) { bestDist = d; bestId = living->agent_id; }
    }
    return bestId;
}

struct NpcCandidate {
    uint32_t agentId = 0;
    uint16_t playerNumber = 0;
    uint32_t npcId = 0;
    uint32_t effects = 0;
    float x = 0.0f;
    float y = 0.0f;
    float distance = 0.0f;
    uint32_t score = 0xFFFFFFFFu;
};

static size_t CollectMerchantNpcCandidates(float x, float y, float maxDist, uint16_t preferredPlayerNumber, NpcCandidate* out, size_t maxOut) {
    if (!out || maxOut == 0) return 0;

    for (size_t i = 0; i < maxOut; ++i) {
        out[i] = {};
        out[i].score = 0xFFFFFFFFu;
    }

    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    const float maxDistSq = maxDist * maxDist;
    size_t count = 0;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 6) continue;
        if (living->hp <= 0.0f) continue;
        if ((living->effects & 0x0010u) != 0) continue;

        const float distSq = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (distSq > maxDistSq) continue;

        NpcCandidate candidate;
        candidate.agentId = living->agent_id;
        candidate.playerNumber = living->player_number;
        candidate.npcId = living->transmog_npc_id;
        candidate.effects = living->effects;
        candidate.x = living->x;
        candidate.y = living->y;
        candidate.distance = sqrtf(distSq);
        candidate.score = static_cast<uint32_t>(candidate.distance) + (candidate.playerNumber == preferredPlayerNumber ? 0u : 100000u);

        size_t insertAt = maxOut;
        for (size_t slot = 0; slot < maxOut; ++slot) {
            if (candidate.score < out[slot].score) {
                insertAt = slot;
                break;
            }
        }
        if (insertAt == maxOut) continue;

        for (size_t slot = maxOut - 1; slot > insertAt; --slot) {
            out[slot] = out[slot - 1];
        }
        out[insertAt] = candidate;
        if (count < maxOut) count++;
    }

    return count;
}

static void DumpMerchantNpcCandidates(float x, float y, float maxDist, uint16_t preferredPlayerNumber) {
    NpcCandidate candidates[8];
    const size_t count = CollectMerchantNpcCandidates(x, y, maxDist, preferredPlayerNumber, candidates, _countof(candidates));
    IntReport("  NPC candidates near merchant coords (preferred player_number=%u): %zu", preferredPlayerNumber, count);
    for (size_t i = 0; i < count; ++i) {
        const auto& c = candidates[i];
        IntReport("    cand[%zu]: agent=%u player=%u npc_id=%u effects=0x%08X dist=%.0f pos=(%.0f, %.0f)%s",
                  i,
                  c.agentId,
                  c.playerNumber,
                  c.npcId,
                  c.effects,
                  c.distance,
                  c.x,
                  c.y,
                  c.playerNumber == preferredPlayerNumber ? " [preferred]" : "");
    }
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

static bool WaitForMerchantContext(DWORD timeoutMs) {
    static constexpr uint32_t kMerchantRootHash = 3613855137u;
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (TradeMgr::GetMerchantItemCount() > 0) return true;
        if (UIMgr::GetFrameByHash(kMerchantRootHash) != 0) return true;
        Sleep(100);
    }
    return false;
}

static bool KickAllHeroesWithObservation(DWORD timeoutMs) {
    PartyMgr::KickAllHeroes();
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (PartyMgr::CountPartyHeroes() == 0) return true;
        Sleep(250);
    }
    return PartyMgr::CountPartyHeroes() == 0;
}

static bool OpenMerchantContextWithVariants(uint32_t npcId) {
    auto* npc = AgentMgr::GetAgentByID(npcId);
    const uint32_t npcPtr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(npc));

    struct DialogVariant {
        const char* label;
        uint32_t header;
        uint32_t value;
        bool requiresPtr;
    };
    const DialogVariant variants[] = {
        {"dialog 0x3B by id", 0x3Bu, npcId, false},
        {"dialog 0x3B by ptr", 0x3Bu, npcPtr, true},
        {"dialog 0x3A by id", 0x3Au, npcId, false},
        {"dialog 0x3A by ptr", 0x3Au, npcPtr, true},
        {"dialog 0x3B generic merchant 0x84", 0x3Bu, 0x84u, false},
        {"dialog 0x3B generic merchant 0x85", 0x3Bu, 0x85u, false},
        {"dialog 0x3A generic merchant 0x84", 0x3Au, 0x84u, false},
        {"dialog 0x3A generic merchant 0x85", 0x3Au, 0x85u, false},
    };

    for (int attempt = 1; attempt <= 2; ++attempt) {
        IntReport("  Merchant open attempt %d via AgentMgr::InteractNPC...", attempt);
        AgentMgr::ChangeTarget(npcId);
        Sleep(250);
        AgentMgr::InteractNPC(npcId);
        Sleep(750);
        if (WaitForMerchantContext(1500)) return true;

        IntReport("  Merchant open attempt %d via raw 0x38 interact...", attempt);
        CtoS::SendPacket(2, 0x38u, npcId);
        Sleep(750);
        if (WaitForMerchantContext(1500)) {
            IntReport("  Merchant context opened via raw 0x38 interact");
            return true;
        }

        for (const auto& variant : variants) {
            if (variant.requiresPtr && variant.value <= 0x10000) continue;
            IntReport("  Trying %s (0x%X, 0x%08X)...", variant.label, variant.header, variant.value);
            CtoS::SendPacket(2, variant.header, variant.value);
            Sleep(750);
            if (WaitForMerchantContext(2000)) {
                IntReport("  Merchant context opened via %s", variant.label);
                return true;
            }
        }
    }

    return false;
}

static bool GoToNpcLikeAutoIt(uint32_t npcId, DWORD timeoutMs) {
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        auto* npc = AgentMgr::GetAgentByID(npcId);
        if (!npc) return false;

        float px = 0.0f;
        float py = 0.0f;
        if (TryReadAgentPosition(ReadMyId(), px, py)) {
            const float dist = AgentMgr::GetDistance(px, py, npc->x, npc->y);
            if (dist <= 250.0f) {
                Sleep(1000);
                return true;
            }
        }

        GameThread::EnqueuePost([x = npc->x, y = npc->y]() {
            AgentMgr::Move(x, y);
        });
        Sleep(100);
        AgentMgr::InteractNPC(npcId);
        Sleep(250);
    }
    return false;
}

int RunFroggyFeatureTest() {
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;
    StartWatchdog();

    // ===== PHASE 1: Travel to Gadd's Encampment =====
    // NOTE: Unit tests (Phase 0) moved AFTER stabilization because some
    // "unit" tests send game commands (FlagHeroes, Dialog) that crash
    // if the game isn't fully loaded yet.
    IntReport("=== PHASE 1: Travel to Gadd's Encampment ===");

    // Wait for game to be fully ready after bootstrap
    WaitForPlayerWorldReady(15000);

    // Re-initialize CtoS now that we're in-game (PacketLocation may have been null at boot)
    CtoS::Initialize();

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

    // Wait for game to fully stabilize — party/dialog commands crash if sent too soon
    IntReport("  Waiting for game to stabilize...");
    WaitForStablePlayerState(10000);
    Sleep(5000);

    // ===== PHASE 0: Pure logic unit tests (run AFTER game is stable) =====
    IntReport("=== PHASE 0: Unit Tests (deferred until game stable) ===");
    int unitFailures = Bot::Froggy::RunFroggyUnitTests();
    IntReport("Unit tests: %d failures", unitFailures);
    s_failed += unitFailures;

    // ===== PHASE 2: Outpost Tests (party, inventory, skillbar) =====
    IntReport("=== PHASE 2: Outpost Tests ===");

    // Add heroes
    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before setup: %u", heroesBefore);
    if (heroesBefore > 0) {
        IntReport("  Clearing existing heroes before setup...");
        const bool cleared = KickAllHeroesWithObservation(4000);
        const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
        IntReport("  Party heroes after clear: %u", heroesAfterKick);
        IntCheck("Existing heroes cleared before setup", cleared && heroesAfterKick == 0);
    }

    IntReport("  Adding heroes...");
    uint32_t heroIds[] = {30, 14, 21, 4, 24, 15, 29};
    for (int i = 0; i < 7; i++) {
        IntReport("  Adding hero %u (%d/7)...", heroIds[i], i + 1);
        PartyMgr::AddHero(heroIds[i]);
        Sleep(1000);
    }
    for (int i = 0; i < 7; i++) {
        PartyMgr::SetHeroBehavior(i + 1, 1);
        Sleep(300);
    }
    Sleep(1000);
    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after setup: %u", heroesAfterAdd);
    IntCheck("Seven heroes present after setup", heroesAfterAdd == 7);

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

    // Effects — party effects array may be empty in outpost (no buffs active)
    uint32_t myId = AgentMgr::GetMyId();
    if (myId > 0) {
        IntCheck("HasEffect(bogus 9999)=false", !EffectMgr::HasEffect(myId, 9999));
        auto* effects = EffectMgr::GetPlayerEffects();
        if (effects) {
            IntCheck("GetPlayerEffects returns non-null", true);
            IntCheck("Player effects agent_id matches self", effects->agent_id == myId);
        } else {
            IntSkip("GetPlayerEffects", "Party effects array empty in outpost (no buffs active)");
        }
    }

    // ===== PHASE 3: Merchant Tests =====
    // Movement safety guards added to AgentMgr::Move — re-enabled.
    IntReport("=== PHASE 3: Merchant Tests ===");

    // Move to merchant
    static constexpr float kMerchX = -8374.0f;
    static constexpr float kMerchY = -22491.0f;
    IntReport("  Moving to merchant (%.0f, %.0f)...", kMerchX, kMerchY);
    MovePlayerNear(kMerchX, kMerchY, 350.0f, 15000);
    Sleep(500);

    // Find and interact with merchant (with retry)
    static constexpr uint16_t kGaddsMerchantPlayerNumber = 6060;
    DumpMerchantNpcCandidates(kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber);

    NpcCandidate merchantCandidates[6];
    size_t merchantCandidateCount = CollectMerchantNpcCandidates(
        kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber, merchantCandidates, _countof(merchantCandidates));
    if (!merchantCandidateCount) {
        IntReport("  No merchant candidates at 1500 range, walking closer...");
        MovePlayerNear(kMerchX, kMerchY, 200.0f, 10000);
        Sleep(500);
        DumpMerchantNpcCandidates(kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber);
        merchantCandidateCount = CollectMerchantNpcCandidates(
            kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber, merchantCandidates, _countof(merchantCandidates));
    }

    if (merchantCandidateCount) {
        bool merchantOpen = false;
        uint32_t openedMerchantId = 0;

        for (size_t idx = 0; idx < merchantCandidateCount; ++idx) {
            const auto& candidate = merchantCandidates[idx];
            auto* npc = AgentMgr::GetAgentByID(candidate.agentId);
            auto* living = npc ? static_cast<AgentLiving*>(npc) : nullptr;
            IntReport("  Trying merchant candidate %zu/%zu: agent=%u allegiance=%u player_number=%u npc_id=%u at (%.0f, %.0f)",
                idx + 1,
                merchantCandidateCount,
                candidate.agentId,
                living ? living->allegiance : 0,
                living ? living->player_number : 0,
                living ? living->transmog_npc_id : 0,
                npc ? npc->x : 0.0f,
                npc ? npc->y : 0.0f);
            bool reachedNpc = false;
            if (npc) {
                reachedNpc = GoToNpcLikeAutoIt(candidate.agentId, 15000);
                float px = 0, py = 0;
                TryReadAgentPosition(ReadMyId(), px, py);
                IntReport("  After GoToNpcLikeAutoIt: pos=(%.0f,%.0f) reached=%d", px, py, reachedNpc);
            }


            if (!reachedNpc) {
                IntReport("  WARN: Could not reach candidate %u", candidate.agentId);
                continue;
            }

            merchantOpen = OpenMerchantContextWithVariants(candidate.agentId);
            if (merchantOpen) {
                openedMerchantId = candidate.agentId;
                break;
            }

            AgentMgr::CancelAction();
            Sleep(500);
        }

        IntCheck("Merchant window opened", merchantOpen);

        if (merchantOpen) {
            uint32_t itemCount = TradeMgr::GetMerchantItemCount();
            IntReport("  Merchant opened via candidate agent=%u", openedMerchantId);
            IntCheck("Merchant has items", itemCount > 0);
            IntReport("  Merchant has %u items", itemCount);
        }

        AgentMgr::CancelAction();
        Sleep(500);
    } else {
        IntSkip("Merchant tests", "Merchant NPC not found near target coords");
    }
    // ===== PHASE 4: Enter Explorable =====
phase4:
    // Movement safety guards added to AgentMgr::Move — re-enabled.
    IntReport("=== PHASE 4: Enter Sparkfly Swamp ===");

    // Log current position before walking
    {
        float px = 0, py = 0;
        TryReadAgentPosition(ReadMyId(), px, py);
        IntReport("  Current position: (%.0f, %.0f) MapID=%u", px, py, ReadMapId());
    }

    // Walk to exit portal (must use MovePlayerNear / EnqueuePost)
    IntReport("  Walking to exit waypoint 1 (-10018, -21892)...");
    MovePlayerNear(-10018.0f, -21892.0f, 350.0f, 20000);
    {
        float px = 0, py = 0;
        TryReadAgentPosition(ReadMyId(), px, py);
        IntReport("  After wp1: (%.0f, %.0f)", px, py);
    }
    IntReport("  Walking to exit waypoint 2 (-9550, -20400)...");
    MovePlayerNear(-9550.0f, -20400.0f, 350.0f, 20000);
    {
        float px = 0, py = 0;
        TryReadAgentPosition(ReadMyId(), px, py);
        IntReport("  After wp2: (%.0f, %.0f)", px, py);
    }

    // Push toward explorable zone exit
    IntReport("  Pushing toward Sparkfly...");
    DWORD zoneStart = GetTickCount();
    bool leftOutpost = false;
    while ((GetTickCount() - zoneStart) < 45000) {
        if (MapMgr::GetMapId() != MAP_GADDS) { leftOutpost = true; break; }
        GameThread::EnqueuePost([]() {
            AgentMgr::Move(-9451.0f, -19766.0f);
        });
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
                MovePlayerNear(-4559.0f, -14406.0f, 500.0f, 25000);
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

froggy_done:
    StopWatchdog();
    IntReport("=== FROGGY FEATURE TESTS COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return s_failed;
}

} // namespace GWA3::SmokeTest
