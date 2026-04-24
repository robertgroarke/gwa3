#pragma once

#include <cstdint>

namespace GWA3 {
struct Agent;
}

namespace GWA3::Bot::DungeonInteractions {
class OpenedChestTracker;
}

namespace GWA3::Bot::DungeonLoot {

inline constexpr uint8_t TYPE_USABLE = 9u;
inline constexpr uint8_t TYPE_DYE = 10u;
inline constexpr uint8_t TYPE_MATERIAL = 11u;
inline constexpr uint8_t TYPE_KEY = 18u;
inline constexpr uint8_t TYPE_GOLD = 20u;
inline constexpr uint8_t TYPE_TROPHY = 30u;
inline constexpr uint8_t TYPE_SCROLL = 31u;
inline constexpr uint8_t TYPE_BUNDLE = 6u;

using WaitFn = void(*)(uint32_t ms);
using BoolFn = bool(*)();
using MoveToPointFn = void(*)(float x, float y, float threshold);

struct LootPickupOptions {
    uint32_t general_loot_min_free_slots = 2u;
    float interact_threshold = 200.0f;
    uint32_t move_timeout_ms = 5000u;
    uint32_t move_poll_ms = 100u;
    uint32_t pickup_retry_limit = 10u;
    uint32_t pickup_timeout_ms = 6000u;
    uint32_t pickup_delay_ms = 250u;
    uint32_t global_timeout_ms = 120000u;
    uint32_t character_gold_cap = 100000u;
};

struct ChestOpenOptions {
    float move_threshold = 200.0f;
    uint32_t interact_delay_ms = 2000u;
    float pickup_range = 800.0f;
    LootPickupOptions loot = {};
};

bool IsAlwaysPickupModel(uint32_t modelId);
bool IsQuestPickupModel(uint32_t modelId);
bool ShouldPickUpItemAgent(const Agent* agent, uint32_t myAgentId, uint32_t freeSlots,
                           const LootPickupOptions& options = {});
int PickUpNearbyLoot(float maxRange, WaitFn wait_ms = nullptr, BoolFn is_dead = nullptr,
                     const LootPickupOptions& options = {});
bool OpenNearbyChest(float maxRange, DungeonInteractions::OpenedChestTracker& tracker,
                     MoveToPointFn move_to_point, WaitFn wait_ms = nullptr, BoolFn is_dead = nullptr,
                     const ChestOpenOptions& options = {});

} // namespace GWA3::Bot::DungeonLoot
