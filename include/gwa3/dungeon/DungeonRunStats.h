#pragma once

#include <gwa3/advanced/Loot.h>

#include <cstdint>

namespace GWA3::DungeonRunStats {

struct TitleCounters {
    bool available = false;
    uint32_t deldrimor = 0u;
    uint32_t asura = 0u;
    uint32_t norn = 0u;
    uint32_t vanguard = 0u;
};

struct InventoryCounters {
    bool available = false;
    uint32_t lockpicks = 0u;
    uint32_t rare_skins = 0u;
    uint32_t gold_items = 0u;
    uint32_t black_dyes = 0u;
    uint32_t tomes = 0u;
};

struct Snapshot {
    bool title_baseline_ready = false;
    TitleCounters title_delta = {};
    TitleCounters title_total = {};
    bool inventory_baseline_ready = false;
    InventoryCounters inventory = {};
    uint32_t lockpicks_gained = 0u;
    uint32_t wipes = 0u;
    uint32_t items_picked = 0u;
    uint32_t skins_picked = 0u;
    uint32_t rare_skins = 0u;
    uint32_t gold_items = 0u;
    uint32_t dropped_lockpicks = 0u;
    uint32_t chests_opened = 0u;
    uint32_t black_dyes = 0u;
    uint32_t tomes = 0u;
};

void ResetSession();
void EnsureBaselines();
Snapshot GetSnapshot();

void RecordPickedLoot(const AdvancedLoot::PickedLootInfo& info);
void RecordChestOpened(uint32_t count = 1u);
void RecordChestOpenedByAgent(uint32_t chestAgentId);
void SetWipes(uint32_t wipes);

void PublishMonitoringLog(const char* prefix, const char* reason, uint32_t wipes = 0u);

} // namespace GWA3::DungeonRunStats
