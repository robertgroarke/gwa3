// Consolidated test module generated from small test files.
#include "DungeonLootTestSupport.h"
#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/testing/TestFramework.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/ItemModelIds.h>

// --- tests/test_dungeon_loot_acquire_boss_key_split.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_acquire_boss_key_split {


using namespace GWA3::DungeonLoot;
using namespace GWA3::Tests::DungeonLootSupport;

GWA3_TEST(dungeon_loot_acquire_boss_key_does_not_open_door_for_validation, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    ResetBossKeyCallbacks();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 300u, 99999u, TYPE_KEY, 1u, 0u, 0u,
                                         GWA3::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::AgentMgr::AddItemAgent(60u, 250.0f, -500.0f, 300u, 0u);

    BossKeyAcquireOptions options;
    options.key_x = 250.0f;
    options.key_y = -500.0f;
    options.key_scan_range = 1000.0f;
    options.passes = 0u;

    GWA3_ASSERT(!AcquireBossKey(options));
    GWA3_ASSERT_EQ(OpenDoorCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_acquire_boss_key_split

// --- tests/test_dungeon_loot_acquire_boss_key_pass.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_acquire_boss_key_pass {


using namespace GWA3::DungeonLoot;
using namespace GWA3::Tests::DungeonLootSupport;

GWA3_TEST(dungeon_loot_acquire_boss_key_runs_shared_pass_until_clear, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    ResetBossKeyCallbacks();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);

    BossKeyAcquireOptions options;
    options.key_x = 1234.0f;
    options.key_y = -5678.0f;
    options.key_scan_range = 9000.0f;
    options.passes = 1u;
    options.pre_scan_wait_ms = 0u;
    options.clear_target_wait_ms = 0u;
    options.retry_wait_ms = 0u;
    options.combat_move_to = &RecordBossKeyCombatMove;
    options.pickup_nearby_loot = &RecordBossKeyPickupNearbyLoot;
    options.move_to_point = &MoveNear;
    options.force_pickup.pickup_retry_limit = 1u;
    options.force_pickup.pickup_timeout_ms = 0u;

    GWA3_ASSERT(AcquireBossKey(options));
    GWA3_ASSERT_EQ(CombatMoveCount(), 1u);
    GWA3_ASSERT_EQ(PickupNearbyCount(), 1u);
    GWA3_ASSERT_EQ(OpenDoorCount(), 0u);
    GWA3_ASSERT_EQ(static_cast<int>(LastCombatMoveX()), 1234);
    GWA3_ASSERT_EQ(static_cast<int>(LastCombatMoveY()), -5678);
    GWA3_ASSERT_EQ(static_cast<int>(LastCombatMoveRange()), 1600);
    GWA3_ASSERT_EQ(static_cast<int>(LastPickupRange()), 4000);
})

GWA3_TEST(dungeon_loot_acquire_boss_key_frees_slot_when_inventory_full, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    ResetBossKeyCallbacks();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 700u, 16001u, 27u, 1u, 60u, 0u,
                                         GWA3::DungeonInventory::RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 701u, GWA3::ItemModelIds::DUNGEON_KEY_BOGROOT,
                                         TYPE_KEY, 1u, 0u, 0u,
                                         GWA3::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::AgentMgr::AddItemAgent(61u, 250.0f, -500.0f, 701u, 0u);

    BossKeyAcquireOptions options;
    options.key_x = 250.0f;
    options.key_y = -500.0f;
    options.key_scan_range = 1000.0f;
    options.passes = 1u;
    options.pre_scan_wait_ms = 0u;
    options.clear_target_wait_ms = 0u;
    options.retry_wait_ms = 0u;
    options.move_to_point = &MoveNear;
    options.force_pickup.pickup_retry_limit = 0u;
    options.force_pickup.pickup_timeout_ms = 0u;
    options.force_pickup.settle_delay_ms = 0u;

    (void)AcquireBossKey(options);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 700u);
    GWA3_ASSERT_EQ(GWA3::DungeonInventory::CountFreeSlots(), 1u);
})

GWA3_TEST(dungeon_loot_required_bundle_pickup_frees_slot_when_inventory_full, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 800u, 16001u, 27u, 1u, 60u, 0u,
                                         GWA3::DungeonInventory::RARITY_WHITE);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 1u, 801u, 22342u, TYPE_BUNDLE, 1u, 0u, 0u,
                                         GWA3::DungeonInventory::RARITY_WHITE);
    GWA3::TestStubs::AgentMgr::AddItemAgent(62u, 25.0f, 0.0f, 801u, 0u);

    LootPickupOptions options;
    options.log_prefix = "DungeonTest";
    options.pickup_retry_limit = 1u;
    options.pickup_timeout_ms = 1000u;
    options.pickup_delay_ms = 0u;
    const int picked = PickUpNearbyLoot(200.0f, nullptr, nullptr, options);

    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastDroppedItemId(), 800u);
    GWA3_ASSERT_EQ(GWA3::DungeonInventory::CountFreeSlots(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastPickedItemAgentId(), 62u);
    GWA3_ASSERT_EQ(picked, 1);
    GWA3_ASSERT_EQ(GWA3::DungeonInteractions::GetHeldBundleItemId(), 801u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_acquire_boss_key_pass

// --- tests/test_dungeon_loot_boss_key_model_set.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_boss_key_model_set {

using namespace GWA3::DungeonLoot;

GWA3_TEST(dungeon_loot_boss_key_model_set_matches_explicit_models, {
    constexpr uint32_t models[] = {123u, 456u};
    BossKeyModelSet modelSet;
    modelSet.model_ids = models;
    modelSet.model_count = 2;
    modelSet.accept_type_key = false;

    GWA3_ASSERT(IsModelInBossKeySet(123u, modelSet));
    GWA3_ASSERT(!IsModelInBossKeySet(789u, modelSet));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_boss_key_model_set

// --- tests/test_dungeon_loot_open_chest_at_bundle.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_chest_at_bundle {


using namespace GWA3::DungeonLoot;
using namespace GWA3::Tests::DungeonLootSupport;

GWA3_TEST(dungeon_loot_open_chest_at_uses_bundle_resolver_first, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    ResetBossKeyCallbacks();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    SetBundleOpenResult(true);

    GWA3::DungeonInteractions::OpenedChestTracker tracker;
    ChestAtOpenOptions options;
    options.bundle_open = &RecordBundleOpen;

    GWA3_ASSERT(OpenChestAt(100.0f, 0.0f, 500.0f, tracker, &MoveNear, nullptr, nullptr, options));
    GWA3_ASSERT_EQ(BundleOpenCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedSignpostId(), 0u);
})

GWA3_TEST(dungeon_loot_open_chest_at_accepts_bundle_fallback_when_chest_signpost_lingers, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddGadgetAgent(90u, 100.0f, 0.0f, 6062u);
    GWA3::TestStubs::AgentMgr::AddItemAgent(91u, 120.0f, 0.0f, 310u, 0u);

    GWA3::DungeonInteractions::OpenedChestTracker tracker;
    ChestAtOpenOptions options;
    options.use_bundle_fallback = true;
    options.bundle_fallback.open_retry_delay_ms = 0u;
    options.bundle_fallback.pickup_retry_delay_ms = 0u;
    options.bundle_fallback.verify_delay_ms = 0u;

    GWA3_ASSERT(OpenChestAt(100.0f, 0.0f, 500.0f, tracker, &MoveNear, nullptr, nullptr, options));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedSignpostId(), 90u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastPickedItemAgentId(), 91u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_chest_at_bundle

// --- tests/test_dungeon_loot_open_chest_at_live_player.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_chest_at_live_player {


using namespace GWA3::DungeonLoot;
using namespace GWA3::Tests::DungeonLootSupport;

GWA3_TEST(dungeon_loot_open_chest_at_falls_back_to_live_player_chest, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddGadgetAgent(80u, 50.0f, 0.0f, 6062u);

    GWA3::DungeonInteractions::OpenedChestTracker tracker;
    ChestAtOpenOptions options;
    options.resolved.interact_delay_ms = 0u;
    options.resolved.attempts = 1;
    options.live_player_search_radius_min = 1000.0f;

    GWA3_ASSERT(OpenChestAt(5000.0f, 0.0f, 100.0f, tracker, &MoveNear, nullptr, nullptr, options));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedSignpostId(), 80u);
    GWA3_ASSERT(tracker.IsOpened(80u));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_chest_at_live_player

// --- tests/test_dungeon_loot_open_chest_at_resolved.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_chest_at_resolved {


using namespace GWA3::DungeonLoot;
using namespace GWA3::Tests::DungeonLootSupport;

GWA3_TEST(dungeon_loot_open_chest_at_resolves_target_chest_and_picks_loot, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 3u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 310u, 930u, TYPE_MATERIAL, 1u, 0u, 0u,
                                         GWA3::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::AgentMgr::AddGadgetAgent(70u, 100.0f, 0.0f, 6062u);
    GWA3::TestStubs::AgentMgr::AddItemAgent(71u, 120.0f, 0.0f, 310u, 1u);

    GWA3::DungeonInteractions::OpenedChestTracker tracker;
    ChestAtOpenOptions options;
    options.resolved.interact_delay_ms = 0u;
    options.resolved.attempts = 1;

    GWA3_ASSERT(OpenChestAt(100.0f, 0.0f, 500.0f, tracker, &MoveNear, nullptr, nullptr, options));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedSignpostId(), 70u);
    GWA3_ASSERT(tracker.IsOpened(70u));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_chest_at_resolved

// --- tests/test_dungeon_loot_open_nearby_chest.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_nearby_chest {


using namespace GWA3::DungeonLoot;
using namespace GWA3::Tests::DungeonLootSupport;

GWA3_TEST(dungeon_loot_open_nearby_chest_marks_tracker_and_interacts, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddGadgetAgent(50u, 100.0f, 0.0f, 6062u);

    GWA3::DungeonInteractions::OpenedChestTracker tracker;
    tracker.ResetForMap(0u);

    GWA3_ASSERT(OpenNearbyChest(200.0f, tracker, &MoveNear));
    GWA3_ASSERT(tracker.IsOpened(50u));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedSignpostId(), 50u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_open_nearby_chest

// --- tests/test_dungeon_loot_pick_up_nearby_loot.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_pick_up_nearby_loot {


using namespace GWA3::DungeonLoot;

GWA3_TEST(dungeon_loot_pick_up_nearby_loot_picks_eligible_item, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 3u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 200u, 930u, TYPE_MATERIAL, 1u, 0u, 0u,
                                         GWA3::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::AgentMgr::AddItemAgent(40u, 50.0f, 0.0f, 200u, 1u);

    const int picked = PickUpNearbyLoot(200.0f);
    GWA3_ASSERT_EQ(picked, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastPickedItemAgentId(), 40u);
})

GWA3_TEST(dungeon_loot_world_ready_requires_loaded_map, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::MapMgr::SetLoadingState(0u);
    GWA3_ASSERT(!IsWorldReadyForLoot());
    GWA3_ASSERT(!IsWorldReadyForLootWithPlayerAgent());

    GWA3::TestStubs::MapMgr::SetLoadingState(1u);
    GWA3_ASSERT(IsWorldReadyForLoot());
    GWA3_ASSERT(IsWorldReadyForLootWithPlayerAgent());
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_pick_up_nearby_loot

// --- tests/test_dungeon_loot_pickup_model_filters.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_pickup_model_filters {

using namespace GWA3::DungeonLoot;

GWA3_TEST(dungeon_loot_pickup_model_filters_match_froggy_rules, {
    GWA3_ASSERT(IsAlwaysPickupModel(930u));
    GWA3_ASSERT(IsAlwaysPickupModel(22751u));
    GWA3_ASSERT(!IsAlwaysPickupModel(12345u));

    GWA3_ASSERT(IsQuestPickupModel(22342u));
    GWA3_ASSERT(IsQuestPickupModel(21796u));
    GWA3_ASSERT(!IsQuestPickupModel(9999u));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_pickup_model_filters

// --- tests/test_dungeon_loot_post_combat_range.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_post_combat_range {

using namespace GWA3::DungeonLoot;

GWA3_TEST(dungeon_loot_post_combat_range_clamps_froggy_sweep, {
    GWA3_ASSERT_EQ(static_cast<int>(ComputePostCombatLootRange(500.0f)), 2200);
    GWA3_ASSERT_EQ(static_cast<int>(ComputePostCombatLootRange(1600.0f)), 3200);
    GWA3_ASSERT_EQ(static_cast<int>(ComputePostCombatLootRange(4000.0f)), 5000);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_post_combat_range

// --- tests/test_dungeon_loot_post_combat_sweep.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_post_combat_sweep {


using namespace GWA3::DungeonLoot;
using namespace GWA3::Tests::DungeonLootSupport;

GWA3_TEST(dungeon_loot_post_combat_sweep_retries_three_quiet_empty_passes, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    ResetBossKeyCallbacks();

    PostCombatLootSweepOptions options;
    options.pass_wait_ms = 0u;
    options.pickup_nearby_loot = &RecordBossKeyPickupNearbyLoot;

    GWA3_ASSERT_EQ(SweepPostCombatLoot(1600.0f, options), 0);
    GWA3_ASSERT_EQ(PickupNearbyCount(), 3u);
    GWA3_ASSERT_EQ(static_cast<int>(LastPickupRange()), 3200);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_post_combat_sweep

// --- tests/test_dungeon_loot_should_pick_up_item_agent.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_loot_should_pick_up_item_agent {


using namespace GWA3::DungeonLoot;

GWA3_TEST(dungeon_loot_should_pick_up_item_agent_honors_owner_and_inventory_guard, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetGold(0u, 0u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 100u, 930u, TYPE_MATERIAL, 1u, 0u, 0u,
                                         GWA3::DungeonInventory::RARITY_GOLD);
    GWA3::TestStubs::AgentMgr::AddItemAgent(30u, 100.0f, 0.0f, 100u, 1u);

    auto* ownedAgent = GWA3::AgentMgr::GetAgentByID(30u);
    auto* itemAgent = static_cast<const GWA3::AgentItem*>(ownedAgent);
    GWA3_ASSERT_EQ(itemAgent->owner, 1u);
    GWA3_ASSERT_EQ(itemAgent->item_id, 100u);
    GWA3_ASSERT(!ShouldPickUpItemAgent(ownedAgent, 1u, 0u));
    GWA3_ASSERT(ShouldPickUpItemAgent(ownedAgent, 1u, 2u));
    GWA3_ASSERT(!ShouldPickUpItemAgent(ownedAgent, 2u, 0u));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_loot_should_pick_up_item_agent
