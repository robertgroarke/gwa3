#include <gwa3/advanced/Loot.h>

#include <gwa3/advanced/Inventory.h>
#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonRunStats.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>

namespace GWA3::AdvancedLoot {

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

const char* LogPrefix(const LootPickupOptions& options) {
    return options.log_prefix ? options.log_prefix : "AdvancedLoot";
}

struct FailedPickupEntry {
    uint32_t map_id = 0u;
    uint32_t agent_id = 0u;
    uint32_t item_id = 0u;
    DWORD suppress_until_ms = 0u;
};

FailedPickupEntry g_failedPickups[32] = {};
uint32_t g_failedPickupNext = 0u;

bool ItemRequiresInventorySlot(const Item* item) {
    return item != nullptr && item->type != TYPE_GOLD;
}

bool IsRequiredPickupItem(const Item* item) {
    if (!item) return false;
    return item->type == TYPE_KEY ||
           item->type == TYPE_BUNDLE ||
           IsQuestPickupModel(item->model_id);
}

bool IsHeldBundleItem(uint32_t itemId) {
    const auto* inventory = ItemMgr::GetInventory();
    return inventory != nullptr &&
           inventory->bundle != nullptr &&
           inventory->bundle->item_id == itemId;
}

void EnsureFreeSlotForRequiredPickup(const Item* item,
                                     const LootPickupOptions& options,
                                     WaitFn wait_ms) {
    if (!options.emergency_free_slot_for_required_pickups ||
        !ItemRequiresInventorySlot(item) ||
        !IsRequiredPickupItem(item) ||
        AdvancedInventory::CountFreeSlots() > 0u) {
        return;
    }

    AdvancedInventory::EmergencyFreeSlotOptions freeSlotOptions;
    freeSlotOptions.log_prefix = LogPrefix(options);
    freeSlotOptions.wait_ms = wait_ms;
    freeSlotOptions.allow_green_items = options.emergency_drop_green_items_for_required_pickups;
    const auto result = AdvancedInventory::DropEmergencyInventoryItemForFreeSlot(freeSlotOptions);
    if (result.dropped) {
        Log::Warn("%s: emergency cleanup before required pickup item=%u model=%u reason=%s freeSlots=%u",
                  LogPrefix(options),
                  result.item_id,
                  result.model_id,
                  result.reason ? result.reason : "<unknown>",
                  AdvancedInventory::CountFreeSlots());
    } else {
        Log::Warn("%s: required pickup has full inventory but no emergency drop candidate model=%u type=%u",
                  LogPrefix(options),
                  item ? item->model_id : 0u,
                  item ? item->type : 0u);
    }
}

bool IsWorldReady(const LootPickupOptions& options) {
    return options.is_world_ready ? options.is_world_ready() : true;
}

bool IsFailedPickupSuppressed(uint32_t agentId, uint32_t itemId) {
    const DWORD now = GetTickCount();
    const uint32_t mapId = MapMgr::GetMapId();
    for (const auto& entry : g_failedPickups) {
        if (entry.map_id != mapId || entry.agent_id != agentId || entry.item_id != itemId) continue;
        if (static_cast<int32_t>(entry.suppress_until_ms - now) > 0) return true;
    }
    return false;
}

void RememberFailedPickup(uint32_t agentId, uint32_t itemId, uint32_t suppressMs) {
    if (suppressMs == 0u) return;
    auto& entry = g_failedPickups[g_failedPickupNext++ % (sizeof(g_failedPickups) / sizeof(g_failedPickups[0]))];
    entry.map_id = MapMgr::GetMapId();
    entry.agent_id = agentId;
    entry.item_id = itemId;
    entry.suppress_until_ms = GetTickCount() + suppressMs;
}

} // namespace

LootPickupOptions MakeLootPickupOptions(const char* log_prefix, BoolFn is_world_ready) {
    LootPickupOptions options;
    options.log_prefix = log_prefix;
    options.is_world_ready = is_world_ready;
    return options;
}

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
    const char* prefix = options.log_prefix ? options.log_prefix : "AdvancedLoot";
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

    if (item->type == TYPE_KEY) return true;

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

    const uint16_t rarity = AdvancedInventory::GetItemRarity(item);
    return rarity == AdvancedInventory::RARITY_GOLD && freeSlots >= options.general_loot_min_free_slots;
}

uint32_t CountNearbyPickupCandidates(float maxRange, const LootPickupOptions& options) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;
    const uint32_t myId = me->agent_id;
    const uint32_t freeSlots = AdvancedInventory::CountFreeSlots();

    uint32_t count = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) continue;
        if (AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y) > maxRange) continue;
        auto* itemAgent = static_cast<const AgentItem*>(agent);
        if (IsFailedPickupSuppressed(agent->agent_id, itemAgent->item_id)) continue;
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

    uint32_t freeSlots = AdvancedInventory::CountFreeSlots();

    const DWORD globalStart = GetTickCount();
    int picked = 0;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
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
        auto* itemAgent = static_cast<const AgentItem*>(agent);
        auto* item = ItemMgr::GetItemById(itemAgent->item_id);
        if (!item) continue;
        if (IsFailedPickupSuppressed(agent->agent_id, itemAgent->item_id)) continue;

        freeSlots = AdvancedInventory::CountFreeSlots();
        EnsureFreeSlotForRequiredPickup(item, options, wait_ms);
        freeSlots = AdvancedInventory::CountFreeSlots();
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
        const uint32_t itemId = itemAgent->item_id;
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        PickedLootInfo pickupInfo = {};
        pickupInfo.item_id = itemId;
        pickupInfo.model_id = item->model_id;
        pickupInfo.quantity = item->quantity;
        pickupInfo.value = item->value;
        pickupInfo.rarity = AdvancedInventory::GetItemRarity(item);
        pickupInfo.type = item->type;
        pickupInfo.dye_tint = item->dye.dye_tint;
        pickupInfo.gold_before = goldBefore;
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
            EnsureFreeSlotForRequiredPickup(item, options, wait_ms);
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
            if (IsHeldBundleItem(itemId)) break;
            if (ItemMgr::GetGoldCharacter() != goldBefore) break;
            if (IsDead(is_dead)) return picked;
        }

        const bool goldChanged = ItemMgr::GetGoldCharacter() != goldBefore;
        auto* pickedItem = ItemMgr::GetItemById(itemId);
        const bool movedIntoInventory = pickedItem && pickedItem->bag != nullptr;
        const bool heldBundle = IsHeldBundleItem(itemId);
        const bool pickedUp = !AgentMgr::GetAgentExists(itemAgentId) || goldChanged || movedIntoInventory || heldBundle;
        if (!pickedUp) {
            if (!IsRequiredPickupItem(item)) {
                RememberFailedPickup(itemAgentId, itemId, options.failed_pickup_suppression_ms);
            }
            if (options.log_prefix) {
                Log::Warn("%s: PickUpNearbyLoot failed agent=%u item=%u range=%.0f retries=%u goldBefore=%u goldAfter=%u inventory=%d heldBundle=%d",
                          LogPrefix(options),
                          itemAgentId,
                          itemId,
                          maxRange,
                          retries,
                          goldBefore,
                          ItemMgr::GetGoldCharacter(),
                          movedIntoInventory ? 1 : 0,
                          heldBundle ? 1 : 0);
            }
            continue;
        }

        pickupInfo.gold_after = ItemMgr::GetGoldCharacter();
        pickupInfo.gold_changed = goldChanged;
        DungeonRunStats::RecordPickedLoot(pickupInfo);
        if (options.on_item_picked) {
            options.on_item_picked(pickupInfo, options.on_item_picked_user_data);
        }

        ++picked;
        freeSlots = AdvancedInventory::CountFreeSlots();
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

} // namespace GWA3::AdvancedLoot
