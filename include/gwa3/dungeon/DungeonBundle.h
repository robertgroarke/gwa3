#pragma once

#include <cstdint>

namespace GWA3::DungeonInteractions {
class OpenedChestTracker;
}

namespace GWA3::DungeonBundle {

using MoveToPointFn = void(*)(float x, float y, float threshold);

struct DoorOpenOptions {
    float signpost_search_radius = 1500.0f;
    int interact_count = 6;
    uint32_t interact_delay_ms = 500u;
};

struct ChestOpenOptions {
    float signpost_search_radius = 1500.0f;
    float item_search_radius = 18000.0f;
    int interact_count = 2;
    int pickup_attempts = 3;
    uint32_t interact_delay_ms = 500u;
    uint32_t pickup_delay_ms = 500u;
};

bool InteractSignpostNearPoint(
    float x,
    float y,
    float searchRadius = 1500.0f,
    int interactCount = 2,
    uint32_t delayMs = 500u);
bool AutoItGoToSignpostAndAcquireHeldBundleNearPoint(
    float x,
    float y,
    float searchRadius = 1500.0f,
    int passes = 2,
    uint32_t signpostDelayMs = 100u,
    uint32_t acquireTimeoutMs = 1000u);
bool InteractSignpostAndAcquireHeldBundleNearPoint(
    float x,
    float y,
    float searchRadius = 1500.0f,
    int interactCount = 2,
    uint32_t interactDelayMs = 100u,
    uint32_t acquireTimeoutMs = 2000u);
bool InteractSignpostAndPickUpLootNearPoint(
    float x,
    float y,
    float signpostSearchRadius = 1500.0f,
    int interactCount = 2,
    uint32_t interactDelayMs = 500u,
    float lootSearchRadius = 18000.0f,
    uint32_t lootDelayMs = 500u);
bool PickUpNearestItemNearPoint(
    float x,
    float y,
    float searchRadius = 18000.0f,
    int pickupAttempts = 3,
    uint32_t delayMs = 500u);
bool PickUpNearestItemByModelNearPoint(
    float x,
    float y,
    uint32_t modelId,
    float searchRadius = 18000.0f,
    int pickupAttempts = 3,
    uint32_t delayMs = 500u);
bool OpenChestAndPickUpBundle(
    float x,
    float y,
    float signpostSearchRadius = 1500.0f,
    float itemSearchRadius = 18000.0f,
    int interactCount = 2,
    int pickupAttempts = 3,
    uint32_t interactDelayMs = 500u,
    uint32_t pickupDelayMs = 500u);
bool TryOpenChestAt(
    float x,
    float y,
    uint32_t currentMapId,
    DungeonInteractions::OpenedChestTracker& tracker,
    const ChestOpenOptions& options = {});
bool TryOpenDoorAt(
    float x,
    float y,
    const DoorOpenOptions& options = {});
bool ExecuteDoorOpenSequence(
    float x,
    float y,
    MoveToPointFn move_to_point,
    const DoorOpenOptions& options = {},
    float settle_threshold = 200.0f,
    uint32_t settle_delay_ms = 500u);
bool OpenChestAndAcquireHeldBundleByModel(
    float x,
    float y,
    uint32_t modelId,
    float signpostSearchRadius = 1500.0f,
    float itemSearchRadius = 18000.0f,
    int interactCount = 2,
    int pickupAttempts = 3,
    uint32_t interactDelayMs = 500u,
    uint32_t pickupDelayMs = 500u);
bool OpenChestAndAcquireHeldBundleByModelChestPreferred(
    float x,
    float y,
    uint32_t modelId,
    float signpostSearchRadius = 1500.0f,
    float itemSearchRadius = 18000.0f,
    int interactCount = 2,
    int pickupAttempts = 3,
    uint32_t interactDelayMs = 500u,
    uint32_t pickupDelayMs = 500u);
bool OpenChestAndAcquireHeldBundleByModelActionInteract(
    float x,
    float y,
    uint32_t modelId,
    float itemSearchRadius = 18000.0f,
    int interactCount = 2,
    int pickupAttempts = 3,
    uint32_t interactDelayMs = 500u,
    uint32_t pickupDelayMs = 500u);
bool OpenChestAndAcquireHeldBundleByModelLegacy(
    float x,
    float y,
    uint32_t modelId,
    float signpostSearchRadius = 1500.0f,
    float itemSearchRadius = 18000.0f,
    int interactCount = 2,
    int pickupAttempts = 3,
    uint32_t interactDelayMs = 500u,
    uint32_t pickupDelayMs = 500u);

} // namespace GWA3::DungeonBundle
