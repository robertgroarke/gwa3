#include <gwa3/bot/DungeonItemActions.h>

#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/TradeMgr.h>

#include <Windows.h>

namespace GWA3::Bot::DungeonItemActions {

namespace {

void CallWait(WaitFn wait_fn, uint32_t ms) {
    if (wait_fn) {
        wait_fn(ms);
        return;
    }
    Sleep(ms);
}

uint32_t FindItemIdByModels(uint32_t model_a, uint32_t model_b) {
    for (uint32_t bag_index = 1u; bag_index <= 4u; ++bag_index) {
        auto* bag = ItemMgr::GetBag(bag_index);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (!item) continue;
            if (item->model_id == model_a || item->model_id == model_b) {
                return item->item_id;
            }
        }
    }
    return 0u;
}

} // namespace

int IdentifyItems(ItemFilterFn should_identify, WaitFn wait_ms, const IdentifyOptions& options) {
    auto* inv = ItemMgr::GetInventory();
    if (!inv || !should_identify) return 0;

    const uint32_t kit_id = FindItemIdByModels(options.normal_kit_model, options.superior_kit_model);
    if (kit_id == 0u) return 0;

    int identified = 0;
    for (uint32_t bag_index = 1u; bag_index <= 4u; ++bag_index) {
        auto* bag = ItemMgr::GetBag(bag_index);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (!item || item->item_id == 0u) continue;
            if (!should_identify(item)) continue;

            ItemMgr::IdentifyItem(item->item_id, kit_id);
            CallWait(wait_ms, options.delay_ms);
            ++identified;

            if (!ItemMgr::GetItemById(kit_id)) {
                return identified;
            }
        }
    }
    return identified;
}

int SalvageItems(ItemFilterFn should_salvage, WaitFn wait_ms, const SalvageOptions& options) {
    auto* inv = ItemMgr::GetInventory();
    if (!inv || !should_salvage) return 0;

    const uint32_t kit_id = FindItemIdByModels(options.salvage_kit_model, options.expert_salvage_kit_model);
    if (kit_id == 0u) return 0;

    int salvaged = 0;
    for (uint32_t bag_index = 1u; bag_index <= 4u; ++bag_index) {
        auto* bag = ItemMgr::GetBag(bag_index);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (!item || item->item_id == 0u) continue;
            if (!should_salvage(item)) continue;

            ItemMgr::SalvageSessionOpen(kit_id, item->item_id);
            CallWait(wait_ms, options.open_delay_ms);
            ItemMgr::SalvageMaterials();
            CallWait(wait_ms, options.materials_delay_ms);
            ItemMgr::SalvageSessionDone();
            CallWait(wait_ms, options.done_delay_ms);
            ++salvaged;

            if (!ItemMgr::GetItemById(kit_id)) {
                return salvaged;
            }
        }
    }
    return salvaged;
}

int SellItems(ItemFilterFn should_sell, WaitFn wait_ms, const SellOptions& options) {
    auto* inv = ItemMgr::GetInventory();
    if (!inv || !should_sell) return 0;

    int sold = 0;
    for (uint32_t bag_index = 1u; bag_index <= 4u; ++bag_index) {
        auto* bag = ItemMgr::GetBag(bag_index);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (!item) continue;
            if (!should_sell(item)) continue;

            TradeMgr::TransactItems(options.transact_type, item->quantity, item->item_id);
            CallWait(wait_ms, options.delay_ms + options.extra_delay_ms);
            ++sold;
        }
    }
    return sold;
}

int DepositItemsToStorage(ItemFilterFn should_store, WaitFn wait_ms, const DepositOptions& options) {
    auto* inv = ItemMgr::GetInventory();
    if (!inv || !should_store) return 0;

    int deposited = 0;
    for (uint32_t bag_index = 1u; bag_index <= 4u; ++bag_index) {
        auto* bag = ItemMgr::GetBag(bag_index);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (!item) continue;
            if (!should_store(item)) continue;

            for (uint32_t storage_bag_index = options.first_storage_bag;
                 storage_bag_index <= options.last_storage_bag;
                 ++storage_bag_index) {
                auto* storage_bag = ItemMgr::GetBag(storage_bag_index);
                if (!storage_bag) continue;
                if (storage_bag->items_count >= storage_bag->items.size) continue;

                ItemMgr::MoveItem(item->item_id, storage_bag_index, storage_bag->items_count);
                CallWait(wait_ms, options.delay_ms);
                ++deposited;
                break;
            }
        }
    }
    return deposited;
}

bool UseItemByModel(uint32_t model_id, WaitFn wait_ms, const UseItemOptions& options) {
    auto* item = DungeonInventory::FindItemByModel(model_id);
    if (!item) return false;

    ItemMgr::UseItem(item->item_id);
    CallWait(wait_ms, options.delay_ms);
    return true;
}

bool UseItemIfEffectMissing(uint32_t agent_id, uint32_t effect_skill_id, uint32_t model_id,
                            WaitFn wait_ms, const UseItemOptions& options) {
    if (agent_id == 0u) return false;
    if (EffectMgr::HasEffect(agent_id, effect_skill_id)) return false;
    return UseItemByModel(model_id, wait_ms, options);
}

} // namespace GWA3::Bot::DungeonItemActions
