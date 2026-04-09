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
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/game/Agent.h>

#include <Windows.h>

namespace GWA3::SmokeTest {

static int s_passed = 0;
static int s_failed = 0;
static int s_skipped = 0;
static constexpr bool kSkipPhase0UnitTestsForMerchantIsolation = true;
static constexpr bool kUseExperimentalAutoItMerchantInteractLane = false;
static constexpr bool kStopAfterMerchantFailureForCrashIsolation = true;
static constexpr bool kUseNativeInteractNpcForMerchantPhaseA = true;
static constexpr bool kUseLayeredMerchantIsolation = true;
static constexpr bool kStopAfterMerchantInteractOnly = false;
static constexpr bool kSkipPhase2OutpostSetupForMerchantIsolation = true;
static constexpr bool kSkipMerchantCancelActionForIsolation = true;
static constexpr bool kStopAfterFirstMerchantCandidateIsolation = true;
static constexpr bool kUseDialogByIdForFirstCandidateIsolation = false;
static constexpr bool kUseCandidateSpecificDialogAfterNativeInteract = true;
static constexpr bool kUseGameThreadDialogAfterNativeInteract = true;
static constexpr bool kSkipWatchdogForMerchantIsolation = true;
static constexpr bool kUseSessionMerchantHarnessInsideFroggy = false;
static constexpr bool kInvokeSessionMerchantHarnessBeforePhase1 = false;

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

struct LivingAgentSnapshot {
    uint32_t agentId = 0;
    uint32_t type = 0;
    uint32_t allegiance = 0;
    float hp = 0.0f;
    uint32_t effects = 0;
    uint16_t playerNumber = 0;
    uint32_t npcId = 0;
    float x = 0.0f;
    float y = 0.0f;
};

static bool TrySnapshotLivingAgent(uint32_t agentId, LivingAgentSnapshot& out) {
    out = {};
    auto* a = AgentMgr::GetAgentByID(agentId);
    if (!a) return false;
    __try {
        auto* living = static_cast<AgentLiving*>(a);
        out.agentId = living->agent_id;
        out.type = living->type;
        out.allegiance = living->allegiance;
        out.hp = living->hp;
        out.effects = living->effects;
        out.playerNumber = living->player_number;
        out.npcId = living->transmog_npc_id;
        out.x = living->x;
        out.y = living->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static uint32_t FindNearestNpc(float x, float y, float maxDist) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue; // NPC
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (d < bestDist) { bestDist = d; bestId = living.agentId; }
    }
    return bestId;
}

static uint32_t FindNearestNpcByPlayerNumber(float x, float y, float maxDist, uint16_t playerNumber) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue;
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;
        if (living.playerNumber != playerNumber) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (d < bestDist) { bestDist = d; bestId = living.agentId; }
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
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue;
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;

        const float distSq = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (distSq > maxDistSq) continue;

        NpcCandidate candidate;
        candidate.agentId = living.agentId;
        candidate.playerNumber = living.playerNumber;
        candidate.npcId = living.npcId;
        candidate.effects = living.effects;
        candidate.x = living.x;
        candidate.y = living.y;
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
        if (UIMgr::IsFrameVisible(kMerchantRootHash)) return true;
        Sleep(100);
    }
    return false;
}

static void ReportDialogSnapshot(const char* label) {
    const bool open = DialogMgr::IsDialogOpen();
    const uint32_t sender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t buttonCount = DialogMgr::GetButtonCount();
    IntReport("  %s: dialogOpen=%d sender=%u buttons=%u", label, open ? 1 : 0, sender, buttonCount);
    for (uint32_t i = 0; i < buttonCount && i < 6; ++i) {
        const auto* button = DialogMgr::GetButton(i);
        if (!button) continue;
        IntReport("    dialogButton[%u]: dialog_id=0x%X icon=%u skill=%u", i, button->dialog_id, button->button_icon, button->skill_id);
    }
}

static void ReportMerchantPreInteractState(const char* label, uint32_t npcId, float npcX, float npcY) {
    float meX = 0.0f, meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const uint32_t currentTarget = AgentMgr::GetTargetId();
    const bool dialogOpen = DialogMgr::IsDialogOpen();
    const uint32_t dialogSender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t dialogButtons = DialogMgr::GetButtonCount();
    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(3613855137u);
    const uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    const uint32_t heroCount = PartyMgr::CountPartyHeroes();
    const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    IntReport("  %s: npc=%u playerPos=(%.0f,%.0f) npcPos=(%.0f,%.0f) dist=%.0f target=%u dialogOpen=%d sender=%u buttons=%u merchantFrame=0x%08X items=%u heroes=%u",
        label,
        npcId,
        meX, meY,
        npcX, npcY,
        dist,
        currentTarget,
        dialogOpen ? 1 : 0,
        dialogSender,
        dialogButtons,
        static_cast<unsigned>(merchantFrame),
        merchantItems,
        heroCount);
}

static void DumpMerchantResolutionState(const NpcCandidate& candidate, float probeX, float probeY) {
    IntReport("  Merchant resolution dump: candidate agent=%u player=%u probe=(%.0f, %.0f)",
              candidate.agentId, candidate.playerNumber, probeX, probeY);
    IntReport("    globals: map=%u myId=%u maxAgents=%u agentBase=0x%08X",
              ReadMapId(),
              ReadMyId(),
              AgentMgr::GetMaxAgents(),
              static_cast<unsigned>(Offsets::AgentBase));

    LivingAgentSnapshot me;
    if (TrySnapshotLivingAgent(ReadMyId(), me)) {
        IntReport("    me: agent=%u type=0x%X allegiance=%u hp=%.2f effects=0x%08X player=%u npc_id=%u pos=(%.0f, %.0f)",
                  me.agentId, me.type, me.allegiance, me.hp, me.effects,
                  me.playerNumber, me.npcId, me.x, me.y);
    } else {
        IntReport("    me: agent %u not readable", ReadMyId());
    }

    LivingAgentSnapshot byId;
    if (TrySnapshotLivingAgent(candidate.agentId, byId)) {
        IntReport("    by-id: agent=%u type=0x%X allegiance=%u hp=%.2f effects=0x%08X player=%u npc_id=%u pos=(%.0f, %.0f)",
                  byId.agentId, byId.type, byId.allegiance, byId.hp, byId.effects,
                  byId.playerNumber, byId.npcId, byId.x, byId.y);
    } else {
        IntReport("    by-id: agent %u not readable", candidate.agentId);
    }

    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    uint32_t samePlayerCount = 0;
    for (uint32_t i = 1; i < maxAgents && samePlayerCount < 8; ++i) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.playerNumber != candidate.playerNumber) continue;
        ++samePlayerCount;
        IntReport("    same-player[%u]: agent=%u type=0x%X allegiance=%u hp=%.2f effects=0x%08X npc_id=%u pos=(%.0f, %.0f)",
                  samePlayerCount - 1,
                  living.agentId,
                  living.type,
                  living.allegiance,
                  living.hp,
                  living.effects,
                  living.npcId,
                  living.x,
                  living.y);
    }
    if (!samePlayerCount) {
        IntReport("    same-player: no readable agents with player_number=%u", candidate.playerNumber);
    }

    NpcCandidate nearby[8];
    const size_t nearbyCount = CollectMerchantNpcCandidates(
        probeX, probeY, 500.0f, candidate.playerNumber, nearby, _countof(nearby));
    IntReport("    nearby around probe: %zu candidates", nearbyCount);
    for (size_t i = 0; i < nearbyCount; ++i) {
        const auto& c = nearby[i];
        IntReport("      nearby[%zu]: agent=%u player=%u npc_id=%u effects=0x%08X dist=%.0f pos=(%.0f, %.0f)%s",
                  i,
                  c.agentId,
                  c.playerNumber,
                  c.npcId,
                  c.effects,
                  c.distance,
                  c.x,
                  c.y,
                  c.playerNumber == candidate.playerNumber ? " [same-player]" : "");
    }
}

static bool TryDialogButtonsFromDialogMgr() {
    const uint32_t buttonCount = DialogMgr::GetButtonCount();
    for (uint32_t i = 0; i < buttonCount && i < 6; ++i) {
        const auto* button = DialogMgr::GetButton(i);
        if (!button || button->dialog_id == 0) continue;
        IntReport("  Trying dialog button from DialogMgr: idx=%u dialog_id=0x%X", i, button->dialog_id);
        QuestMgr::Dialog(button->dialog_id);
        Sleep(750);
        if (WaitForMerchantContext(2000)) {
            IntReport("  Merchant context opened via dialog button 0x%X", button->dialog_id);
            return true;
        }
    }
    return false;
}

static void EmitMerchantScreenshotMarker(const char* reason) {
    IntReport("MERCHANT_SCREENSHOT_NOW %s", reason ? reason : "");
}

static bool KickAllHeroesWithObservation(DWORD timeoutMs) {
    PartyMgr::DebugDumpPartyState("Froggy before KickAllHeroes");
    PartyMgr::KickAllHeroes();
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (PartyMgr::CountPartyHeroes() == 0) {
            PartyMgr::DebugDumpPartyState("Froggy after KickAllHeroes success");
            return true;
        }
        if ((GetTickCount() - start) > 2000 && (GetTickCount() - start) < 2250) {
            IntReport("  KickAllHeroes still pending after 2s, reissuing reliable per-hero clear...");
            PartyMgr::DebugDumpPartyState("Froggy before KickAllHeroes reissue");
            PartyMgr::KickAllHeroes();
        }
        Sleep(250);
    }
    PartyMgr::DebugDumpPartyState("Froggy after KickAllHeroes timeout");
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
        {"dialog 0x3A generic merchant 0x84", Packets::DIALOG_SEND_LIVING, 0x84u, false},
        {"dialog 0x3A generic merchant 0x85", Packets::DIALOG_SEND_LIVING, 0x85u, false},
        {"dialog 0x3A by id", Packets::DIALOG_SEND_LIVING, npcId, false},
        {"dialog 0x3A by ptr", Packets::DIALOG_SEND_LIVING, npcPtr, true},
        {"dialog 0x3B generic merchant 0x84", Packets::DIALOG_SEND, 0x84u, false},
        {"dialog 0x3B generic merchant 0x85", Packets::DIALOG_SEND, 0x85u, false},
        {"dialog 0x3B by id", Packets::DIALOG_SEND, npcId, false},
        {"dialog 0x3B by ptr", Packets::DIALOG_SEND, npcPtr, true},
    };

    for (int attempt = 1; attempt <= 2; ++attempt) {
        IntReport("  Merchant open attempt %d phase A: experimental AutoIt-style command lane interact...", attempt);
        if (kUseExperimentalAutoItMerchantInteractLane &&
            !CtoSHook::SendPacketCommand(3, Packets::INTERACT_LIVING, npcId, 0u)) {
            IntReport("  WARN: experimental command lane enqueue failed, falling back to standard CtoS interact");
            CtoS::SendPacket(3, Packets::INTERACT_LIVING, npcId, 0u);
        } else if (!kUseExperimentalAutoItMerchantInteractLane) {
            if (kUseNativeInteractNpcForMerchantPhaseA) {
                IntReport("  Experimental AutoIt-style command lane disabled; using AgentMgr::InteractNPC");
                AgentMgr::InteractNPC(npcId);
            } else {
                // Layered isolation starts with the exact Gadd's merchant harness
                // behavior that is currently known-stable: a single 0x38 interact,
                // then stop before any dialog send.
                if (kUseLayeredMerchantIsolation) {
                    IntReport("  Experimental AutoIt-style command lane disabled; using layered harness-matched interact isolation");
                    IntReport("    phase A step 0: ChangeTarget(%u)", npcId);
                    AgentMgr::ChangeTarget(npcId);
                    Sleep(250);
                    IntReport("    phase A step 1: SendPacket(2, 0x38, %u)", npcId);
                    CtoS::SendPacket(2, Packets::INTERACT_LIVING, npcId);
                    IntReport("    phase A step 1 complete");
                    Sleep(750);
                    IntReport("    phase A step 2: SendPacket(2, 0x38, %u)", npcId);
                    CtoS::SendPacket(2, Packets::INTERACT_LIVING, npcId);
                    IntReport("    phase A step 2 complete");
                } else {
                    // Match the working GWA2 maintenance helper more closely:
                    // GoNPC($npc), Sleep(500), Dialog($npc).
                    IntReport("  Experimental AutoIt-style command lane disabled; using maintenance-style 0x38 + dialog(ptr)");
                    IntReport("    phase A step 1: SendPacket(2, 0x38, %u)", npcId);
                    CtoS::SendPacket(2, Packets::INTERACT_LIVING, npcId);
                    IntReport("    phase A step 1 complete");
                    Sleep(500);
                    if (kUseDialogByIdForFirstCandidateIsolation) {
                        IntReport("    phase A step 2: SendPacket(2, 0x3A, npcId=%u)", npcId);
                        CtoS::SendPacket(2, Packets::DIALOG_SEND_LIVING, npcId);
                        IntReport("    phase A step 2 complete");
                    } else if (npcPtr > 0x10000) {
                        IntReport("    phase A step 2: SendPacket(2, 0x3A, npcPtr=0x%08X)", npcPtr);
                        CtoS::SendPacket(2, Packets::DIALOG_SEND_LIVING, npcPtr);
                        IntReport("    phase A step 2 complete");
                    } else {
                        IntReport("    phase A step 2 skipped: npcPtr unavailable, falling back to dialog by id");
                        CtoS::SendPacket(2, Packets::DIALOG_SEND_LIVING, npcId);
                        IntReport("    phase A step 2 fallback complete");
                    }
                }
            }
        }
        Sleep(kUseLayeredMerchantIsolation ? 4000 : 1500);
        ReportDialogSnapshot(kUseLayeredMerchantIsolation
            ? "After layered interact-only wait"
            : "After experimental living-interact only");
        if (WaitForMerchantContext(kUseLayeredMerchantIsolation ? 1500 : 750)) {
            IntReport(kUseLayeredMerchantIsolation
                ? "  Merchant context opened asynchronously after layered interact-only wait"
                : "  Merchant context opened via experimental AutoIt-style living-interact");
            EmitMerchantScreenshotMarker(kUseLayeredMerchantIsolation
                ? "layered_interact_only_wait"
                : "experimental_autoit_living_interact_only");
            return true;
        }
        if (TryDialogButtonsFromDialogMgr()) return true;
        if (kUseLayeredMerchantIsolation && kStopAfterMerchantInteractOnly) {
            IntReport("  Layered merchant isolation: stopping after interact-only wait stage");
            return false;
        }

        if (kUseCandidateSpecificDialogAfterNativeInteract) {
            uint32_t dialogValue = (kUseDialogByIdForFirstCandidateIsolation || npcPtr <= 0x10000) ? npcId : npcPtr;
            if (kUseDialogByIdForFirstCandidateIsolation || npcPtr <= 0x10000) {
                if (kUseGameThreadDialogAfterNativeInteract) {
                    IntReport("  Merchant open attempt %d phase B: GameThread QuestMgr::Dialog(npcId=%u) after native interact...", attempt, npcId);
                    GameThread::EnqueuePost([dialogValue]() {
                        QuestMgr::Dialog(dialogValue);
                    });
                } else {
                    IntReport("  Merchant open attempt %d phase B: explicit 0x3A npcId=%u after native interact...", attempt, npcId);
                    CtoS::SendPacket(2, Packets::DIALOG_SEND_LIVING, npcId);
                }
            } else {
                if (kUseGameThreadDialogAfterNativeInteract) {
                    IntReport("  Merchant open attempt %d phase B: GameThread QuestMgr::Dialog(npcPtr=0x%08X) after native interact...", attempt, npcPtr);
                    GameThread::EnqueuePost([dialogValue]() {
                        QuestMgr::Dialog(dialogValue);
                    });
                } else {
                    IntReport("  Merchant open attempt %d phase B: explicit 0x3A npcPtr=0x%08X after native interact...", attempt, npcPtr);
                    CtoS::SendPacket(2, Packets::DIALOG_SEND_LIVING, npcPtr);
                }
            }
        } else {
            IntReport("  Merchant open attempt %d phase B: explicit 0x3A 0x84 after interact...", attempt);
            CtoS::SendPacket(2, Packets::DIALOG_SEND_LIVING, 0x84u);
        }
        Sleep(750);
        ReportDialogSnapshot(kUseCandidateSpecificDialogAfterNativeInteract
            ? "After explicit native-interact + 0x3A candidate dialog"
            : "After explicit living-interact + 0x84");
        if (WaitForMerchantContext(1500)) {
            IntReport("  Merchant context opened via explicit living-interact + 0x84");
            EmitMerchantScreenshotMarker("explicit_living_interact_0x84");
            return true;
        }
        if (TryDialogButtonsFromDialogMgr()) return true;

        for (const auto& variant : variants) {
            if (variant.requiresPtr && variant.value <= 0x10000) continue;
            DialogMgr::ClearDialog();
            IntReport("  Trying %s (0x%X, 0x%08X)...", variant.label, variant.header, variant.value);
            CtoS::SendPacket(2, variant.header, variant.value);
            Sleep(750);
            ReportDialogSnapshot(variant.label);
            if (WaitForMerchantContext(2000)) {
                IntReport("  Merchant context opened via %s", variant.label);
                EmitMerchantScreenshotMarker(variant.label);
                return true;
            }
            if (TryDialogButtonsFromDialogMgr()) return true;
        }
    }

    return false;
}

static uint32_t ResolveMerchantCandidateAgentId(const NpcCandidate& candidate) {
    if (candidate.playerNumber) {
        const uint32_t byPlayerNumber = FindNearestNpcByPlayerNumber(
            candidate.x, candidate.y, 350.0f, candidate.playerNumber);
        if (byPlayerNumber) return byPlayerNumber;
    }
    LivingAgentSnapshot byId;
    if (TrySnapshotLivingAgent(candidate.agentId, byId)) return candidate.agentId;
    return 0;
}

static bool GoToNpcLikeAutoIt(const NpcCandidate& candidate, DWORD timeoutMs) {
    DWORD start = GetTickCount();
    uint32_t unresolvedCount = 0;
    while ((GetTickCount() - start) < timeoutMs) {
        const uint32_t npcId = ResolveMerchantCandidateAgentId(candidate);
        LivingAgentSnapshot npc;
        if (!npcId || !TrySnapshotLivingAgent(npcId, npc)) {
            IntReport("  AutoIt-style NPC approach: candidate player=%u unresolved, retrying...",
                      candidate.playerNumber);
            if (++unresolvedCount >= 3) {
                float px = 0.0f;
                float py = 0.0f;
                TryReadAgentPosition(ReadMyId(), px, py);
                DumpMerchantResolutionState(candidate, px, py);
                IntReport("  AutoIt-style NPC approach aborted after %u unresolved retries for player=%u",
                          unresolvedCount,
                          candidate.playerNumber);
                return false;
            }
            Sleep(200);
            continue;
        }
        unresolvedCount = 0;

        float px = 0.0f;
        float py = 0.0f;
        if (TryReadAgentPosition(ReadMyId(), px, py)) {
            const float dist = AgentMgr::GetDistance(px, py, npc.x, npc.y);
            IntReport("  Session-style NPC approach: player=(%.0f, %.0f) npc=(%.0f, %.0f) dist=%.0f",
                      px, py, npc.x, npc.y, dist);
            const bool reached = MovePlayerNear(npc.x, npc.y, 70.0f, 2000);
            float pxAfter = 0.0f;
            float pyAfter = 0.0f;
            TryReadAgentPosition(ReadMyId(), pxAfter, pyAfter);
            const float distAfter = AgentMgr::GetDistance(pxAfter, pyAfter, npc.x, npc.y);
            IntReport("  Session-style NPC approach result: reached=%d pos=(%.0f, %.0f) dist=%.0f",
                      reached ? 1 : 0, pxAfter, pyAfter, distAfter);
            if (reached || distAfter <= 80.0f) {
                return true;
            }
        }
        Sleep(300);
    }
    IntReport("  Session-style NPC approach timed out for candidate player=%u after %ums",
              candidate.playerNumber, timeoutMs);
    return false;
}

static bool MovePlayerNearMerchantIsolation(float x, float y, float threshold, int timeoutMs) {
    const DWORD start = GetTickCount();
    GameThread::EnqueuePost([x, y]() {
        AgentMgr::Move(x, y);
    });
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        Sleep(500);

        float px = 0.0f;
        float py = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), px, py)) continue;

        const float dist = AgentMgr::GetDistance(px, py, x, y);
        IntReport("  Merchant move probe: pos=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f",
                  px, py, x, y, dist);
        if (dist <= threshold) {
            return true;
        }
    }
    return false;
}

int RunFroggyFeatureTest() {
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;
    if (kSkipWatchdogForMerchantIsolation) {
        IntReport("Skipping watchdog startup for merchant isolation");
    } else {
        StartWatchdog();
    }

    if (kUseSessionMerchantHarnessInsideFroggy && kInvokeSessionMerchantHarnessBeforePhase1) {
        IntReport("Invoking RunMerchantQuoteTest() before Phase 1 for isolation");
        RunMerchantQuoteTest();
        goto froggy_done;
    }

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

    // ===== PHASE 2: Outpost Tests (party, inventory, skillbar) =====
    IntReport("=== PHASE 2: Outpost Tests ===");

    if (kSkipPhase2OutpostSetupForMerchantIsolation) {
        IntSkip("Phase 2 outpost setup", "Temporarily skipped while isolating merchant interact crash");
        goto phase3;
    }

    // Add heroes
    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before setup: %u", heroesBefore);
    uint32_t heroIds[] = {30, 14, 21, 4, 24, 15, 29};
    if (heroesBefore > 0) {
        IntReport("  Clearing existing heroes before setup...");
        const bool cleared = KickAllHeroesWithObservation(4000);
        const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
        IntReport("  Party heroes after clear: %u", heroesAfterKick);
        IntCheck("Existing heroes cleared before setup", cleared && heroesAfterKick == 0);
    }

    IntReport("  Adding heroes...");
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

    // ===== PHASE 0: Unit Tests (run after outpost setup is stable) =====
    // These tests include stateful hero operations. Running them after the
    // initial party setup keeps Phase 2 aligned with the passing integration
    // sequence, where hero clear/setup is the first party mutation observed.
    if (kSkipPhase0UnitTestsForMerchantIsolation) {
        IntSkip("Phase 0 unit tests", "Temporarily skipped while isolating merchant crash");
    } else {
        IntReport("=== PHASE 0: Unit Tests (deferred until after outpost setup) ===");
        int unitFailures = Bot::Froggy::RunFroggyUnitTests();
        IntReport("Unit tests: %d failures", unitFailures);
        s_failed += unitFailures;
    }

phase3:
    // ===== PHASE 3: Merchant Tests =====
    // Movement safety guards added to AgentMgr::Move — re-enabled.
    IntReport("=== PHASE 3: Merchant Tests ===");

    if (kUseSessionMerchantHarnessInsideFroggy) {
        IntReport("  Delegating merchant phase to RunMerchantQuoteTest() harness for isolation");
        RunMerchantQuoteTest();
        goto froggy_done;
    }

    // Move to merchant
    static constexpr float kMerchX = -8374.0f;
    static constexpr float kMerchY = -22491.0f;
    IntReport("  Moving to merchant (%.0f, %.0f)...", kMerchX, kMerchY);
    const bool reachedMerchantArea = MovePlayerNearMerchantIsolation(kMerchX, kMerchY, 550.0f, 15000);
    IntCheck("Reached merchant area", reachedMerchantArea);
    Sleep(500);

    // Find and interact with merchant (with retry)
    static constexpr uint16_t kGaddsMerchantPlayerNumber = 6060;
    DumpMerchantNpcCandidates(kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber);

    NpcCandidate merchantCandidates[6];
    size_t merchantCandidateCount = CollectMerchantNpcCandidates(
        kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber, merchantCandidates, _countof(merchantCandidates));
    if (!merchantCandidateCount) {
        IntReport("  No merchant candidates at 1500 range, walking closer...");
        MovePlayerNearMerchantIsolation(kMerchX, kMerchY, 200.0f, 10000);
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
            const uint32_t resolvedAgentId = ResolveMerchantCandidateAgentId(candidate);
            LivingAgentSnapshot npc;
            const bool haveNpc = resolvedAgentId && TrySnapshotLivingAgent(resolvedAgentId, npc);
            IntReport("  Trying merchant candidate %zu/%zu: agent=%u allegiance=%u player_number=%u npc_id=%u at (%.0f, %.0f)",
                idx + 1,
                merchantCandidateCount,
                resolvedAgentId ? resolvedAgentId : candidate.agentId,
                haveNpc ? npc.allegiance : 0,
                haveNpc ? npc.playerNumber : candidate.playerNumber,
                haveNpc ? npc.npcId : 0,
                haveNpc ? npc.x : 0.0f,
                haveNpc ? npc.y : 0.0f);
            bool reachedNpc = false;
            if (haveNpc) {
                reachedNpc = GoToNpcLikeAutoIt(candidate, 15000);
                float px = 0, py = 0;
                TryReadAgentPosition(ReadMyId(), px, py);
                IntReport("  After direct merchant approach: pos=(%.0f,%.0f) reached=%d", px, py, reachedNpc);
            }


            if (!reachedNpc) {
                IntReport("  WARN: Could not reach candidate %u", resolvedAgentId ? resolvedAgentId : candidate.agentId);
                continue;
            }

            if (haveNpc) {
                ReportMerchantPreInteractState("Froggy pre-interact snapshot",
                    resolvedAgentId,
                    npc.x,
                    npc.y);
            }

            merchantOpen = OpenMerchantContextWithVariants(resolvedAgentId);
            if (merchantOpen) {
                openedMerchantId = resolvedAgentId;
                break;
            }

            if (kUseLayeredMerchantIsolation && kStopAfterFirstMerchantCandidateIsolation) {
                IntReport("  Layered merchant isolation: stopping after first candidate");
                goto froggy_done;
            }

            if (kSkipMerchantCancelActionForIsolation) {
                IntReport("  Skipping post-merchant CancelAction for isolation");
            } else {
                IntReport("  Post-merchant cleanup: CancelAction()");
                AgentMgr::CancelAction();
                IntReport("  Post-merchant cleanup: CancelAction complete");
                Sleep(500);
            }
        }

        IntCheck("Merchant window opened", merchantOpen);

        if (!merchantOpen && kStopAfterMerchantFailureForCrashIsolation) {
            IntReport("  Stopping after merchant failure for crash isolation");
            goto froggy_done;
        }

        if (merchantOpen) {
            uint32_t itemCount = TradeMgr::GetMerchantItemCount();
            IntReport("  Merchant opened via candidate agent=%u", openedMerchantId);
            IntCheck("Merchant has items", itemCount > 0);
            IntReport("  Merchant has %u items", itemCount);
        }

        if (kSkipMerchantCancelActionForIsolation) {
            IntReport("  Skipping final merchant CancelAction for isolation");
        } else {
            IntReport("  Final merchant cleanup: CancelAction()");
            AgentMgr::CancelAction();
            IntReport("  Final merchant cleanup: CancelAction complete");
            Sleep(500);
        }
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
    IntReport("Entering froggy_done");
    if (kSkipWatchdogForMerchantIsolation) {
        IntReport("Watchdog was skipped for merchant isolation");
    } else {
        IntReport("Stopping watchdog (non-blocking)...");
        StopWatchdog(false);
        IntReport("Watchdog stop requested");
    }
    IntReport("=== FROGGY FEATURE TESTS COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return s_failed;
}

} // namespace GWA3::SmokeTest
