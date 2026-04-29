#pragma once

#include <cstdint>

namespace GWA3 {
struct Agent;
struct Item;
}

namespace GWA3::DungeonInteractions {
class OpenedChestTracker;
}

namespace GWA3::DungeonLoot {

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
using MoveToPointResultFn = bool(*)(float x, float y, float threshold);
using CombatMoveToFn = void(*)(float x, float y, float fightRange);
using PickupNearbyLootFn = int(*)(float maxRange);
using OpenChestAtFn = bool(*)(float x, float y, float searchRadius);
using ChestBundleOpenFn = bool(*)(float chestX, float chestY, float searchRadius);
using SignpostScanLogFn = void(*)(float x, float y, float maxDist, const char* label, bool chestOnly);
using BossKeyItemPredicateFn = bool(*)(const Item* item);

struct BossKeyModelSet {
    const uint32_t* model_ids = nullptr;
    int model_count = 0;
    bool accept_type_key = true;
};

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

struct ChestOpenOptions {
    float move_threshold = 200.0f;
    uint32_t interact_delay_ms = 2000u;
    float pickup_range = 800.0f;
    LootPickupOptions loot = {};
};

struct ResolvedChestOpenOptions {
    float chest_move_threshold = 120.0f;
    float fallback_move_threshold = 200.0f;
    uint32_t interact_delay_ms = 5000u;
    int attempts = 2;
    float pickup_range = 5000.0f;
    const char* log_prefix = nullptr;
    LootPickupOptions loot = {};
};

struct ChestBundleFallbackOptions {
    float min_signpost_radius = 1500.0f;
    float min_loot_radius = 18000.0f;
    float loot_radius_multiplier = 4.0f;
    int open_attempts = 2;
    int pickup_attempts = 3;
    uint32_t open_retry_delay_ms = 500u;
    uint32_t pickup_retry_delay_ms = 500u;
    uint32_t verify_delay_ms = 500u;
    const char* log_prefix = nullptr;
};

struct ChestAtOpenOptions {
    float live_player_search_radius_min = 20000.0f;
    float fallback_search_radius_min = 1500.0f;
    float search_radius_multiplier = 4.0f;
    const char* log_prefix = nullptr;
    ChestBundleOpenFn bundle_open = nullptr;
    bool use_bundle_fallback = false;
    ChestBundleFallbackOptions bundle_fallback = {};
    SignpostScanLogFn signpost_scan_log = nullptr;
    ChestOpenOptions nearby = {};
    ResolvedChestOpenOptions resolved = {};
};

struct BossChestLootOptions {
    float stage_move_threshold = 250.0f;
    int open_attempts = 2;
    uint32_t first_loot_delay_ms = 2000u;
    uint32_t retry_loot_delay_ms = 1000u;
    const char* log_prefix = nullptr;
};

struct BossChestLootResult {
    bool staged = false;
    bool completed = false;
    uint32_t open_attempts = 0u;
    uint32_t open_successes = 0u;
    int picked_loot_count = 0;
};

struct BossKeyPickupOptions {
    float approach_threshold = 90.0f;
    uint32_t settle_delay_ms = 100u;
    uint32_t pickup_retry_limit = 12u;
    uint32_t pickup_timeout_ms = 7000u;
    uint32_t pickup_delay_ms = 250u;
    uint32_t passes = 2u;
    const char* log_prefix = nullptr;
    BossKeyItemPredicateFn is_boss_key = nullptr;
    BossKeyModelSet boss_key_models = {};
    LootPickupOptions loot = {};
};

struct BossKeyAcquireOptions {
    float key_x = 0.0f;
    float key_y = 0.0f;
    float key_scan_range = 0.0f;
    float move_fight_range = 1600.0f;
    float wide_loot_range = 18000.0f;
    float final_loot_range = 4000.0f;
    uint32_t passes = 3u;
    uint32_t pre_scan_wait_ms = 100u;
    uint32_t clear_target_wait_ms = 150u;
    uint32_t retry_wait_ms = 150u;
    const char* log_prefix = nullptr;
    CombatMoveToFn combat_move_to = nullptr;
    PickupNearbyLootFn pickup_nearby_loot = nullptr;
    MoveToPointFn move_to_point = nullptr;
    WaitFn wait_ms = nullptr;
    BoolFn is_dead = nullptr;
    BossKeyItemPredicateFn is_boss_key = nullptr;
    BossKeyModelSet boss_key_models = {};
    LootPickupOptions loot = {};
    BossKeyPickupOptions force_pickup = {};
};

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
bool IsModelInBossKeySet(uint32_t modelId, const BossKeyModelSet& modelSet);
bool IsBossKeyLikeItem(const Item* item);
bool IsBossKeyLikeItem(const Item* item, const BossKeyModelSet& modelSet);
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
uint32_t CountNearbyBossKeyCandidates(float x,
                                      float y,
                                      float maxRange,
                                      BossKeyItemPredicateFn is_boss_key = nullptr,
                                      const BossKeyModelSet& boss_key_models = {});
void LogNearbyBossKeyCandidates(const char* label,
                                float x,
                                float y,
                                float maxRange,
                                const char* log_prefix = nullptr,
                                const LootPickupOptions& options = {},
                                BossKeyItemPredicateFn is_boss_key = nullptr,
                                const BossKeyModelSet& boss_key_models = {});
bool ForcePickUpBossKeyCandidates(float centerX,
                                  float centerY,
                                  float scanRange,
                                  MoveToPointFn move_to_point,
                                  WaitFn wait_ms = nullptr,
                                  BoolFn is_dead = nullptr,
                                  const BossKeyPickupOptions& options = {});
bool AcquireBossKey(const BossKeyAcquireOptions& options);
bool OpenNearbyChest(float maxRange, DungeonInteractions::OpenedChestTracker& tracker,
                     MoveToPointFn move_to_point, WaitFn wait_ms = nullptr, BoolFn is_dead = nullptr,
                     const ChestOpenOptions& options = {});
bool OpenResolvedChestAndPickUpLoot(uint32_t chestId,
                                    float chestX,
                                    float chestY,
                                    float searchRadius,
                                    DungeonInteractions::OpenedChestTracker& tracker,
                                    MoveToPointFn move_to_point,
                                    WaitFn wait_ms = nullptr,
                                    BoolFn is_dead = nullptr,
                                    const ResolvedChestOpenOptions& options = {});
bool OpenChestAt(float chestX,
                 float chestY,
                 float searchRadius,
                 DungeonInteractions::OpenedChestTracker& tracker,
                 MoveToPointFn move_to_point,
                 WaitFn wait_ms = nullptr,
                 BoolFn is_dead = nullptr,
                 const ChestAtOpenOptions& options = {});
bool OpenChestWithBundleFallback(float chestX,
                                 float chestY,
                                 float searchRadius,
                                 WaitFn wait_ms = nullptr,
                                 const ChestBundleFallbackOptions& options = {});
BossChestLootResult OpenBossChestAndLoot(
    float chestX,
    float chestY,
    float searchRadius,
    float lootRadius,
    MoveToPointResultFn move_to_point,
    OpenChestAtFn open_chest_at,
    PickupNearbyLootFn pickup_nearby_loot,
    WaitFn wait_ms = nullptr,
    const BossChestLootOptions& options = {});

} // namespace GWA3::DungeonLoot
