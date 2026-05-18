#include <gwa3/dungeon/DungeonRunStats.h>

#include <gwa3/advanced/Inventory.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/Title.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/PlayerMgr.h>

#include <algorithm>
#include <iterator>
#include <mutex>

namespace GWA3::DungeonRunStats {
namespace {

struct SessionStats {
    bool title_baseline_ready = false;
    TitleCounters title_baseline = {};
    bool inventory_baseline_ready = false;
    InventoryCounters inventory_baseline = {};
    uint32_t items_picked = 0u;
    uint32_t skins_picked = 0u;
    uint32_t rare_skins = 0u;
    uint32_t gold_items = 0u;
    uint32_t dropped_lockpicks = 0u;
    uint32_t chests_opened = 0u;
    uint32_t black_dyes = 0u;
    uint32_t tomes = 0u;
    uint32_t wipes = 0u;
    uint32_t observed_item_ids[512] = {};
    uint32_t observed_item_count = 0u;
    uint32_t observed_item_cursor = 0u;
    uint32_t observed_chest_ids[128] = {};
    uint32_t observed_chest_count = 0u;
    uint32_t observed_chest_cursor = 0u;
};

std::mutex s_mutex;
SessionStats s_stats = {};

bool IsTomeModel(uint32_t modelId) {
    return modelId >= ItemModelIds::ELITE_WARRIOR_TOME &&
           modelId <= ItemModelIds::DERVISH_TOME;
}

bool IsBlackDye(uint32_t modelId, uint8_t dyeTint) {
    return modelId == ItemModelIds::DYE && dyeTint == 10u;
}

uint32_t QuantityOrOne(uint32_t quantity) {
    return quantity > 0u ? quantity : 1u;
}

bool TryReadTitlePoints(uint32_t titleId, uint32_t& points) {
    Title* title = PlayerMgr::GetTitleTrack(titleId);
    if (!title) {
        return false;
    }

    points = title->current_points;
    return true;
}

TitleCounters ReadTitleCounters() {
    TitleCounters counters;
    uint32_t points = 0u;
    if (TryReadTitlePoints(TitleID::Deldrimor, points)) {
        counters.deldrimor = points;
        counters.available = true;
    }
    if (TryReadTitlePoints(TitleID::Asura, points)) {
        counters.asura = points;
        counters.available = true;
    }
    if (TryReadTitlePoints(TitleID::Norn, points)) {
        counters.norn = points;
        counters.available = true;
    }
    if (TryReadTitlePoints(TitleID::Vanguard, points)) {
        counters.vanguard = points;
        counters.available = true;
    }
    return counters;
}

uint32_t Delta(uint32_t current, uint32_t baseline) {
    return current >= baseline ? current - baseline : 0u;
}

TitleCounters BuildTitleDelta(const TitleCounters& current, const TitleCounters& baseline) {
    TitleCounters delta;
    delta.available = current.available;
    delta.deldrimor = Delta(current.deldrimor, baseline.deldrimor);
    delta.asura = Delta(current.asura, baseline.asura);
    delta.norn = Delta(current.norn, baseline.norn);
    delta.vanguard = Delta(current.vanguard, baseline.vanguard);
    return delta;
}

InventoryCounters ReadInventoryCounters() {
    InventoryCounters counters;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) {
        return counters;
    }

    counters.available = true;
    for (uint32_t bagIdx = 1u; bagIdx <= 4u; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) {
            continue;
        }

        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item) {
                continue;
            }

            const uint32_t quantity = QuantityOrOne(item->quantity);
            if (item->model_id == ItemModelIds::LOCKPICK) {
                counters.lockpicks += quantity;
            }
            if (MaintenanceMgr::IsRareSkin(item->model_id)) {
                ++counters.rare_skins;
            }
            if (AdvancedInventory::GetItemRarity(item) == AdvancedInventory::RARITY_GOLD &&
                item->type != AdvancedLoot::TYPE_GOLD) {
                ++counters.gold_items;
            }
            if (IsBlackDye(item->model_id, item->dye.dye_tint)) {
                counters.black_dyes += quantity;
            }
            if (IsTomeModel(item->model_id)) {
                counters.tomes += quantity;
            }
        }
    }
    return counters;
}

void EnsureBaselinesLocked() {
    if (!s_stats.title_baseline_ready) {
        const TitleCounters titles = ReadTitleCounters();
        if (titles.available) {
            s_stats.title_baseline = titles;
            s_stats.title_baseline_ready = true;
        }
    }

    if (!s_stats.inventory_baseline_ready) {
        const InventoryCounters inventory = ReadInventoryCounters();
        if (inventory.available) {
            s_stats.inventory_baseline = inventory;
            s_stats.inventory_baseline_ready = true;
        }
    }
}

bool RememberPickedItemLocked(uint32_t itemId) {
    if (itemId == 0u) {
        return true;
    }

    const uint32_t count = std::min<uint32_t>(
        s_stats.observed_item_count,
        static_cast<uint32_t>(std::size(s_stats.observed_item_ids)));
    for (uint32_t i = 0u; i < count; ++i) {
        if (s_stats.observed_item_ids[i] == itemId) {
            return false;
        }
    }

    const uint32_t slot = s_stats.observed_item_cursor %
        static_cast<uint32_t>(std::size(s_stats.observed_item_ids));
    s_stats.observed_item_ids[slot] = itemId;
    ++s_stats.observed_item_cursor;
    if (s_stats.observed_item_count < static_cast<uint32_t>(std::size(s_stats.observed_item_ids))) {
        ++s_stats.observed_item_count;
    }
    return true;
}

bool RememberOpenedChestLocked(uint32_t chestAgentId) {
    if (chestAgentId == 0u) {
        return true;
    }

    const uint32_t count = std::min<uint32_t>(
        s_stats.observed_chest_count,
        static_cast<uint32_t>(std::size(s_stats.observed_chest_ids)));
    for (uint32_t i = 0u; i < count; ++i) {
        if (s_stats.observed_chest_ids[i] == chestAgentId) {
            return false;
        }
    }

    const uint32_t slot = s_stats.observed_chest_cursor %
        static_cast<uint32_t>(std::size(s_stats.observed_chest_ids));
    s_stats.observed_chest_ids[slot] = chestAgentId;
    ++s_stats.observed_chest_cursor;
    if (s_stats.observed_chest_count < static_cast<uint32_t>(std::size(s_stats.observed_chest_ids))) {
        ++s_stats.observed_chest_count;
    }
    return true;
}

Snapshot BuildSnapshotLocked() {
    EnsureBaselinesLocked();

    Snapshot snapshot;
    snapshot.title_baseline_ready = s_stats.title_baseline_ready;
    snapshot.title_total = ReadTitleCounters();
    if (s_stats.title_baseline_ready && snapshot.title_total.available) {
        snapshot.title_delta = BuildTitleDelta(snapshot.title_total, s_stats.title_baseline);
    }

    snapshot.inventory_baseline_ready = s_stats.inventory_baseline_ready;
    snapshot.inventory = ReadInventoryCounters();
    snapshot.items_picked = s_stats.items_picked;
    snapshot.skins_picked = s_stats.skins_picked;
    snapshot.rare_skins = s_stats.rare_skins;
    snapshot.gold_items = s_stats.gold_items;
    snapshot.dropped_lockpicks = s_stats.dropped_lockpicks;
    snapshot.chests_opened = s_stats.chests_opened;
    snapshot.black_dyes = s_stats.black_dyes;
    snapshot.tomes = s_stats.tomes;
    snapshot.wipes = s_stats.wipes;

    if (s_stats.inventory_baseline_ready && snapshot.inventory.available) {
        const uint32_t lockpickDelta = Delta(snapshot.inventory.lockpicks, s_stats.inventory_baseline.lockpicks);
        const uint32_t rareSkinDelta = Delta(snapshot.inventory.rare_skins, s_stats.inventory_baseline.rare_skins);
        const uint32_t goldItemDelta = Delta(snapshot.inventory.gold_items, s_stats.inventory_baseline.gold_items);
        const uint32_t blackDyeDelta = Delta(snapshot.inventory.black_dyes, s_stats.inventory_baseline.black_dyes);
        const uint32_t tomeDelta = Delta(snapshot.inventory.tomes, s_stats.inventory_baseline.tomes);

        snapshot.lockpicks_gained = std::max(snapshot.dropped_lockpicks, lockpickDelta);
        snapshot.rare_skins = std::max(snapshot.rare_skins, rareSkinDelta);
        snapshot.skins_picked = std::max(snapshot.skins_picked, rareSkinDelta);
        snapshot.gold_items = std::max(snapshot.gold_items, goldItemDelta);
        snapshot.black_dyes = std::max(snapshot.black_dyes, blackDyeDelta);
        snapshot.tomes = std::max(snapshot.tomes, tomeDelta);
    } else {
        snapshot.lockpicks_gained = snapshot.dropped_lockpicks;
    }

    return snapshot;
}

} // namespace

void ResetSession() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_stats = {};
    EnsureBaselinesLocked();
}

void EnsureBaselines() {
    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureBaselinesLocked();
}

Snapshot GetSnapshot() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildSnapshotLocked();
}

void RecordPickedLoot(const AdvancedLoot::PickedLootInfo& info) {
    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureBaselinesLocked();
    if (!RememberPickedItemLocked(info.item_id)) {
        return;
    }

    const uint32_t quantity = QuantityOrOne(info.quantity);
    ++s_stats.items_picked;
    if (MaintenanceMgr::IsRareSkin(info.model_id)) {
        ++s_stats.skins_picked;
        ++s_stats.rare_skins;
    }
    if (info.rarity == AdvancedInventory::RARITY_GOLD && info.type != AdvancedLoot::TYPE_GOLD) {
        ++s_stats.gold_items;
    }
    if (info.model_id == ItemModelIds::LOCKPICK) {
        s_stats.dropped_lockpicks += quantity;
    }
    if (IsBlackDye(info.model_id, info.dye_tint)) {
        s_stats.black_dyes += quantity;
    }
    if (IsTomeModel(info.model_id)) {
        s_stats.tomes += quantity;
    }
}

void RecordChestOpened(uint32_t count) {
    if (count == 0u) {
        return;
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureBaselinesLocked();
    s_stats.chests_opened += count;
}

void RecordChestOpenedByAgent(uint32_t chestAgentId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureBaselinesLocked();
    if (!RememberOpenedChestLocked(chestAgentId)) {
        return;
    }
    ++s_stats.chests_opened;
}

void SetWipes(uint32_t wipes) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_stats.wipes = wipes;
}

void PublishMonitoringLog(const char* prefix, const char* reason, uint32_t wipes) {
    const Snapshot snapshot = GetSnapshot();
    const uint32_t publishedWipes = wipes != 0u ? wipes : snapshot.wipes;
    Log::Info("%s: MonitoringStats reason=%s titleReady=%d deldrimor=%u asura=%u norn=%u vanguard=%u lockpicks=%u wipes=%u rareSkins=%u goldItems=%u droppedLockpicks=%u chestsOpened=%u blackDyes=%u tomes=%u deldrimorTotal=%u asuraTotal=%u nornTotal=%u vanguardTotal=%u itemsPicked=%u skinsPicked=%u lockpicksGained=%u inventoryBaselineReady=%d",
              prefix ? prefix : "Dungeon",
              reason ? reason : "snapshot",
              snapshot.title_baseline_ready ? 1 : 0,
              snapshot.title_delta.deldrimor,
              snapshot.title_delta.asura,
              snapshot.title_delta.norn,
              snapshot.title_delta.vanguard,
              snapshot.inventory.lockpicks,
              publishedWipes,
              snapshot.rare_skins,
              snapshot.gold_items,
              snapshot.dropped_lockpicks,
              snapshot.chests_opened,
              snapshot.black_dyes,
              snapshot.tomes,
              snapshot.title_total.deldrimor,
              snapshot.title_total.asura,
              snapshot.title_total.norn,
              snapshot.title_total.vanguard,
              snapshot.items_picked,
              snapshot.skins_picked,
              snapshot.lockpicks_gained,
              snapshot.inventory_baseline_ready ? 1 : 0);
}

} // namespace GWA3::DungeonRunStats
