#include <gwa3/dungeon/DungeonVendor.h>

#include <gwa3/dungeon/DungeonDiagnostics.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>

namespace GWA3::DungeonVendor {

namespace {

void CallWait(WaitFn wait_fn, uint32_t ms) {
    if (wait_fn) {
        wait_fn(ms);
        return;
    }
    Sleep(ms);
}

void MoveToPoint(float x, float y, float threshold, MoveToPointFn move_to_point) {
    if (move_to_point) {
        move_to_point(x, y, threshold);
        return;
    }
    (void)DungeonNavigation::MoveToAndWait(x, y, threshold);
}

uint32_t MoveToNearestNpc(float anchorX, float anchorY, MoveToPointFn move_to_point,
                          const NpcServiceOptions& options) {
    MoveToPoint(anchorX, anchorY, options.anchor_threshold, move_to_point);

    const uint32_t npcId =
        DungeonInteractions::FindNearestNpc(anchorX, anchorY, options.search_radius);
    if (npcId == 0u) {
        return 0u;
    }

    auto* npc = AgentMgr::GetAgentByID(npcId);
    if (npc) {
        MoveToPoint(npc->x, npc->y, options.agent_threshold, move_to_point);
    }
    return npcId;
}

const char* Prefix(const char* prefix) {
    return prefix ? prefix : "DungeonVendor";
}

void LogMerchantOpenSnapshot(const char* label, uint32_t npcId, float npcX, float npcY, const char* prefix) {
    auto* me = AgentMgr::GetMyAgent();
    const float meX = me ? me->x : 0.0f;
    const float meY = me ? me->y : 0.0f;
    Log::Info("%s: %s npc=%u playerPos=(%.0f, %.0f) npcPos=(%.0f, %.0f) dist=%.0f target=%u items=%u",
              Prefix(prefix),
              label,
              npcId,
              meX,
              meY,
              npcX,
              npcY,
              me ? AgentMgr::GetDistance(meX, meY, npcX, npcY) : -1.0f,
              AgentMgr::GetTargetId(),
              TradeMgr::GetMerchantItemCount());
}

bool TryOpenMerchantContextCandidate(uint32_t npcId,
                                     float npcX,
                                     float npcY,
                                     WaitFn wait_ms,
                                     const MerchantContextNearCoordsOptions& options) {
    LogMerchantOpenSnapshot("Merchant pre-interact snapshot", npcId, npcX, npcY, options.log_prefix);

    AgentMgr::ChangeTarget(npcId);
    CallWait(wait_ms, options.merchant.change_target_delay_ms);

    Log::Info("%s: Merchant open via native interact loop...", Prefix(options.log_prefix));
    for (uint32_t nativeAttempt = 1u; nativeAttempt <= options.merchant.interact_attempts; ++nativeAttempt) {
        Log::Info("%s:   InteractNPC attempt %u: agent=%u",
                  Prefix(options.log_prefix),
                  nativeAttempt,
                  npcId);
        AgentMgr::InteractNPC(npcId);
        CallWait(wait_ms, options.merchant.interact_delay_ms);
    }
    CallWait(wait_ms, options.merchant.post_interact_delay_ms);
    if (WaitForMerchantContext(2000u, wait_ms, options.merchant.merchant_root_hash, options.merchant.wait_poll_ms)) {
        Log::Info("%s: Merchant window opened via native interact", Prefix(options.log_prefix));
        return true;
    }

    Log::Info("%s: Merchant native interact produced no context, trying raw packet fallback...",
              Prefix(options.log_prefix));
    for (uint32_t packetAttempt = 1u; packetAttempt <= options.merchant.interact_attempts; ++packetAttempt) {
        Log::Info("%s:   Raw GoNPC attempt %u: agent=%u",
                  Prefix(options.log_prefix),
                  packetAttempt,
                  npcId);
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        CallWait(wait_ms, options.merchant.interact_delay_ms);
    }
    CallWait(wait_ms, options.merchant.post_interact_delay_ms);
    if (WaitForMerchantContext(options.merchant.wait_timeout_ms,
                               wait_ms,
                               options.merchant.merchant_root_hash,
                               options.merchant.wait_poll_ms)) {
        Log::Info("%s: Merchant window opened via raw packet fallback", Prefix(options.log_prefix));
        return true;
    }

    LogMerchantOpenSnapshot("Merchant post-interact snapshot", npcId, npcX, npcY, options.log_prefix);
    return false;
}

bool TryMerchantCandidate(const DungeonDiagnostics::NearbyNpcCandidate& candidate,
                          std::size_t index,
                          std::size_t candidateCount,
                          bool fallback,
                          MoveToPointResultFn move_to_point,
                          WaitFn wait_ms,
                          const MerchantContextNearCoordsOptions& options) {
    const char* prefix = Prefix(options.log_prefix);
    if (fallback) {
        Log::Info("%s: Merchant fallback candidate %u/%u: agent=%u player=%u npcId=%u searchDist=%.0f playerDist=%.0f",
                  prefix,
                  static_cast<unsigned>(index + 1u),
                  static_cast<unsigned>(candidateCount),
                  candidate.agentId,
                  candidate.playerNumber,
                  candidate.npcId,
                  candidate.distanceToSearch,
                  candidate.distanceToPlayer);
    } else {
        Log::Info("%s: Merchant candidate %u/%u: agent=%u npcId=%u searchDist=%.0f playerDist=%.0f",
                  prefix,
                  static_cast<unsigned>(index + 1u),
                  static_cast<unsigned>(candidateCount),
                  candidate.agentId,
                  candidate.npcId,
                  candidate.distanceToSearch,
                  candidate.distanceToPlayer);
    }

    auto* npc = AgentMgr::GetAgentByID(candidate.agentId);
    const float npcX = npc ? npc->x : candidate.x;
    const float npcY = npc ? npc->y : candidate.y;
    if (move_to_point && !move_to_point(npcX, npcY, options.candidate_move_threshold)) {
        Log::Info("%s:   Could not move close enough to merchant candidate %u",
                  prefix,
                  candidate.agentId);
        return false;
    }

    if (TryOpenMerchantContextCandidate(candidate.agentId, npcX, npcY, wait_ms, options)) {
        if (fallback) {
            Log::Info("%s: Merchant fallback candidate %u opened merchant context",
                      prefix,
                      candidate.agentId);
        }
        return true;
    }

    Log::Info("%s:   Merchant candidate %u failed to open context, trying next candidate",
              prefix,
              candidate.agentId);
    return false;
}

} // namespace

bool WaitForMerchantContext(uint32_t timeoutMs, WaitFn wait_ms,
                            uint32_t merchant_root_hash, uint32_t poll_ms) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (TradeMgr::GetMerchantItemCount() > 0u) return true;
        if (UIMgr::IsFrameVisible(merchant_root_hash)) return true;
        CallWait(wait_ms, poll_ms);
    }
    return false;
}

bool OpenMerchantContextWithLegacyPacket(uint32_t npcId, WaitFn wait_ms,
                                         const MerchantContextOptions& options) {
    if (npcId == 0u) {
        return false;
    }

    AgentMgr::ChangeTarget(npcId);
    CallWait(wait_ms, options.change_target_delay_ms);
    for (uint32_t attempt = 0u; attempt < options.interact_attempts; ++attempt) {
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        CallWait(wait_ms, options.interact_delay_ms);
    }
    CallWait(wait_ms, options.post_interact_delay_ms);
    return WaitForMerchantContext(options.wait_timeout_ms, wait_ms,
                                  options.merchant_root_hash, options.wait_poll_ms);
}

bool OpenMerchantContextNearCoords(float searchX,
                                   float searchY,
                                   float searchRadius,
                                   MoveToPointResultFn move_to_point,
                                   WaitFn wait_ms,
                                   const MerchantContextNearCoordsOptions& options) {
    DungeonDiagnostics::NearbyNpcCandidate candidates[8]{};
    const std::size_t candidateCount =
        DungeonDiagnostics::CollectNearbyNpcCandidates(searchX, searchY, searchRadius, candidates, _countof(candidates));
    DungeonDiagnostics::LogNearbyNpcCandidates("Merchant", searchX, searchY, searchRadius, candidates, candidateCount);
    if (candidateCount == 0u) {
        Log::Info("%s: No merchant candidates found near target coords", Prefix(options.log_prefix));
        return false;
    }

    bool triedPreferredMerchant = false;
    bool attemptedAgents[8]{};
    if (options.preferred_player_number != 0u) {
        for (std::size_t i = 0u; i < candidateCount; ++i) {
            const auto& candidate = candidates[i];
            if (!candidate.agentId || candidate.playerNumber != options.preferred_player_number) {
                continue;
            }
            triedPreferredMerchant = true;
            attemptedAgents[i] = true;
            if (TryMerchantCandidate(candidate, i, candidateCount, false, move_to_point, wait_ms, options)) {
                return true;
            }
        }

        if (!triedPreferredMerchant) {
            Log::Info("%s: No preferred merchant candidate found (player_number=%u)",
                      Prefix(options.log_prefix),
                      options.preferred_player_number);
        }
    }

    for (std::size_t i = 0u; i < candidateCount; ++i) {
        if (attemptedAgents[i]) {
            continue;
        }
        const auto& candidate = candidates[i];
        if (!candidate.agentId) {
            continue;
        }
        if (TryMerchantCandidate(candidate, i, candidateCount, true, move_to_point, wait_ms, options)) {
            return true;
        }
    }

    return false;
}

int SellItemsAtMerchant(float anchorX, float anchorY, MoveToPointFn move_to_point,
                        DungeonItemActions::ItemFilterFn should_sell,
                        WaitFn wait_ms, const SellAtMerchantOptions& options) {
    const uint32_t npcId = MoveToNearestNpc(anchorX, anchorY, move_to_point, options.npc);
    if (npcId == 0u) {
        return 0;
    }
    if (!OpenMerchantContextWithLegacyPacket(npcId, wait_ms, options.merchant)) {
        return 0;
    }
    return DungeonItemActions::SellItems(should_sell, wait_ms, options.sell);
}

int DepositItemsAtStorage(float anchorX, float anchorY, MoveToPointFn move_to_point,
                          DungeonItemActions::ItemFilterFn should_store,
                          WaitFn wait_ms, const DepositAtStorageOptions& options) {
    const uint32_t npcId = MoveToNearestNpc(anchorX, anchorY, move_to_point, options.npc);
    if (npcId == 0u) {
        return 0;
    }

    AgentMgr::InteractNPC(npcId);
    CallWait(wait_ms, options.npc.interact_delay_ms);
    return DungeonItemActions::DepositItemsToStorage(should_store, wait_ms, options.deposit);
}

} // namespace GWA3::DungeonVendor
