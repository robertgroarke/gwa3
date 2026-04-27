// Froggy loot and route-adjacent runtime helpers. Included by FroggyHM.cpp.

static int LootAfterCombatSweep(float aggroRange, const char* reason) {
    DungeonLoot::PostCombatLootSweepOptions options = {};
    options.log_prefix = "Froggy";
    options.reason = reason;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.is_world_ready = &DungeonLoot::IsWorldReadyForLootWithPlayerAgent;
    options.pickup_nearby_loot = &PickupNearbyLoot;
    return DungeonLoot::SweepPostCombatLoot(aggroRange, options);
}
