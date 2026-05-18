#include <gwa3/managers/MerchantMgr.h>

#include <gwa3/advanced/Diagnostics.h>
#include <gwa3/advanced/Effects.h>
#include <gwa3/advanced/Inventory.h>
#include <gwa3/advanced/Interactions.h>
#include <gwa3/advanced/Runtime.h>
#include <gwa3/advanced/Waypoint.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>

namespace GWA3::MerchantMgr {

namespace {

constexpr std::size_t kMaxMerchantContextCandidates = 32u;

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
    (void)AdvancedWaypoint::MoveToAndWait(x, y, threshold);
}

uint32_t MoveToNearestNpc(float anchorX, float anchorY, MoveToPointFn move_to_point,
                          const NpcServiceOptions& options) {
    MoveToPoint(anchorX, anchorY, options.anchor_threshold, move_to_point);

    const uint32_t npcId =
        AdvancedInteractions::FindNearestNpc(anchorX, anchorY, options.search_radius);
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
    return prefix ? prefix : "MerchantMgr";
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
              MerchantMgr::GetMerchantItemCount());
}

bool MoveToPointWithResult(float x, float y, float threshold, MoveToPointResultFn move_to_point) {
    if (move_to_point) {
        return move_to_point(x, y, threshold);
    }
    return AdvancedWaypoint::MoveToAndWait(x, y, threshold).arrived;
}

bool HasOpenStandardMerchantStock() {
    const uint32_t merchantItemCount = MerchantMgr::GetMerchantItemCount();
    if (merchantItemCount == 0u) {
        return false;
    }

    static constexpr uint32_t kStandardMerchantModels[] = {
        ItemModelIds::IDENTIFICATION_KIT,
        ItemModelIds::SUPERIOR_IDENTIFICATION_KIT,
        ItemModelIds::SALVAGE_KIT,
        ItemModelIds::EXPERT_SALVAGE_KIT,
        ItemModelIds::SUPERIOR_SALVAGE_KIT,
    };

    // Scan the visible merchant stock directly. GetMerchantItemIdByModelId falls
    // back through material-trader virtual items and emits a full diagnostic
    // dump for misses, which is too noisy and too slow for candidate probing.
    for (uint32_t position = 1u; position <= merchantItemCount; ++position) {
        const Item* item = MerchantMgr::GetMerchantItemByPosition(position);
        if (!item) {
            continue;
        }
        for (uint32_t modelId : kStandardMerchantModels) {
            if (item->model_id == modelId) {
                return true;
            }
        }
    }
    return false;
}

bool WaitForStandardMerchantStock(uint32_t timeoutMs, WaitFn wait_ms, uint32_t poll_ms) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (HasOpenStandardMerchantStock()) {
            return true;
        }
        CallWait(wait_ms, poll_ms);
    }
    return HasOpenStandardMerchantStock();
}

bool ValidateMerchantContext(uint32_t npcId,
                             const char* open_path,
                             const MerchantContextNearCoordsOptions& options) {
    if (!options.require_standard_merchant_stock) {
        return true;
    }

    if (HasOpenStandardMerchantStock()) {
        Log::Info("%s: Merchant candidate %u opened standard merchant stock via %s",
                  Prefix(options.log_prefix),
                  npcId,
                  open_path ? open_path : "unknown path");
        return true;
    }

    Log::Warn("%s: Merchant candidate %u opened non-standard trader stock via %s; "
              "rejecting for maintenance (items=%u)",
              Prefix(options.log_prefix),
              npcId,
              open_path ? open_path : "unknown path",
              MerchantMgr::GetMerchantItemCount());
    return false;
}

bool WaitForValidatedMerchantContext(uint32_t npcId,
                                     const char* open_path,
                                     uint32_t timeoutMs,
                                     WaitFn wait_ms,
                                     const MerchantContextNearCoordsOptions& options) {
    if (!options.require_standard_merchant_stock) {
        return WaitForMerchantContext(timeoutMs,
                                      wait_ms,
                                      options.merchant.merchant_root_hash,
                                      options.merchant.wait_poll_ms);
    }

    if (WaitForStandardMerchantStock(timeoutMs, wait_ms, options.merchant.wait_poll_ms)) {
        Log::Info("%s: Merchant candidate %u opened standard merchant stock via %s",
                  Prefix(options.log_prefix),
                  npcId,
                  open_path ? open_path : "unknown path");
        return true;
    }

    if (MerchantMgr::GetMerchantItemCount() > 0u) {
        ValidateMerchantContext(npcId, open_path, options);
    }
    return false;
}

uint32_t ResolveOutpostMapId(uint32_t configured_outpost_map_id,
                             const MaintenanceLocation& location,
                             const MaintenanceStateOptions& options) {
    if (configured_outpost_map_id != 0u) return configured_outpost_map_id;
    if (location.outpost_map_id != 0u) return location.outpost_map_id;
    return options.fallback_outpost_map_id;
}

bool EnsureMaintenanceOutpost(uint32_t outpost_map_id,
                              const MaintenanceStateOptions& options,
                              const char* state_label) {
    const char* prefix = Prefix(options.log_prefix);
    if (outpost_map_id == 0u) {
        Log::Warn("%s: %s missing outpost map id", prefix, state_label);
        return false;
    }

    const uint32_t map_id = MapMgr::GetMapId();
    if (map_id != outpost_map_id) {
        Log::Info("%s: %s traveling to outpost map %u from map %u",
                  prefix,
                  state_label,
                  outpost_map_id,
                  map_id);
        if (!AdvancedRuntime::EnsureOutpostReady(
                outpost_map_id,
                options.outpost_ready_timeout_ms,
                state_label)) {
            Log::Warn("%s: %s failed to reach outpost map %u",
                      prefix,
                      state_label,
                      outpost_map_id);
            return false;
        }
    }

    if (!AdvancedRuntime::WaitForTownRuntimeReady(outpost_map_id, options.town_runtime_timeout_ms)) {
        Log::Info("%s: %s outpost runtime not ready after travel; waiting for next tick",
                  prefix,
                  state_label);
        CallWait(options.wait_ms, options.retry_wait_ms);
        return false;
    }

    return true;
}

void LogMaintenanceInventorySnapshot(const char* prefix) {
    const uint32_t free_slots = AdvancedInventory::CountFreeSlots();
    const uint32_t id_kits = AdvancedInventory::CountItemByModel(ItemModelIds::IDENTIFICATION_KIT) +
                             AdvancedInventory::CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
    const uint32_t salv_kits = AdvancedInventory::CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT) +
                               AdvancedInventory::CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT);
    const uint32_t char_gold = ItemMgr::GetGoldCharacter();
    const uint32_t effect_count = AdvancedEffects::GetPlayerEffectCount();
    const bool has_blessing = AdvancedEffects::HasAnyDungeonBlessing();
    const bool has_conset = AdvancedEffects::HasFullConset();

    Log::Info("%s: Inventory: %u free slots, %u ID kits, %u salvage kits, %u gold",
              prefix,
              free_slots,
              id_kits,
              salv_kits,
              char_gold);
    Log::Info("%s: Buffs: %u effects, blessing=%s, conset=%s",
              prefix,
              effect_count,
              has_blessing ? "yes" : "no",
              has_conset ? "yes" : "no");
}

void ClaimConfiguredUnclaimedItems(const MaintenanceStateOptions& options) {
    if (!options.unclaimed_items.model_ids || options.unclaimed_items.model_id_count == 0u) {
        return;
    }

    AdvancedInventory::UnclaimedItemClaimOptions claim_options = options.unclaimed_items;
    if (!claim_options.log_prefix) {
        claim_options.log_prefix = options.log_prefix;
    }
    if (!claim_options.wait_ms) {
        claim_options.wait_ms = options.wait_ms;
    }

    const auto result = AdvancedInventory::ClaimUnclaimedItemsByModel(claim_options);
    if (result.accepted) {
        Log::Info("%s: claimed unclaimed town items quantity=%u",
                  Prefix(options.log_prefix),
                  result.matching_quantity);
    }
}

bool PerformMaintenanceAtMerchant(const MaintenanceLocation& location,
                                  const MaintenanceStateOptions& options,
                                  const MaintenanceMgr::Config& maintenance_cfg,
                                  const char* merchant_failure_label,
                                  bool deposit_gold_on_failure) {
    const char* prefix = Prefix(options.log_prefix);
    (void)MoveToPointWithResult(
        location.merchant_x,
        location.merchant_y,
        location.merchant_move_threshold,
        options.move_to_point);
    if (OpenMaintenanceMerchantContext(location, options.move_to_point, options.wait_ms, options.log_prefix)) {
        MaintenanceMgr::PerformMaintenance(maintenance_cfg);
        CallWait(options.wait_ms, options.post_maintenance_wait_ms);
        return true;
    }

    Log::Info("%s: %s", prefix, merchant_failure_label);
    if (deposit_gold_on_failure) {
        MaintenanceMgr::DepositGold(options.deposit_gold_keep_on_char);
    }
    return false;
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
    if (WaitForValidatedMerchantContext(npcId, "native interact", 2000u, wait_ms, options)) {
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
    if (WaitForValidatedMerchantContext(npcId,
                                        "raw packet fallback",
                                        options.merchant.wait_timeout_ms,
                                        wait_ms,
                                        options)) {
        Log::Info("%s: Merchant window opened via raw packet fallback", Prefix(options.log_prefix));
        return true;
    }

    LogMerchantOpenSnapshot("Merchant post-interact snapshot", npcId, npcX, npcY, options.log_prefix);
    return false;
}

bool TryMerchantCandidate(const AdvancedDiagnostics::NearbyNpcCandidate& candidate,
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
        if (MerchantMgr::GetMerchantItemCount() > 0u) return true;
        if (merchant_root_hash != 0u && UIMgr::IsFrameVisible(merchant_root_hash)) return true;
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
    AdvancedDiagnostics::NearbyNpcCandidate candidates[kMaxMerchantContextCandidates]{};
    const std::size_t candidateCount =
        AdvancedDiagnostics::CollectNearbyNpcCandidates(searchX, searchY, searchRadius, candidates, _countof(candidates));
    AdvancedDiagnostics::LogNearbyNpcCandidates("Merchant", searchX, searchY, searchRadius, candidates, candidateCount);
    if (candidateCount == 0u) {
        Log::Info("%s: No merchant candidates found near target coords", Prefix(options.log_prefix));
        return false;
    }

    bool triedPreferredMerchant = false;
    bool attemptedAgents[kMaxMerchantContextCandidates]{};
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
                        AdvancedItemActions::ItemFilterFn should_sell,
                        WaitFn wait_ms, const SellAtMerchantOptions& options) {
    const uint32_t npcId = MoveToNearestNpc(anchorX, anchorY, move_to_point, options.npc);
    if (npcId == 0u) {
        return 0;
    }
    if (!OpenMerchantContextWithLegacyPacket(npcId, wait_ms, options.merchant)) {
        return 0;
    }
    return AdvancedItemActions::SellItems(should_sell, wait_ms, options.sell);
}

int DepositItemsAtStorage(float anchorX, float anchorY, MoveToPointFn move_to_point,
                          AdvancedItemActions::ItemFilterFn should_store,
                          WaitFn wait_ms, const DepositAtStorageOptions& options) {
    const uint32_t npcId = MoveToNearestNpc(anchorX, anchorY, move_to_point, options.npc);
    if (npcId == 0u) {
        return 0;
    }

    AgentMgr::InteractNPC(npcId);
    CallWait(wait_ms, options.npc.interact_delay_ms);
    return AdvancedItemActions::DepositItemsToStorage(should_store, wait_ms, options.deposit);
}

MaintenanceMgr::Config BuildMaintenanceConfig(uint32_t outpost_map_id,
                                              const MaintenanceLocation& location) {
    MaintenanceMgr::Config config = {};
    config.maintenanceTown = outpost_map_id;
    if (outpost_map_id != 0u && outpost_map_id == location.outpost_map_id) {
        config.xunlaiChestX = location.xunlai_chest_x;
        config.xunlaiChestY = location.xunlai_chest_y;
        config.materialTraderX = location.material_trader_x;
        config.materialTraderY = location.material_trader_y;
        config.materialTraderPlayerNumber = location.material_trader_player_number;
    }
    return config;
}

bool OpenMaintenanceMerchantContext(const MaintenanceLocation& location,
                                    MoveToPointResultFn move_to_point,
                                    WaitFn wait_ms,
                                    const char* log_prefix) {
    MerchantContextNearCoordsOptions options;
    options.preferred_player_number = location.merchant_player_number;
    options.require_standard_merchant_stock = true;
    options.log_prefix = log_prefix;
    options.merchant.merchant_root_hash = 0u;
    return OpenMerchantContextNearCoords(
        location.merchant_x,
        location.merchant_y,
        location.merchant_search_radius,
        move_to_point,
        wait_ms,
        options);
}

MaintenanceStateResult RunMerchantMaintenanceState(uint32_t configured_outpost_map_id,
                                                   const MaintenanceLocation& location,
                                                   const MaintenanceStateOptions& options) {
    const char* prefix = Prefix(options.log_prefix);
    Log::Info("%s: State: Merchant (return to outpost)", prefix);
    Log::Info("%s: Merchant lane: shared maintenance path", prefix);

    const uint32_t outpost_map_id = ResolveOutpostMapId(configured_outpost_map_id, location, options);
    if (!EnsureMaintenanceOutpost(outpost_map_id, options, "Merchant")) {
        return MapMgr::GetMapId() == outpost_map_id ? MaintenanceStateResult::Retry : MaintenanceStateResult::Stop;
    }
    ClaimConfiguredUnclaimedItems(options);

    const MaintenanceMgr::Config maintenance_cfg = BuildMaintenanceConfig(outpost_map_id, location);
    PerformMaintenanceAtMerchant(
        location,
        options,
        maintenance_cfg,
        "Merchant maintenance: merchant window failed to open, skipping sell/buy",
        true);

    if (!AdvancedRuntime::WaitForTownRuntimeReady(outpost_map_id, options.town_runtime_timeout_ms)) {
        Log::Warn("%s: Merchant: town runtime not stable after maintenance; retrying next tick", prefix);
        return MaintenanceStateResult::Retry;
    }

    if (MaintenanceMgr::NeedsMaintenance(maintenance_cfg)) {
        return MaintenanceStateResult::NeedsMaintenance;
    }
    return MaintenanceStateResult::Done;
}

MaintenanceStateResult RunFullMaintenanceState(uint32_t configured_outpost_map_id,
                                               uint32_t* wipe_count,
                                               const MaintenanceLocation& location,
                                               const MaintenanceStateOptions& options) {
    const char* prefix = Prefix(options.log_prefix);
    Log::Info("%s: State: Maintenance", prefix);

    const uint32_t outpost_map_id = ResolveOutpostMapId(configured_outpost_map_id, location, options);
    if (!EnsureMaintenanceOutpost(outpost_map_id, options, "Maintenance")) {
        return MapMgr::GetMapId() == outpost_map_id ? MaintenanceStateResult::Retry : MaintenanceStateResult::Stop;
    }
    ClaimConfiguredUnclaimedItems(options);

    const auto dp_result =
        AdvancedItemActions::UseDpRemovalSweetIfNeeded(wipe_count, options.wait_ms, options.dp_removal);
    if (dp_result.used_model_id != 0u) {
        Log::Info("%s: Using DP removal sweet (model=%u) after %u wipes",
                  prefix,
                  dp_result.used_model_id,
                  dp_result.previous_wipe_count);
    }

    LogMaintenanceInventorySnapshot(prefix);

    const MaintenanceMgr::Config maintenance_cfg = BuildMaintenanceConfig(outpost_map_id, location);
    const bool primary_merchant_opened = PerformMaintenanceAtMerchant(
        location,
        options,
        maintenance_cfg,
        "Maintenance: merchant window failed to open, skipping shared maintenance",
        false);
    if (!primary_merchant_opened) {
        Log::Warn("%s: Maintenance: running storage/trader fallback without an open standard merchant",
                  prefix);
        MaintenanceMgr::PerformMaintenance(maintenance_cfg);
        CallWait(options.wait_ms, options.post_maintenance_wait_ms);
    }

    if (!AdvancedRuntime::WaitForTownRuntimeReady(outpost_map_id, options.town_runtime_timeout_ms)) {
        Log::Warn("%s: Maintenance: town runtime not stable after merchant/conset maintenance; retrying next tick",
                  prefix);
        return MaintenanceStateResult::Retry;
    }

    uint32_t free_slots = AdvancedInventory::CountFreeSlots();
    uint32_t emergency_sold = 0u;
    const uint32_t cleanup_target =
        maintenance_cfg.minFreeSlots > options.critical_free_slots
            ? maintenance_cfg.minFreeSlots
            : options.critical_free_slots;
    if (free_slots < cleanup_target) {
        if (OpenMaintenanceMerchantContext(location, options.move_to_point, options.wait_ms, prefix)) {
            emergency_sold =
                MaintenanceMgr::SellEmergencyItemsForFreeSlots(cleanup_target);
            free_slots = AdvancedInventory::CountFreeSlots();
            Log::Warn("%s: Maintenance emergency merchant cleanup sold %u item(s); freeSlots=%u/%u",
                      prefix,
                      emergency_sold,
                      free_slots,
                      cleanup_target);
        } else {
            Log::Warn("%s: Maintenance emergency merchant cleanup could not open merchant", prefix);
        }
    }

    bool still_needs_maintenance = MaintenanceMgr::NeedsMaintenance(maintenance_cfg);
    if (still_needs_maintenance && free_slots >= options.critical_free_slots) {
        Log::Info("%s: Maintenance: running one follow-up merchant pass after inventory changed", prefix);
        PerformMaintenanceAtMerchant(
            location,
            options,
            maintenance_cfg,
            "Maintenance: follow-up merchant window failed to open",
            false);
        if (!AdvancedRuntime::WaitForTownRuntimeReady(outpost_map_id, options.town_runtime_timeout_ms)) {
            Log::Warn("%s: Maintenance: town runtime not stable after follow-up maintenance; retrying next tick",
                      prefix);
            return MaintenanceStateResult::Retry;
        }
        free_slots = AdvancedInventory::CountFreeSlots();
        still_needs_maintenance = MaintenanceMgr::NeedsMaintenance(maintenance_cfg);
    }
    if (free_slots < options.critical_free_slots) {
        Log::Warn("%s: Critically low inventory space (%u slots). Consider manual cleanup.",
                  prefix,
                  free_slots);
    }
    if (free_slots < options.critical_free_slots) {
        Log::Warn("%s: Maintenance: inventory not cleared enough to continue (freeSlots=%u preferred=%u critical=%u needsMaintenance=%s); stopping bot",
                  prefix,
                  free_slots,
                  maintenance_cfg.minFreeSlots,
                  options.critical_free_slots,
                  still_needs_maintenance ? "yes" : "no");
        return MaintenanceStateResult::Stop;
    }
    if (MaintenanceMgr::NeedsCharacterConsetRestock(maintenance_cfg)) {
        Log::Warn("%s: Maintenance: consets still missing after maintenance; refusing explorable entry",
                  prefix);
        return MaintenanceStateResult::NeedsMaintenance;
    }
    if (still_needs_maintenance) {
        Log::Warn("%s: Maintenance: continuing after exhausted cleanup with pending preferred upkeep (freeSlots=%u preferred=%u emergencySold=%u)",
                  prefix,
                  free_slots,
                  maintenance_cfg.minFreeSlots,
                  emergency_sold);
    }
    if (free_slots < maintenance_cfg.minFreeSlots) {
        Log::Info("%s: Maintenance: continuing after exhausted maintenance with %u free slots (preferred=%u)",
                  prefix,
                  free_slots,
                  maintenance_cfg.minFreeSlots);
    }

    return MaintenanceStateResult::Done;
}

} // namespace GWA3::MerchantMgr
