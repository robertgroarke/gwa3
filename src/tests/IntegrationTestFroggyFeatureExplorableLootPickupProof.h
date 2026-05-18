#include "IntegrationTestFroggyFeatureExplorableLootPickupSupport.h"

static bool RunExplorableLootPickupProof() {
    FroggyFeatureReport("=== PHASE 5K: Explorable Loot Pickup Proof ===");

    if (!ValidateExplorableLootPickupContext()) return false;

    ExplorableLootCandidate candidate = {};
    if (!FindExplorableLootCandidate(candidate)) return false;

    const InventorySnapshot inventoryBefore = CaptureInventorySnapshot();
    ReportExplorableLootCandidate(candidate, inventoryBefore);
    MoveNearExplorableLootCandidate(candidate);

    const bool pickedUpIntoInventory = AttemptExplorableLootPickup(candidate, inventoryBefore);
    return ValidateExplorableLootPickupResult(candidate, inventoryBefore, pickedUpIntoInventory);
}
