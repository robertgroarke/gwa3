#include <gwa3/bot/DungeonLoot.h>

#include <gwa3/bot/DungeonInteractions.h>
#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>

namespace GWA3::Bot::DungeonLoot {

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
    case 25410u: case 25413u: case 25416u:
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

bool ShouldPickUpItemAgent(const Agent* agent, uint32_t myAgentId, uint32_t freeSlots,
                           const LootPickupOptions& options) {
    if (!agent || agent->type != 0x400u) return false;
    auto* itemAgent = static_cast<const AgentItem*>(agent);
    if (itemAgent->owner != 0u && itemAgent->owner != myAgentId) return false;

    auto* item = ItemMgr::GetItemById(itemAgent->item_id);
    if (!item) return false;

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

int PickUpNearbyLoot(float maxRange, WaitFn wait_ms, BoolFn is_dead, const LootPickupOptions& options) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    const uint32_t myId = me->agent_id;

    uint32_t freeSlots = DungeonInventory::CountFreeSlots();
    if (freeSlots == 0u) return 0;

    const DWORD globalStart = GetTickCount();
    int picked = 0;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents && freeSlots > 0u; ++i) {
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
            ItemMgr::PickUpItem(itemAgentId);
            CallWait(wait_ms, options.pickup_delay_ms);
            ++retries;
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
            continue;
        }

        ++picked;
        if (!goldChanged) {
            --freeSlots;
        }
        me = AgentMgr::GetMyAgent();
        if (!me) return picked;
        if ((GetTickCount() - globalStart) > options.global_timeout_ms) {
            return picked;
        }
    }

    return picked;
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

} // namespace GWA3::Bot::DungeonLoot
