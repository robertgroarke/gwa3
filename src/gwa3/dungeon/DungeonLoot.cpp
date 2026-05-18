#include <gwa3/dungeon/DungeonLoot.h>

#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonRunStats.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>

namespace GWA3::DungeonLoot {

namespace {

void CallWait(WaitFn wait_fn, uint32_t ms) {
    if (wait_fn) {
        wait_fn(ms);
        return;
    }
    Sleep(ms);
}

bool IsDead(BoolFn is_dead_fn) {
    return is_dead_fn ? is_dead_fn() : false;
}

const char* LogPrefix(const ChestAtOpenOptions& options) {
    return options.log_prefix ? options.log_prefix : "DungeonLoot";
}

float MaxFloat(float a, float b) {
    return a > b ? a : b;
}

void LogSignpostScan(const ChestAtOpenOptions& options,
                     float x,
                     float y,
                     float maxDist,
                     const char* label,
                     bool chestOnly) {
    if (options.signpost_scan_log) {
        options.signpost_scan_log(x, y, maxDist, label, chestOnly);
    }
}

bool IsBossKeyCandidate(const Item* item,
                        BossKeyItemPredicateFn predicate,
                        const BossKeyModelSet& modelSet) {
    if (!item) return false;
    if (predicate) return predicate(item);
    return IsBossKeyLikeItem(item, modelSet);
}

bool EnsureFreeSlotForBossKeyPickup(WaitFn wait_ms, const char* prefix) {
    if (DungeonInventory::CountFreeSlots() > 0u) {
        return true;
    }

    DungeonInventory::EmergencyFreeSlotOptions options;
    options.log_prefix = prefix;
    options.wait_ms = wait_ms;
    options.allow_green_items = true;
    const auto result = DungeonInventory::DropEmergencyInventoryItemForFreeSlot(options);
    if (!result.dropped) {
        Log::Warn("%s: Boss key pickup blocked by full inventory and no emergency drop candidate",
                  prefix ? prefix : "DungeonLoot");
        return false;
    }

    const uint32_t freeSlots = DungeonInventory::CountFreeSlots();
    Log::Info("%s: Boss key emergency slot result dropped=%u model=%u freeSlots=%u",
              prefix ? prefix : "DungeonLoot",
              result.item_id,
              result.model_id,
              freeSlots);
    return freeSlots > 0u;
}

} // namespace

ChestAtOpenOptions MakeChestAtOpenOptions(
    const char* log_prefix,
    bool use_bundle_fallback,
    const ChestBundleFallbackOptions& bundle_fallback,
    SignpostScanLogFn signpost_scan_log,
    BoolFn is_world_ready) {
    ChestAtOpenOptions options;
    options.log_prefix = log_prefix;
    options.use_bundle_fallback = use_bundle_fallback;
    options.bundle_fallback = bundle_fallback;
    options.bundle_fallback.log_prefix = log_prefix;
    options.signpost_scan_log = signpost_scan_log;
    options.nearby.loot = MakeLootPickupOptions(log_prefix, is_world_ready);
    options.resolved.log_prefix = log_prefix;
    options.resolved.loot = MakeLootPickupOptions(log_prefix, is_world_ready);
    return options;
}

bool IsModelInBossKeySet(uint32_t modelId, const BossKeyModelSet& modelSet) {
    if (!modelSet.model_ids || modelSet.model_count <= 0) return false;
    for (int i = 0; i < modelSet.model_count; ++i) {
        if (modelSet.model_ids[i] == modelId) return true;
    }
    return false;
}

bool IsBossKeyLikeItem(const Item* item) {
    if (!item) return false;
    return item->type == TYPE_KEY;
}

bool IsBossKeyLikeItem(const Item* item, const BossKeyModelSet& modelSet) {
    if (!item) return false;
    if (IsModelInBossKeySet(item->model_id, modelSet)) return true;
    return modelSet.accept_type_key && item->type == TYPE_KEY;
}

uint32_t CountNearbyBossKeyCandidates(float x,
                                      float y,
                                      float maxRange,
                                      BossKeyItemPredicateFn is_boss_key,
                                      const BossKeyModelSet& boss_key_models) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t myId = me ? me->agent_id : 0u;
    uint32_t count = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) continue;
        auto* itemAgent = static_cast<const AgentItem*>(agent);
        if (itemAgent->owner != 0u && itemAgent->owner != myId) continue;
        if (AgentMgr::GetDistance(x, y, agent->x, agent->y) > maxRange) continue;
        auto* item = ItemMgr::GetItemById(itemAgent->item_id);
        if (!IsBossKeyCandidate(item, is_boss_key, boss_key_models)) continue;
        ++count;
    }
    return count;
}

void LogNearbyBossKeyCandidates(const char* label,
                                float x,
                                float y,
                                float maxRange,
                                const char* log_prefix,
                                const LootPickupOptions& options,
                                BossKeyItemPredicateFn is_boss_key,
                                const BossKeyModelSet& boss_key_models) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t myId = me ? me->agent_id : 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    const char* prefix = log_prefix ? log_prefix : "DungeonLoot";
    uint32_t matches = 0u;
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) continue;
        auto* itemAgent = static_cast<const AgentItem*>(agent);
        if (itemAgent->owner != 0u && itemAgent->owner != myId) continue;
        const float dist = AgentMgr::GetDistance(x, y, agent->x, agent->y);
        if (dist > maxRange) continue;
        auto* item = ItemMgr::GetItemById(itemAgent->item_id);
        if (!IsBossKeyCandidate(item, is_boss_key, boss_key_models)) continue;
        ++matches;
        Log::Info("%s: %s candidate[%u] agent=%u itemId=%u model=%u type=%u owner=%u pos=(%.0f, %.0f) dist=%.0f shouldPick=%d",
                  prefix,
                  label ? label : "BossKey",
                  matches,
                  agent->agent_id,
                  itemAgent->item_id,
                  item->model_id,
                  item->type,
                  itemAgent->owner,
                  agent->x,
                  agent->y,
                  dist,
                  ShouldPickUpItemAgent(agent, myId, DungeonInventory::CountFreeSlots(), options) ? 1 : 0);
    }
    Log::Info("%s: %s totalCandidates=%u center=(%.0f, %.0f) radius=%.0f",
              prefix,
              label ? label : "BossKey",
              matches,
              x,
              y,
              maxRange);
}

bool ForcePickUpBossKeyCandidates(float centerX,
                                  float centerY,
                                  float scanRange,
                                  MoveToPointFn move_to_point,
                                  WaitFn wait_ms,
                                  BoolFn is_dead,
                                  const BossKeyPickupOptions& options) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return false;
    const uint32_t myId = me->agent_id;
    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonLoot";
    bool pickedAny = false;

    for (uint32_t pass = 1u; pass <= options.passes; ++pass) {
        bool pickedThisPass = false;
        const uint32_t maxAgents = AgentMgr::GetMaxAgents();
        for (uint32_t i = 1u; i < maxAgents; ++i) {
            auto* agent = AgentMgr::GetAgentByID(i);
            if (!agent || agent->type != 0x400u) continue;
            auto* itemAgent = static_cast<const AgentItem*>(agent);
            if (itemAgent->owner != 0u && itemAgent->owner != myId) continue;
            const float distFromCenter = AgentMgr::GetDistance(centerX, centerY, agent->x, agent->y);
            if (distFromCenter > scanRange) continue;
            auto* item = ItemMgr::GetItemById(itemAgent->item_id);
            if (!IsBossKeyCandidate(item, options.is_boss_key, options.boss_key_models)) continue;

            if (!EnsureFreeSlotForBossKeyPickup(wait_ms, prefix)) {
                return pickedAny;
            }

            me = AgentMgr::GetMyAgent();
            if (!me) return pickedAny;
            const float distFromPlayer = AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y);
            Log::Info("%s: ForcePickUpBossKey pass=%u agent=%u model=%u type=%u distFromCenter=%.0f distFromPlayer=%.0f",
                      prefix,
                      pass,
                      agent->agent_id,
                      item->model_id,
                      item->type,
                      distFromCenter,
                      distFromPlayer);

            if (distFromPlayer > options.approach_threshold) {
                if (move_to_point) {
                    move_to_point(agent->x, agent->y, options.approach_threshold);
                } else {
                    AgentMgr::Move(agent->x, agent->y);
                }
                CallWait(wait_ms, options.settle_delay_ms);
                me = AgentMgr::GetMyAgent();
                if (!me) return pickedAny;
                const float settleDist = AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y);
                Log::Info("%s: ForcePickUpBossKey tightened approach agent=%u settleDist=%.0f",
                          prefix,
                          agent->agent_id,
                          settleDist);
            }

            const uint32_t itemAgentId = agent->agent_id;
            const DWORD start = GetTickCount();
            uint32_t retries = 0u;
            while (retries < options.pickup_retry_limit &&
                   (GetTickCount() - start) < options.pickup_timeout_ms) {
                ItemMgr::PickUpItem(itemAgentId);
                CallWait(wait_ms, options.pickup_delay_ms);
                ++retries;
                if (!AgentMgr::GetAgentExists(itemAgentId)) {
                    pickedAny = true;
                    pickedThisPass = true;
                    Log::Info("%s: ForcePickUpBossKey success agent=%u retries=%u",
                              prefix,
                              itemAgentId,
                              retries);
                    break;
                }
                if (IsDead(is_dead)) return pickedAny;
            }
        }

        const uint32_t remaining = CountNearbyBossKeyCandidates(
            centerX,
            centerY,
            scanRange,
            options.is_boss_key,
            options.boss_key_models);
        Log::Info("%s: ForcePickUpBossKey pass=%u remaining=%u pickedAny=%d",
                  prefix,
                  pass,
                  remaining,
                  pickedAny ? 1 : 0);
        if (remaining == 0u || !pickedThisPass) {
            break;
        }
    }

    return pickedAny;
}

bool AcquireBossKey(const BossKeyAcquireOptions& options) {
    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonLoot";
    if (options.key_scan_range <= 0.0f) {
        Log::Warn("%s: AcquireBossKey requires caller-provided key coordinates and scan range", prefix);
        return false;
    }

    Log::Info("%s: AcquireBossKey start", prefix);
    AgentMgr::CancelAction();
    CallWait(options.wait_ms, options.pre_scan_wait_ms);
    AgentMgr::ChangeTarget(0);
    CallWait(options.wait_ms, options.clear_target_wait_ms);
    LogNearbyBossKeyCandidates(
        "AcquireBossKey before-passes",
        options.key_x,
        options.key_y,
        options.key_scan_range,
        prefix,
        options.loot,
        options.is_boss_key,
        options.boss_key_models);

    BossKeyPickupOptions forceOptions = options.force_pickup;
    if (!forceOptions.log_prefix) {
        forceOptions.log_prefix = prefix;
    }
    if (!forceOptions.is_boss_key) {
        forceOptions.is_boss_key = options.is_boss_key;
    }
    if (!forceOptions.boss_key_models.model_ids) {
        forceOptions.boss_key_models = options.boss_key_models;
    }

    for (uint32_t pass = 1u; pass <= options.passes; ++pass) {
        const float lootRange = pass < options.passes ? options.wide_loot_range : options.final_loot_range;
        if (options.combat_move_to) {
            options.combat_move_to(options.key_x, options.key_y, options.move_fight_range);
        } else {
            AgentMgr::Move(options.key_x, options.key_y);
        }

        const int picked = options.pickup_nearby_loot
            ? options.pickup_nearby_loot(lootRange)
            : PickUpNearbyLoot(lootRange, options.wait_ms, options.is_dead, options.loot);
        const bool forced = ForcePickUpBossKeyCandidates(
            options.key_x,
            options.key_y,
            options.key_scan_range,
            options.move_to_point,
            options.wait_ms,
            options.is_dead,
            forceOptions);
        auto* me = AgentMgr::GetMyAgent();
        const float meX = me ? me->x : options.key_x;
        const float meY = me ? me->y : options.key_y;
        const uint32_t nearbyKeys = CountNearbyBossKeyCandidates(
            options.key_x,
            options.key_y,
            options.key_scan_range,
            options.is_boss_key,
            options.boss_key_models);
        const uint32_t freeSlots = DungeonInventory::CountFreeSlots();
        Log::Info("%s: AcquireBossKey pass=%u picked=%d forced=%d nearbyKeys=%u player=(%.0f, %.0f) distToKey=%.0f",
                  prefix,
                  pass,
                  picked,
                  forced ? 1 : 0,
                  nearbyKeys,
                  meX,
                  meY,
                  AgentMgr::GetDistance(meX, meY, options.key_x, options.key_y));
        Log::Info("%s: AcquireBossKey pass=%u freeSlots=%u", prefix, pass, freeSlots);
        LogNearbyBossKeyCandidates(
            "AcquireBossKey after-pass",
            options.key_x,
            options.key_y,
            options.key_scan_range,
            prefix,
            options.loot,
            options.is_boss_key,
            options.boss_key_models);
        if (nearbyKeys == 0u) {
            return true;
        }
        AgentMgr::CancelAction();
        CallWait(options.wait_ms, options.retry_wait_ms);
    }

    auto* me = AgentMgr::GetMyAgent();
    const float meX = me ? me->x : options.key_x;
    const float meY = me ? me->y : options.key_y;
    const uint32_t nearbyKeys = CountNearbyBossKeyCandidates(
        options.key_x,
        options.key_y,
        options.key_scan_range,
        options.is_boss_key,
        options.boss_key_models);
    LogNearbyBossKeyCandidates(
        "AcquireBossKey final",
        options.key_x,
        options.key_y,
        options.key_scan_range,
        prefix,
        options.loot,
        options.is_boss_key,
        options.boss_key_models);
    Log::Warn("%s: AcquireBossKey incomplete nearbyKeys=%u player=(%.0f, %.0f)",
              prefix,
              nearbyKeys,
              meX,
              meY);
    const uint32_t finalFreeSlots = DungeonInventory::CountFreeSlots();
    if (nearbyKeys > 0u && finalFreeSlots == 0u) {
        Log::Warn("%s: AcquireBossKey failed because inventory is full; refusing door-open validation", prefix);
        return false;
    }
    return nearbyKeys == 0u;
}

bool OpenNearbyChest(float maxRange, DungeonInteractions::OpenedChestTracker& tracker,
                     MoveToPointFn move_to_point, WaitFn wait_ms, BoolFn is_dead,
                     const ChestOpenOptions& options) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return false;

    tracker.ResetForMap(MapMgr::GetMapId());
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x200u) continue;
        const float dist = AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y);
        if (dist > maxRange) continue;

        auto* gadget = static_cast<const AgentGadget*>(agent);
        if (!DungeonInteractions::IsChestGadgetId(gadget->gadget_id)) continue;
        if (tracker.IsOpened(agent->agent_id)) continue;

        tracker.MarkOpened(agent->agent_id);
        if (dist > options.move_threshold && move_to_point) {
            move_to_point(agent->x, agent->y, options.move_threshold);
            if (IsDead(is_dead)) return false;
        }

        AgentMgr::InteractSignpost(agent->agent_id);
        CallWait(wait_ms, options.interact_delay_ms);
        (void)PickUpNearbyLoot(options.pickup_range, wait_ms, is_dead, options.loot);
        DungeonRunStats::RecordChestOpenedByAgent(agent->agent_id);
        return true;
    }

    return false;
}

bool OpenResolvedChestAndPickUpLoot(uint32_t chestId,
                                    float chestX,
                                    float chestY,
                                    float searchRadius,
                                    DungeonInteractions::OpenedChestTracker& tracker,
                                    MoveToPointFn move_to_point,
                                    WaitFn wait_ms,
                                    BoolFn is_dead,
                                    const ResolvedChestOpenOptions& options) {
    if (chestId == 0u) {
        return false;
    }

    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonLoot";
    tracker.ResetForMap(MapMgr::GetMapId());
    if (tracker.IsOpened(chestId)) {
        Log::Info("%s: OpenChestAt chest signpost %u already marked opened", prefix, chestId);
        return true;
    }

    auto* chest = AgentMgr::GetAgentByID(chestId);
    if (chest) {
        if (move_to_point) {
            move_to_point(chest->x, chest->y, options.chest_move_threshold);
        }
    } else if (move_to_point) {
        move_to_point(chestX, chestY, options.fallback_move_threshold);
    }

    int picked = 0;
    for (int attempt = 1; attempt <= options.attempts; ++attempt) {
        if (DungeonInventory::CountFreeSlots() == 0u) {
            DungeonInventory::EmergencyFreeSlotOptions freeSlotOptions;
            freeSlotOptions.log_prefix = prefix;
            freeSlotOptions.wait_ms = wait_ms;
            (void)DungeonInventory::EnsureEmergencyFreeSlots(1u, freeSlotOptions);
        }
        Log::Info("%s: OpenChestAt attempt %d signpost=%u near (%.0f, %.0f)",
                  prefix,
                  attempt,
                  chestId,
                  chestX,
                  chestY);
        AgentMgr::InteractSignpost(chestId);
        CallWait(wait_ms, options.interact_delay_ms);
        if (DungeonInventory::CountFreeSlots() == 0u) {
            DungeonInventory::EmergencyFreeSlotOptions freeSlotOptions;
            freeSlotOptions.log_prefix = prefix;
            freeSlotOptions.wait_ms = wait_ms;
            (void)DungeonInventory::EnsureEmergencyFreeSlots(1u, freeSlotOptions);
        }
        picked += PickUpNearbyLoot(options.pickup_range, wait_ms, is_dead, options.loot);
        if (chest && move_to_point) {
            move_to_point(chest->x, chest->y, options.chest_move_threshold);
        }
    }

    Log::Info("%s: OpenChestAt result signpost=%u picked=%d", prefix, chestId, picked);
    const bool chestStillPresent = DungeonInteractions::IsChestStillPresentNear(chestX, chestY, searchRadius);
    if (picked > 0 || !chestStillPresent) {
        tracker.MarkOpened(chestId);
        DungeonRunStats::RecordChestOpenedByAgent(chestId);
        return true;
    }

    Log::Warn("%s: OpenChestAt interaction inconclusive signpost=%u picked=%d chestStillPresent=%d",
              prefix,
              chestId,
              picked,
              chestStillPresent ? 1 : 0);
    return false;
}

static uint32_t ResolveChestAtTargetCoords(float chestX,
                                           float chestY,
                                           float searchRadius,
                                           const ChestAtOpenOptions& options) {
    uint32_t chestId = DungeonInteractions::FindNearestChestSignpost(chestX, chestY, searchRadius);
    if (chestId != 0u) return chestId;

    return DungeonInteractions::ResolveGenericChestFallback(
        DungeonInteractions::FindNearestSignpost(chestX, chestY, searchRadius),
        chestX,
        chestY,
        searchRadius,
        "target-coords",
        LogPrefix(options));
}

static uint32_t ResolveChestFromLivePlayerPosition(float chestX,
                                                   float chestY,
                                                   float searchRadius,
                                                   float playerX,
                                                   float playerY,
                                                   float playerSearchRadius,
                                                   const ChestAtOpenOptions& options) {
    const char* prefix = LogPrefix(options);
    Log::Info("%s: OpenChestAt retrying from live player position player=(%.0f, %.0f) radius=%.0f",
              prefix, playerX, playerY, playerSearchRadius);
    LogSignpostScan(options, playerX, playerY, playerSearchRadius, "OpenChestAt player-all-signpost scan", false);
    LogSignpostScan(options, playerX, playerY, playerSearchRadius, "OpenChestAt player-chest-only scan", true);

    uint32_t chestId = DungeonInteractions::FindNearestChestSignpost(playerX, playerY, playerSearchRadius);
    if (chestId != 0u) return chestId;

    return DungeonInteractions::ResolveGenericChestFallback(
        DungeonInteractions::FindNearestSignpost(playerX, playerY, playerSearchRadius),
        chestX,
        chestY,
        searchRadius,
        "live-player",
        prefix);
}

bool OpenChestAt(float chestX,
                 float chestY,
                 float searchRadius,
                 DungeonInteractions::OpenedChestTracker& tracker,
                 MoveToPointFn move_to_point,
                 WaitFn wait_ms,
                 BoolFn is_dead,
                 const ChestAtOpenOptions& options) {
    auto* me = AgentMgr::GetMyAgent();
    const float playerX = me ? me->x : 0.0f;
    const float playerY = me ? me->y : 0.0f;
    const float playerDist = me ? AgentMgr::GetDistance(playerX, playerY, chestX, chestY) : -1.0f;
    const char* prefix = LogPrefix(options);
    Log::Info("%s: OpenChestAt start target=(%.0f, %.0f) player=(%.0f, %.0f) dist=%.0f radius=%.0f",
              prefix, chestX, chestY, playerX, playerY, playerDist, searchRadius);

    if (options.bundle_open && options.bundle_open(chestX, chestY, searchRadius)) {
        DungeonRunStats::RecordChestOpened();
        return true;
    }
    if (options.use_bundle_fallback &&
        OpenChestWithBundleFallback(chestX, chestY, searchRadius, wait_ms, options.bundle_fallback)) {
        return true;
    }

    uint32_t chestId = ResolveChestAtTargetCoords(chestX, chestY, searchRadius, options);
    if (chestId == 0u && me) {
        const float playerSearchRadius =
            MaxFloat(searchRadius * options.search_radius_multiplier, options.live_player_search_radius_min);
        chestId = ResolveChestFromLivePlayerPosition(
            chestX,
            chestY,
            searchRadius,
            playerX,
            playerY,
            playerSearchRadius,
            options);
        if (chestId == 0u &&
            OpenNearbyChest(playerSearchRadius, tracker, move_to_point, wait_ms, is_dead, options.nearby)) {
            Log::Info("%s: OpenChestAt live player fallback OpenNearbyChest succeeded", prefix);
            return true;
        }
    }

    if (chestId == 0u) {
        LogSignpostScan(options, chestX, chestY, searchRadius, "OpenChestAt all-signpost scan", false);
        LogSignpostScan(options, chestX, chestY, searchRadius, "OpenChestAt chest-only scan", true);
        const float nearbyRange =
            MaxFloat(options.fallback_search_radius_min, searchRadius * options.search_radius_multiplier);
        if (OpenNearbyChest(nearbyRange, tracker, move_to_point, wait_ms, is_dead, options.nearby)) {
            Log::Info("%s: OpenChestAt fallback OpenNearbyChest succeeded near target=(%.0f, %.0f)",
                      prefix,
                      chestX,
                      chestY);
            return true;
        }
        Log::Warn("%s: OpenChestAt found no signpost near (%.0f, %.0f) radius=%.0f",
                  prefix,
                  chestX,
                  chestY,
                  searchRadius);
        return false;
    }

    return OpenResolvedChestAndPickUpLoot(
        chestId,
        chestX,
        chestY,
        searchRadius,
        tracker,
        move_to_point,
        wait_ms,
        is_dead,
        options.resolved);
}

bool OpenChestWithBundleFallback(float chestX,
                                 float chestY,
                                 float searchRadius,
                                 WaitFn wait_ms,
                                 const ChestBundleFallbackOptions& options) {
    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonLoot";
    const float sharedSignpostRadius = MaxFloat(options.min_signpost_radius, searchRadius);
    const float sharedLootRadius = MaxFloat(
        options.min_loot_radius,
        searchRadius * options.loot_radius_multiplier);
    if (!DungeonBundle::OpenChestAndPickUpBundle(
            chestX,
            chestY,
            sharedSignpostRadius,
            sharedLootRadius,
            options.open_attempts,
            options.pickup_attempts,
            options.open_retry_delay_ms,
            options.pickup_retry_delay_ms)) {
        return false;
    }

    CallWait(wait_ms, options.verify_delay_ms);
    const bool chestStillPresent = DungeonInteractions::IsChestStillPresentNear(chestX, chestY, searchRadius);
    Log::Info("%s: OpenChestWithBundleFallback succeeded target=(%.0f, %.0f) signpostRadius=%.0f lootRadius=%.0f chestStillPresent=%d",
              prefix,
              chestX,
              chestY,
              sharedSignpostRadius,
              sharedLootRadius,
              chestStillPresent ? 1 : 0);
    if (chestStillPresent) {
        Log::Info("%s: OpenChestWithBundleFallback keeping success despite lingering chest signpost", prefix);
    }
    return true;
}

BossChestLootResult OpenBossChestAndLoot(
    float chestX,
    float chestY,
    float searchRadius,
    float lootRadius,
    MoveToPointResultFn move_to_point,
    OpenChestAtFn open_chest_at,
    PickupNearbyLootFn pickup_nearby_loot,
    WaitFn wait_ms,
    const BossChestLootOptions& options) {
    BossChestLootResult result = {};
    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "DungeonLoot";
    if (move_to_point == nullptr || open_chest_at == nullptr || pickup_nearby_loot == nullptr) {
        Log::Warn("%s: Boss chest loot missing required callbacks", prefix);
        return result;
    }

    result.staged = move_to_point(chestX, chestY, options.stage_move_threshold);
    if (!result.staged) {
        Log::Warn("%s: Boss chest staging failed target=(%.0f, %.0f)", prefix, chestX, chestY);
        return result;
    }

    const int attempts = options.open_attempts > 0 ? options.open_attempts : 1;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        ++result.open_attempts;
        if (open_chest_at(chestX, chestY, searchRadius)) {
            ++result.open_successes;
        }

        const uint32_t delayMs = attempt == 0
            ? options.first_loot_delay_ms
            : options.retry_loot_delay_ms;
        if (wait_ms != nullptr && delayMs > 0u) {
            wait_ms(delayMs);
        }
        result.picked_loot_count += pickup_nearby_loot(lootRadius);
        if (result.open_successes > 0u) {
            Log::Info("%s: Boss chest open succeeded; skipping duplicate open attempts", prefix);
            break;
        }
    }

    result.completed = true;
    Log::Info("%s: Boss chest loot completed target=(%.0f, %.0f) attempts=%u successes=%u picked=%d",
              prefix,
              chestX,
              chestY,
              result.open_attempts,
              result.open_successes,
              result.picked_loot_count);
    return result;
}

} // namespace GWA3::DungeonLoot
