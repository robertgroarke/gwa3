#include <gwa3/dungeon/DungeonLoot.h>

#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
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

bool IsWorldReady(const LootPickupOptions& options) {
    return options.is_world_ready ? options.is_world_ready() : true;
}

const char* LogPrefix(const LootPickupOptions& options) {
    return options.log_prefix ? options.log_prefix : "DungeonLoot";
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

} // namespace

bool IsAlwaysPickupModel(uint32_t modelId) {
    switch (modelId) {
    case 2619u: case 36985u:
    case 27067u: case 27071u: case 27033u: case 27052u: case 22374u:
    case 2605u: case 2606u: case 501u: case 502u: case 503u: case 2566u:
    case 2607u: case 6102u: case 6104u: case 6531u:
    case 15564u: case 15565u: case 15867u: case 15869u: case 15870u: case 15871u:
    case 17054u: case 17055u: case 17075u:
    case 22781u: case 22782u:
    case 24628u: case 24582u:
    case 910u: case 2513u: case 5585u: case 6049u: case 6366u: case 6367u: case 6375u:
    case 15477u: case 19171u: case 19172u: case 19173u: case 22190u: case 24593u:
    case 28435u: case 30855u: case 31145u: case 31146u: case 35124u: case 36682u:
    case 15528u: case 15479u: case 19170u: case 21492u: case 21812u: case 22644u:
    case 30208u: case 31150u: case 35125u: case 36681u:
    case 17060u: case 17061u: case 17062u: case 22269u: case 28431u: case 28432u:
    case 28436u: case 29431u: case 31151u: case 31152u: case 31153u: case 35121u:
    case 6370u: case 19039u: case 21488u: case 21489u: case 22191u: case 26784u:
    case 28433u: case 35127u:
    case 556u: case 18345u: case 21491u: case 37765u: case 21833u: case 28434u:
    case 930u: case 935u: case 936u: case 945u:
    case 21786u: case 21787u: case 21788u: case 21789u: case 21790u:
    case 21791u: case 21792u: case 21793u: case 21794u: case 21795u:
    case 21796u: case 21797u: case 21798u: case 21799u: case 21800u:
    case 21801u: case 21802u: case 21803u: case 21804u: case 21805u:
    case 22751u:
        return true;
    default:
        return false;
    }
}

bool IsQuestPickupModel(uint32_t modelId) {
    switch (modelId) {
    case 22342u: case 24350u:
    case 22751u:
    case 21796u: case 21797u: case 21798u: case 21799u: case 21800u:
    case 21801u: case 21802u: case 21803u: case 21804u: case 21805u:
    case 28435u: case 28436u: case 28431u: case 22269u:
        return true;
    default:
        return false;
    }
}

bool IsBossKeyLikeItem(const Item* item) {
    if (!item) return false;
    return item->type == TYPE_KEY;
}

bool IsWorldReadyForLoot() {
    return MapMgr::GetIsMapLoaded() && AgentMgr::GetMyId() != 0u;
}

bool IsWorldReadyForLootWithPlayerAgent() {
    return IsWorldReadyForLoot() && AgentMgr::GetMyAgent() != nullptr;
}

float ComputePostCombatLootRange(float aggroRange, float minRange, float maxRange) {
    float range = aggroRange * 2.0f;
    if (range < minRange) range = minRange;
    if (range > maxRange) range = maxRange;
    return range;
}

int SweepPostCombatLoot(float aggroRange, const PostCombatLootSweepOptions& options) {
    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonLoot";
    const char* reason = options.reason ? options.reason : "";
    const float lootRange = ComputePostCombatLootRange(aggroRange);
    int totalPicked = 0;
    bool sawCandidates = false;
    int quietPasses = 0;
    for (int pass = 1; pass <= options.max_passes; ++pass) {
        if (options.is_world_ready && !options.is_world_ready()) {
            Log::Warn("%s: Post-combat loot aborted reason=%s pass=%d map=%u loaded=%d myId=%u",
                      prefix,
                      reason,
                      pass,
                      MapMgr::GetMapId(),
                      MapMgr::GetIsMapLoaded() ? 1 : 0,
                      AgentMgr::GetMyId());
            break;
        }

        const uint32_t candidatesBefore = CountNearbyPickupCandidates(lootRange);
        if (candidatesBefore > 0u) {
            sawCandidates = true;
        }

        const int picked = options.pickup_nearby_loot
            ? options.pickup_nearby_loot(lootRange)
            : PickUpNearbyLoot(lootRange);
        totalPicked += picked;

        const uint32_t candidatesAfter = CountNearbyPickupCandidates(lootRange);
        Log::Info("%s: Post-combat loot reason=%s pass=%d range=%.0f picked=%d total=%d",
                  prefix,
                  reason,
                  pass,
                  lootRange,
                  picked,
                  totalPicked);
        if (picked > 0 || candidatesAfter > 0u) {
            sawCandidates = true;
            quietPasses = 0;
            CallWait(options.wait_ms, options.pass_wait_ms);
            continue;
        }

        ++quietPasses;
        if (!sawCandidates) {
            if (pass < options.no_candidate_passes_before_stop) {
                CallWait(options.wait_ms, options.pass_wait_ms);
                continue;
            }
            break;
        }

        if (quietPasses >= options.quiet_passes_after_candidates) {
            break;
        }
        CallWait(options.wait_ms, options.pass_wait_ms);
    }
    return totalPicked;
}

bool ShouldPickUpItemAgent(const Agent* agent, uint32_t myAgentId, uint32_t freeSlots,
                           const LootPickupOptions& options) {
    if (!agent || agent->type != 0x400u) return false;
    auto* itemAgent = static_cast<const AgentItem*>(agent);
    if (itemAgent->owner != 0u && itemAgent->owner != myAgentId) return false;

    auto* item = ItemMgr::GetItemById(itemAgent->item_id);
    if (!item) return false;

    if (IsBossKeyLikeItem(item)) return true;

    if (freeSlots < options.general_loot_min_free_slots) {
        if (item->type == TYPE_GOLD) return true;
        if (item->type == TYPE_BUNDLE) return true;
        return false;
    }

    if (MaintenanceMgr::IsRareSkin(item->model_id)) return true;
    if (IsAlwaysPickupModel(item->model_id)) return true;
    if (IsQuestPickupModel(item->model_id)) return true;

    switch (item->type) {
    case TYPE_BUNDLE:
        return item->model_id == 22342u || item->model_id == 24350u;
    case TYPE_DYE:
        return item->dye.dye_tint == 10u;
    case TYPE_KEY:
        return true;
    case TYPE_GOLD:
        return ItemMgr::GetGoldCharacter() < options.character_gold_cap;
    case TYPE_MATERIAL:
    case TYPE_SCROLL:
    case TYPE_TROPHY:
        return false;
    case TYPE_USABLE:
        return item->model_id >= 21786u && item->model_id <= 21805u;
    default:
        break;
    }

    const uint16_t rarity = DungeonInventory::GetItemRarity(item);
    return rarity == DungeonInventory::RARITY_GOLD && freeSlots >= options.general_loot_min_free_slots;
}

uint32_t CountNearbyPickupCandidates(float maxRange, const LootPickupOptions& options) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;
    const uint32_t myId = me->agent_id;
    const uint32_t freeSlots = DungeonInventory::CountFreeSlots();

    uint32_t count = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) continue;
        if (AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y) > maxRange) continue;
        if (!ShouldPickUpItemAgent(agent, myId, freeSlots, options)) continue;
        ++count;
    }
    return count;
}

int PickUpNearbyLoot(float maxRange, WaitFn wait_ms, BoolFn is_dead, const LootPickupOptions& options) {
    if (!IsWorldReady(options)) {
        Log::Warn("%s: PickUpNearbyLoot skipped world-not-ready map=%u loaded=%d myId=%u range=%.0f",
                  LogPrefix(options),
                  MapMgr::GetMapId(),
                  MapMgr::GetIsMapLoaded() ? 1 : 0,
                  AgentMgr::GetMyId(),
                  maxRange);
        return 0;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    const uint32_t myId = me->agent_id;

    uint32_t freeSlots = DungeonInventory::CountFreeSlots();
    if (freeSlots == 0u) return 0;

    const DWORD globalStart = GetTickCount();
    int picked = 0;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents && freeSlots > 0u; ++i) {
        if (!IsWorldReady(options)) {
            Log::Warn("%s: PickUpNearbyLoot aborting mid-scan world-not-ready map=%u loaded=%d myId=%u picked=%d",
                      LogPrefix(options),
                      MapMgr::GetMapId(),
                      MapMgr::GetIsMapLoaded() ? 1 : 0,
                      AgentMgr::GetMyId(),
                      picked);
            return picked;
        }

        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) continue;
        const float dist = AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y);
        if (dist > maxRange) continue;
        if (!ShouldPickUpItemAgent(agent, myId, freeSlots, options)) continue;

        if (dist > options.interact_threshold) {
            AgentMgr::Move(agent->x, agent->y);
            const DWORD moveStart = GetTickCount();
            while (AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y) > options.interact_threshold &&
                   (GetTickCount() - moveStart) < options.move_timeout_ms) {
                CallWait(wait_ms, options.move_poll_ms);
                me = AgentMgr::GetMyAgent();
                if (!IsWorldReady(options)) {
                    Log::Warn("%s: PickUpNearbyLoot aborting during approach world-not-ready map=%u loaded=%d myId=%u picked=%d",
                              LogPrefix(options),
                              MapMgr::GetMapId(),
                              MapMgr::GetIsMapLoaded() ? 1 : 0,
                              AgentMgr::GetMyId(),
                              picked);
                    return picked;
                }
                if (!me || IsDead(is_dead)) return picked;
            }
        }

        const uint32_t itemAgentId = agent->agent_id;
        const auto* itemAgent = static_cast<const AgentItem*>(agent);
        const uint32_t itemId = itemAgent->item_id;
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const DWORD itemStart = GetTickCount();
        uint32_t retries = 0u;
        while (retries < options.pickup_retry_limit &&
               (GetTickCount() - itemStart) < options.pickup_timeout_ms) {
            if (!IsWorldReady(options)) {
                Log::Warn("%s: PickUpNearbyLoot aborting before item interact world-not-ready map=%u loaded=%d myId=%u agent=%u item=%u retries=%u",
                          LogPrefix(options),
                          MapMgr::GetMapId(),
                          MapMgr::GetIsMapLoaded() ? 1 : 0,
                          AgentMgr::GetMyId(),
                          itemAgentId,
                          itemId,
                          retries);
                return picked;
            }
            ItemMgr::PickUpItem(itemAgentId);
            CallWait(wait_ms, options.pickup_delay_ms);
            ++retries;
            if (!IsWorldReady(options)) {
                Log::Warn("%s: PickUpNearbyLoot aborting after item interact world-not-ready map=%u loaded=%d myId=%u agent=%u item=%u retries=%u",
                          LogPrefix(options),
                          MapMgr::GetMapId(),
                          MapMgr::GetIsMapLoaded() ? 1 : 0,
                          AgentMgr::GetMyId(),
                          itemAgentId,
                          itemId,
                          retries);
                return picked;
            }
            if (!AgentMgr::GetAgentExists(itemAgentId)) break;
            auto* pickedItem = ItemMgr::GetItemById(itemId);
            if (pickedItem && pickedItem->bag != nullptr) break;
            if (ItemMgr::GetGoldCharacter() != goldBefore) break;
            if (IsDead(is_dead)) return picked;
        }

        const bool goldChanged = ItemMgr::GetGoldCharacter() != goldBefore;
        auto* pickedItem = ItemMgr::GetItemById(itemId);
        const bool movedIntoInventory = pickedItem && pickedItem->bag != nullptr;
        const bool pickedUp = !AgentMgr::GetAgentExists(itemAgentId) || goldChanged || movedIntoInventory;
        if (!pickedUp) {
            if (options.log_prefix) {
                Log::Warn("%s: PickUpNearbyLoot failed agent=%u item=%u range=%.0f retries=%u goldBefore=%u goldAfter=%u inventory=%d",
                          LogPrefix(options),
                          itemAgentId,
                          itemId,
                          maxRange,
                          retries,
                          goldBefore,
                          ItemMgr::GetGoldCharacter(),
                          movedIntoInventory ? 1 : 0);
            }
            continue;
        }

        ++picked;
        if (!goldChanged) {
            --freeSlots;
        }
        me = AgentMgr::GetMyAgent();
        if (!me) return picked;
        if ((GetTickCount() - globalStart) > options.global_timeout_ms) {
            if (options.log_prefix) {
                Log::Warn("%s: PickUpNearbyLoot global timeout exceeded picked=%d timeoutMs=%u",
                          LogPrefix(options),
                          picked,
                          options.global_timeout_ms);
            }
            return picked;
        }
    }

    return picked;
}

uint32_t CountNearbyBossKeyCandidates(float x,
                                      float y,
                                      float maxRange,
                                      BossKeyItemPredicateFn is_boss_key) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t myId = me ? me->agent_id : 0u;
    uint32_t count = 0u;
    const BossKeyItemPredicateFn predicate = is_boss_key ? is_boss_key : &IsBossKeyLikeItem;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) continue;
        auto* itemAgent = static_cast<const AgentItem*>(agent);
        if (itemAgent->owner != 0u && itemAgent->owner != myId) continue;
        if (AgentMgr::GetDistance(x, y, agent->x, agent->y) > maxRange) continue;
        auto* item = ItemMgr::GetItemById(itemAgent->item_id);
        if (!item || !predicate(item)) continue;
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
                                BossKeyItemPredicateFn is_boss_key) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t myId = me ? me->agent_id : 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    const char* prefix = log_prefix ? log_prefix : "DungeonLoot";
    const BossKeyItemPredicateFn predicate = is_boss_key ? is_boss_key : &IsBossKeyLikeItem;
    uint32_t matches = 0u;
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) continue;
        auto* itemAgent = static_cast<const AgentItem*>(agent);
        if (itemAgent->owner != 0u && itemAgent->owner != myId) continue;
        const float dist = AgentMgr::GetDistance(x, y, agent->x, agent->y);
        if (dist > maxRange) continue;
        auto* item = ItemMgr::GetItemById(itemAgent->item_id);
        if (!item || !predicate(item)) continue;
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
    const BossKeyItemPredicateFn predicate = options.is_boss_key ? options.is_boss_key : &IsBossKeyLikeItem;
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
            if (!item || !predicate(item)) continue;

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

        const uint32_t remaining = CountNearbyBossKeyCandidates(centerX, centerY, scanRange, predicate);
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
        options.is_boss_key);

    BossKeyPickupOptions forceOptions = options.force_pickup;
    if (!forceOptions.log_prefix) {
        forceOptions.log_prefix = prefix;
    }
    if (!forceOptions.is_boss_key) {
        forceOptions.is_boss_key = options.is_boss_key;
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
            options.is_boss_key);
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
            options.is_boss_key);
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
        options.is_boss_key);
    LogNearbyBossKeyCandidates(
        "AcquireBossKey final",
        options.key_x,
        options.key_y,
        options.key_scan_range,
        prefix,
        options.loot,
        options.is_boss_key);
    Log::Warn("%s: AcquireBossKey incomplete nearbyKeys=%u player=(%.0f, %.0f)",
              prefix,
              nearbyKeys,
              meX,
              meY);
    if (nearbyKeys > 0u && options.open_door_at) {
        Log::Info("%s: AcquireBossKey attempting door-open validation despite persistent key agent", prefix);
        if (options.open_door_at(options.boss_door_x, options.boss_door_y)) {
            Log::Info("%s: AcquireBossKey accepted via door-open validation", prefix);
            return true;
        }
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
        Log::Info("%s: OpenChestAt attempt %d signpost=%u near (%.0f, %.0f)",
                  prefix,
                  attempt,
                  chestId,
                  chestX,
                  chestY);
        AgentMgr::InteractSignpost(chestId);
        CallWait(wait_ms, options.interact_delay_ms);
        picked += PickUpNearbyLoot(options.pickup_range, wait_ms, is_dead, options.loot);
        if (chest && move_to_point) {
            move_to_point(chest->x, chest->y, options.chest_move_threshold);
        }
    }

    Log::Info("%s: OpenChestAt result signpost=%u picked=%d", prefix, chestId, picked);
    const bool chestStillPresent = DungeonInteractions::IsChestStillPresentNear(chestX, chestY, searchRadius);
    if (picked > 0 || !chestStillPresent) {
        tracker.MarkOpened(chestId);
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

} // namespace GWA3::DungeonLoot
