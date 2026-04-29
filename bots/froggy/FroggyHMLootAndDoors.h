// Froggy loot, boss-key, chest, and door helpers. Included by FroggyHM.cpp
// while these thin adapters are progressively moved into shared dungeon helpers.

#include <gwa3/game/ItemModelIds.h>

// ===== Loot Pickup =====

inline constexpr uint32_t BOGROOT_BOSS_KEY_MODELS[] = {
    ItemModelIds::DUNGEON_KEY_SORROWS_FURNACE,
    ItemModelIds::DUNGEON_KEY_PRISON,
    ItemModelIds::DUNGEON_KEY_BOGROOT,
};

static int PickupNearbyLoot(float maxRange) {
    DungeonLoot::LootPickupOptions options;
    options.is_world_ready = &DungeonLoot::IsWorldReadyForLoot;
    options.log_prefix = "Froggy";
    return DungeonLoot::PickUpNearbyLoot(maxRange, &DungeonRuntime::WaitMs, &IsDead, options);
}

static void LogNearbyBogrootBossKeyCandidates(const char* label, float x, float y, float maxRange) {
    DungeonLoot::BossKeyModelSet modelSet;
    modelSet.model_ids = BOGROOT_BOSS_KEY_MODELS;
    modelSet.model_count = static_cast<int>(sizeof(BOGROOT_BOSS_KEY_MODELS) / sizeof(BOGROOT_BOSS_KEY_MODELS[0]));
    modelSet.accept_type_key = true;
    DungeonLoot::LogNearbyBossKeyCandidates(
        label,
        x,
        y,
        maxRange,
        "Froggy",
        {},
        nullptr,
        modelSet);
}

static void MoveToBogrootBossKeyWithAggro(float x, float y, float fightRange) {
    AggroMoveToEx(x, y, fightRange);
}

static bool AcquireBogrootBossKey() {
    DungeonLoot::BossKeyAcquireOptions options;
    options.key_x = BOGROOT_BOSS_KEY_X;
    options.key_y = BOGROOT_BOSS_KEY_Y;
    options.key_scan_range = BOGROOT_BOSS_KEY_SCAN_RANGE;
    options.log_prefix = "Froggy";
    options.combat_move_to = &MoveToBogrootBossKeyWithAggro;
    options.pickup_nearby_loot = &PickupNearbyLoot;
    options.move_to_point = &DungeonNavigation::MoveToPoint;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.is_dead = &IsDead;
    options.boss_key_models.model_ids = BOGROOT_BOSS_KEY_MODELS;
    options.boss_key_models.model_count = static_cast<int>(sizeof(BOGROOT_BOSS_KEY_MODELS) / sizeof(BOGROOT_BOSS_KEY_MODELS[0]));
    options.boss_key_models.accept_type_key = true;
    options.loot.is_world_ready = &DungeonLoot::IsWorldReadyForLoot;
    options.loot.log_prefix = "Froggy";
    options.force_pickup.log_prefix = "Froggy";
    return DungeonLoot::AcquireBossKey(options);
}

// Known chest gadget IDs
// Track opened chests to avoid re-interaction
static DungeonInteractions::OpenedChestTracker s_openedChestTracker;

static void ResetOpenedChestTracker(const char* reason) {
    DungeonInteractions::ResetOpenedChestTrackerForCurrentMap(s_openedChestTracker, reason, "Froggy");
}

static bool OpenChestAt(float chestX, float chestY, float searchRadius) {
    DungeonLoot::ChestAtOpenOptions options;
    options.log_prefix = "Froggy";
    options.use_bundle_fallback = true;
    options.bundle_fallback.min_signpost_radius = CHEST_BUNDLE_MIN_SIGNPOST_RADIUS;
    options.bundle_fallback.min_loot_radius = CHEST_BUNDLE_MIN_LOOT_RADIUS;
    options.bundle_fallback.loot_radius_multiplier = CHEST_BUNDLE_LOOT_RADIUS_MULTIPLIER;
    options.bundle_fallback.open_attempts = CHEST_BUNDLE_OPEN_ATTEMPTS;
    options.bundle_fallback.pickup_attempts = CHEST_BUNDLE_PICKUP_ATTEMPTS;
    options.bundle_fallback.open_retry_delay_ms = CHEST_BUNDLE_OPEN_RETRY_DELAY_MS;
    options.bundle_fallback.pickup_retry_delay_ms = CHEST_BUNDLE_PICKUP_RETRY_DELAY_MS;
    options.bundle_fallback.verify_delay_ms = CHEST_BUNDLE_VERIFY_DELAY_MS;
    options.bundle_fallback.log_prefix = "Froggy";
    options.signpost_scan_log = &LogNearbySignposts;
    options.nearby.loot.is_world_ready = &DungeonLoot::IsWorldReadyForLoot;
    options.nearby.loot.log_prefix = "Froggy";
    options.resolved.log_prefix = "Froggy";
    options.resolved.loot.is_world_ready = &DungeonLoot::IsWorldReadyForLoot;
    options.resolved.loot.log_prefix = "Froggy";
    return DungeonLoot::OpenChestAt(
        chestX,
        chestY,
        searchRadius,
        s_openedChestTracker,
        &DungeonNavigation::MoveToPoint,
        &DungeonRuntime::WaitMs,
        &IsDead,
        options);
}

static bool OpenDungeonDoorAt(float doorX, float doorY) {
    DungeonInteractions::DoorOpenOptions options;
    options.log_prefix = "Froggy";
    options.signpost_scan_log = &LogNearbySignposts;
    options.agent_log = &LogAgentIdentity;
    options.failure_probe = &LogNearbyBogrootBossKeyCandidates;
    return DungeonInteractions::OpenDoorAtWithProbe(
        doorX,
        doorY,
        BOGROOT_BOSS_KEY_DOOR_PROBE_X,
        BOGROOT_BOSS_KEY_DOOR_PROBE_Y,
        &MoveToAndWait,
        &DungeonRuntime::WaitMs,
        options);
}
