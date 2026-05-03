#pragma once

#include <cstdint>

namespace GWA3 {
struct Agent;
}

namespace GWA3::AdvancedLoot {

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
using PickupNearbyLootFn = int(*)(float maxRange);

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
    BoolFn is_world_ready = nullptr;
    const char* log_prefix = nullptr;
};

LootPickupOptions MakeLootPickupOptions(const char* log_prefix = nullptr,
                                        BoolFn is_world_ready = nullptr);

struct PostCombatLootSweepOptions {
    int max_passes = 6;
    int no_candidate_passes_before_stop = 3;
    int quiet_passes_after_candidates = 2;
    uint32_t pass_wait_ms = 250u;
    const char* log_prefix = nullptr;
    const char* reason = nullptr;
    WaitFn wait_ms = nullptr;
    BoolFn is_world_ready = nullptr;
    PickupNearbyLootFn pickup_nearby_loot = nullptr;
};

bool IsAlwaysPickupModel(uint32_t modelId);
bool IsQuestPickupModel(uint32_t modelId);
bool IsWorldReadyForLoot();
bool IsWorldReadyForLootWithPlayerAgent();
float ComputePostCombatLootRange(float aggroRange,
                                 float minRange = 2200.0f,
                                 float maxRange = 5000.0f);
int SweepPostCombatLoot(float aggroRange, const PostCombatLootSweepOptions& options = {});
bool ShouldPickUpItemAgent(const Agent* agent, uint32_t myAgentId, uint32_t freeSlots,
                           const LootPickupOptions& options = {});
uint32_t CountNearbyPickupCandidates(float maxRange, const LootPickupOptions& options = {});
int PickUpNearbyLoot(float maxRange, WaitFn wait_ms = nullptr, BoolFn is_dead = nullptr,
                     const LootPickupOptions& options = {});

} // namespace GWA3::AdvancedLoot
