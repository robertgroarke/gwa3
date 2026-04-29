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
    if (s_openedChestTracker.count() > 0 || s_openedChestTracker.map_id() != MapMgr::GetMapId()) {
        Log::Info("Froggy: Reset opened chest tracker reason=%s previousMap=%u currentMap=%u previousCount=%u",
                  reason ? reason : "",
                  s_openedChestTracker.map_id(),
                  MapMgr::GetMapId(),
                  static_cast<uint32_t>(s_openedChestTracker.count()));
    }
    s_openedChestTracker.ResetForMap(MapMgr::GetMapId());
}

static bool TryOpenChestViaBundleResolver(float chestX, float chestY, float searchRadius) {
    const float sharedSignpostRadius = max(CHEST_BUNDLE_MIN_SIGNPOST_RADIUS, searchRadius);
    const float sharedLootRadius = max(CHEST_BUNDLE_MIN_LOOT_RADIUS, searchRadius * CHEST_BUNDLE_LOOT_RADIUS_MULTIPLIER);
    if (!DungeonBundle::OpenChestAndPickUpBundle(
            chestX,
            chestY,
            sharedSignpostRadius,
            sharedLootRadius,
            CHEST_BUNDLE_OPEN_ATTEMPTS,
            CHEST_BUNDLE_PICKUP_ATTEMPTS,
            CHEST_BUNDLE_OPEN_RETRY_DELAY_MS,
            CHEST_BUNDLE_PICKUP_RETRY_DELAY_MS)) {
        return false;
    }

    WaitMs(CHEST_BUNDLE_VERIFY_DELAY_MS);
    const bool chestStillPresent = DungeonInteractions::IsChestStillPresentNear(chestX, chestY, searchRadius);
    Log::Info("Froggy: OpenChestAt shared DungeonBundle resolver succeeded target=(%.0f, %.0f) signpostRadius=%.0f lootRadius=%.0f chestStillPresent=%d",
              chestX,
              chestY,
              sharedSignpostRadius,
              sharedLootRadius,
              chestStillPresent ? 1 : 0);
    if (!chestStillPresent) {
        return true;
    }
    Log::Warn("Froggy: OpenChestAt shared resolver reported success but chest still appears present");
    return false;
}

static bool OpenChestAt(float chestX, float chestY, float searchRadius) {
    DungeonLoot::ChestAtOpenOptions options;
    options.log_prefix = "Froggy";
    options.bundle_open = &TryOpenChestViaBundleResolver;
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
    return DungeonInteractions::OpenDoorAt(
        doorX,
        doorY,
        BOGROOT_BOSS_KEY_DOOR_PROBE_X,
        BOGROOT_BOSS_KEY_DOOR_PROBE_Y,
        &MoveToAndWait,
        &DungeonRuntime::WaitMs,
        options);
}
