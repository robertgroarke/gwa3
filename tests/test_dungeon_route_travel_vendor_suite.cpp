// Consolidated test module generated from small test files.
#include "DungeonRouteTestSupport.h"
#include "DungeonTravelTestSupport.h"
#include <cstring>
#include <gwa3/game/ItemModelIds.h>
#include "DungeonVendorTestSupport.h"

// --- tests/test_dungeon_route_checkpoint_labels.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_checkpoint_labels {

using namespace GWA3::DungeonRoute;

GWA3_TEST(dungeon_route_checkpoint_label_detection, {
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::DungeonDoorCheckpoint));
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::QuestDoorCheckpoint));
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::BossLockCheckpoint));
    GWA3_ASSERT(IsCheckpointWaypointLabel(WaypointLabelKind::BlastDoorCheckpoint));
    GWA3_ASSERT(!IsCheckpointWaypointLabel(WaypointLabelKind::DungeonDoor));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_route_checkpoint_labels

// --- tests/test_dungeon_route_generic_restart.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_generic_restart {

using namespace GWA3::DungeonRoute;

GWA3_TEST(dungeon_route_generic_restart_and_stuck_backtrack, {
    GWA3_ASSERT_EQ(ComputeGenericRestartIndex(5, 2), 3);
    GWA3_ASSERT_EQ(ComputeGenericRestartIndex(1, 2), 0);
    GWA3_ASSERT_EQ(ComputeStuckBacktrackIndex(4, 1), 3);
    GWA3_ASSERT_EQ(ComputeStuckBacktrackIndex(0, 1), 0);
    GWA3_ASSERT(!ShouldTriggerStuckBacktrack(4, 5));
    GWA3_ASSERT(ShouldTriggerStuckBacktrack(5, 5));

    auto state = MakeWaypointProgressState(2);
    for (int i = 0; i < 4; ++i) {
        GWA3_ASSERT(!EvaluateWaypointProgress(2, state).backtrack);
    }
    const auto stuck = EvaluateWaypointProgress(2, state);
    GWA3_ASSERT(stuck.backtrack);
    GWA3_ASSERT_EQ(stuck.backtrack_index, 1);

    const auto reset = EvaluateWaypointProgress(4, state);
    GWA3_ASSERT(!reset.backtrack);
    GWA3_ASSERT_EQ(state.last_nearest_index, 4);
    GWA3_ASSERT_EQ(state.unchanged_nearest_count, 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_route_generic_restart

// --- tests/test_dungeon_route_nearest_waypoint.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_nearest_waypoint {

using namespace GWA3::DungeonRoute;

GWA3_TEST(dungeon_route_nearest_waypoint_index, {
    const Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3_ASSERT_EQ(FindNearestWaypointIndex(waypoints, 3, 20.0f, 10.0f), 0);
    GWA3_ASSERT_EQ(FindNearestWaypointIndex(waypoints, 3, 140.0f, 0.0f), 1);
    GWA3_ASSERT_EQ(FindNearestWaypointIndex(waypoints, 3, 280.0f, 5.0f), 2);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_route_nearest_waypoint

// --- tests/test_dungeon_route_numeric_label.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_numeric_label {

using namespace GWA3::DungeonRoute;

GWA3_TEST(dungeon_route_numeric_label_parse, {
    int ordinal = -1;
    GWA3_ASSERT(TryParseWaypointOrdinal("17", ordinal));
    GWA3_ASSERT_EQ(ordinal, 17);
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("17")), static_cast<int>(WaypointLabelKind::Numeric));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_route_numeric_label

// --- tests/test_dungeon_route_restart_rules_non_numeric.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_restart_rules_non_numeric {

using namespace GWA3::DungeonRoute;

GWA3_TEST(dungeon_route_restart_rules_fall_back_for_non_numeric_labels, {
    const Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "Keg"},
        {200.0f, 0.0f, 0.0f, "3"},
    };
    const RestartRule rules[] = {
        {1, 3, 1},
    };

    GWA3_ASSERT_EQ(ComputeRestartIndexFromRules(waypoints, 3, 1, rules, 1, 2), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_route_restart_rules_non_numeric

// --- tests/test_dungeon_route_restart_rules_numeric.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_restart_rules_numeric {

using namespace GWA3::DungeonRoute;

GWA3_TEST(dungeon_route_restart_rules_apply_by_numeric_label, {
    const Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "2"},
        {200.0f, 0.0f, 0.0f, "3"},
        {300.0f, 0.0f, 0.0f, "4"},
        {400.0f, 0.0f, 0.0f, "5"},
        {500.0f, 0.0f, 0.0f, "6"},
    };
    const RestartRule rules[] = {
        {1, 3, 1},
        {4, 6, 3},
    };

    GWA3_ASSERT_EQ(ComputeRestartIndexFromRules(waypoints, 6, 1, rules, 2, 2), 1);
    GWA3_ASSERT_EQ(ComputeRestartIndexFromRules(waypoints, 6, 4, rules, 2, 2), 3);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_route_restart_rules_numeric

// --- tests/test_dungeon_route_special_labels.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_special_labels {

using namespace GWA3::DungeonRoute;

GWA3_TEST(dungeon_route_special_label_classification, {
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Blessing")), static_cast<int>(WaypointLabelKind::Blessing));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Lvl1 to Lvl2")), static_cast<int>(WaypointLabelKind::LevelTransition));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Dungeon Key")), static_cast<int>(WaypointLabelKind::DungeonKey));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Dungeon Door")), static_cast<int>(WaypointLabelKind::DungeonDoor));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Dungeon Door Checkpoint")), static_cast<int>(WaypointLabelKind::DungeonDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Quest Door Checkpoint")), static_cast<int>(WaypointLabelKind::QuestDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Boss 8")), static_cast<int>(WaypointLabelKind::Boss));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Boss Lock Checkpoint 2")), static_cast<int>(WaypointLabelKind::BossLockCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Chest")), static_cast<int>(WaypointLabelKind::Chest));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Signpost")), static_cast<int>(WaypointLabelKind::Signpost));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Keg")), static_cast<int>(WaypointLabelKind::Keg));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Blast Door")), static_cast<int>(WaypointLabelKind::BlastDoor));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Blast Door Checkpoint A")), static_cast<int>(WaypointLabelKind::BlastDoorCheckpoint));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Asura Flame Staff")), static_cast<int>(WaypointLabelKind::AsuraFlameStaff));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Staff Check")), static_cast<int>(WaypointLabelKind::StaffCheck));
    GWA3_ASSERT_EQ(static_cast<int>(ClassifyWaypointLabel("Spider Eggs 2")), static_cast<int>(WaypointLabelKind::SpiderEggs));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_route_special_labels

// --- tests/test_dungeon_route_waypoint_traversal_policy.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_route_waypoint_traversal_policy {

using namespace GWA3::Tests::DungeonRoute;

GWA3_TEST(dungeon_route_waypoint_traversal_policy, {
    ResetMoveRecorder();

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {10.0f, 20.0f, 0.0f, "Move"},
        {30.0f, 40.0f, 1350.0f, "Aggro"},
        {50.0f, 60.0f, 900.0f, "Aggro 2"},
    };

    GWA3::DungeonRoute::ExecuteWaypointTravel(
        waypoints[0],
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedTrue,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveX), 10);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveY), 20);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 250);

    ResetMoveRecorder();
    GWA3::DungeonRoute::ExecuteWaypointTravel(
        waypoints[1],
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedTrue,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 1350);

    ResetMoveRecorder();
    GWA3::DungeonRoute::ExecuteWaypointTravel(
        waypoints[1],
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedFalse,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 1);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastMoveThreshold), 250);

    ResetMoveRecorder();
    GWA3::DungeonRoute::ExecuteReverseWaypointTraversal(
        waypoints,
        2,
        0,
        &RecordMoveToPoint,
        &RecordAggroMoveToPoint,
        &MapLoadedTrue,
        250.0f);
    GWA3_ASSERT_EQ(g_moveCallCount, 3);
    GWA3_ASSERT_EQ(g_moveHistoryCount, 3);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryX[0]), 50);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryValue[0]), 900);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryX[1]), 30);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryValue[1]), 1350);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryX[2]), 10);
    GWA3_ASSERT_EQ(static_cast<int>(g_moveHistoryValue[2]), 250);
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_route_waypoint_traversal_policy

// --- tests/test_dungeon_travel_build_plan_default.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_travel_build_plan_default {


using namespace GWA3::DungeonTravel;

GWA3_TEST(dungeon_travel_build_plan_uses_default_district_zero, {
    const auto plan = BuildRandomDistrictTravelPlan(857u, RandomDistrictPool::EuropePlusInternational, 3u);

    GWA3_ASSERT_EQ(plan.map_id, 857u);
    GWA3_ASSERT_EQ(plan.region, 2);
    GWA3_ASSERT_EQ(plan.district, 0u);
    GWA3_ASSERT_EQ(plan.language, 4u);
    GWA3_ASSERT(std::strcmp(plan.district_name, "eu-it") == 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_travel_build_plan_default

// --- tests/test_dungeon_travel_build_plan_wrap.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_travel_build_plan_wrap {


using namespace GWA3::DungeonTravel;

GWA3_TEST(dungeon_travel_build_plan_wraps_random_index_by_pool_size, {
    const auto europePlan =
        BuildRandomDistrictTravelPlan(200u, RandomDistrictPool::EuropeOnly, 15u);
    const auto intlPlan =
        BuildRandomDistrictTravelPlan(200u, RandomDistrictPool::EuropePlusInternational, 15u);

    GWA3_ASSERT_EQ(europePlan.region, 2);
    GWA3_ASSERT_EQ(europePlan.language, 2u);
    GWA3_ASSERT(std::strcmp(europePlan.district_name, "eu-fr") == 0);

    GWA3_ASSERT_EQ(intlPlan.region, -2);
    GWA3_ASSERT_EQ(intlPlan.language, 0u);
    GWA3_ASSERT(std::strcmp(intlPlan.district_name, "international") == 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_travel_build_plan_wrap

// --- tests/test_dungeon_travel_europe_plus_international_pool.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_travel_europe_plus_international_pool {

using namespace GWA3::DungeonTravel;

GWA3_TEST(dungeon_travel_europe_plus_international_pool_size, {
    GWA3_ASSERT_EQ(GetRandomDistrictPoolSize(RandomDistrictPool::EuropePlusInternational), 8u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_travel_europe_plus_international_pool

// --- tests/test_dungeon_travel_europe_pool.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_travel_europe_pool {

using namespace GWA3::DungeonTravel;

GWA3_TEST(dungeon_travel_europe_pool_size, {
    GWA3_ASSERT_EQ(GetRandomDistrictPoolSize(RandomDistrictPool::EuropeOnly), 7u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_travel_europe_pool

// --- tests/test_dungeon_travel_global_pool.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_travel_global_pool {

using namespace GWA3::DungeonTravel;

GWA3_TEST(dungeon_travel_global_pool_size, {
    GWA3_ASSERT_EQ(GetRandomDistrictPoolSize(RandomDistrictPool::Global), 11u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_travel_global_pool

// --- tests/test_dungeon_travel_option_order.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_travel_option_order {


using namespace GWA3::DungeonTravel;

GWA3_TEST(dungeon_travel_option_order_matches_autoit_table, {
    const auto& first = GetRandomDistrictOption(RandomDistrictPool::Global, 0u);
    const auto& seventh = GetRandomDistrictOption(RandomDistrictPool::Global, 7u);
    const auto& last = GetRandomDistrictOption(RandomDistrictPool::Global, 10u);

    GWA3_ASSERT_EQ(first.region, 2);
    GWA3_ASSERT_EQ(first.language, 0u);
    GWA3_ASSERT(std::strcmp(first.name, "eu-en") == 0);

    GWA3_ASSERT_EQ(seventh.region, -2);
    GWA3_ASSERT_EQ(seventh.language, 0u);
    GWA3_ASSERT(std::strcmp(seventh.name, "international") == 0);

    GWA3_ASSERT_EQ(last.region, 4);
    GWA3_ASSERT_EQ(last.language, 0u);
    GWA3_ASSERT(std::strcmp(last.name, "asia-jp") == 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_travel_option_order

// --- tests/test_dungeon_vendor_deposit_storage.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_vendor_deposit_storage {

using namespace GWA3::DungeonVendor;
namespace VendorSupport = GWA3::Tests::DungeonVendorSupport;

GWA3_TEST(dungeon_vendor_deposit_items_at_storage_moves_interacts_and_deposits, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(88u, 25.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(8u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 400u, 930u,
                                         GWA3::DungeonItemPolicy::ITEM_TYPE_MATERIAL,
                                         1u, 0u, 0u,
                                         GWA3::DungeonInventory::RARITY_GOLD);

    const int deposited = DepositItemsAtStorage(0.0f, 0.0f, &VendorSupport::VendorMove,
                                                &GWA3::DungeonItemPolicy::ShouldStoreItem,
                                                &VendorSupport::VendorNoWait);
    GWA3_ASSERT_EQ(deposited, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedNpcId(), 88u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMovedItemId(), 400u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMoveBagId(), 8u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_vendor_deposit_storage

// --- tests/test_dungeon_inventory_unclaimed_items.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_inventory_unclaimed_items {

namespace VendorSupport = GWA3::Tests::DungeonVendorSupport;

GWA3_TEST(dungeon_inventory_claims_configured_unclaimed_items, {
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::ItemMgr::SetBagCapacity(7u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(
        7u,
        0u,
        700u,
        GWA3::ItemModelIds::AMPHIBIAN_TONGUE,
        GWA3::DungeonItemPolicy::ITEM_TYPE_MATERIAL,
        3u,
        0u,
        0u,
        GWA3::DungeonInventory::RARITY_WHITE);

    const uint32_t models[] = {GWA3::ItemModelIds::AMPHIBIAN_TONGUE};
    GWA3::DungeonInventory::UnclaimedItemClaimOptions options;
    options.model_ids = models;
    options.model_id_count = 1u;
    options.wait_ms = &VendorSupport::VendorNoWait;

    const auto result = GWA3::DungeonInventory::ClaimUnclaimedItemsByModel(options);
    GWA3_ASSERT(result.accepted);
    GWA3_ASSERT_EQ(result.matching_quantity, 3u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::AcceptUnclaimedCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastAcceptUnclaimedBagIndex(), 7u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_inventory_unclaimed_items

// --- tests/test_dungeon_vendor_open_legacy_packet.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_vendor_open_legacy_packet {

using namespace GWA3::DungeonVendor;
namespace VendorSupport = GWA3::Tests::DungeonVendorSupport;

GWA3_TEST(dungeon_vendor_open_merchant_context_uses_legacy_packet_path, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::TradeMgr::SetMerchantItemCount(1u);

    GWA3_ASSERT(OpenMerchantContextWithLegacyPacket(55u, &VendorSupport::VendorNoWait));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastChangedTargetId(), 55u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::SendPacketCount(), 3u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketHeader(), GWA3::Packets::INTERACT_NPC);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketArg1(), 55u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketArg2(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_vendor_open_legacy_packet

// --- tests/test_dungeon_vendor_open_near_coords.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_vendor_open_near_coords {

using namespace GWA3::DungeonVendor;
namespace VendorSupport = GWA3::Tests::DungeonVendorSupport;

GWA3_TEST(dungeon_vendor_open_merchant_context_near_coords_moves_and_uses_native_interact_path, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(99u, 40.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::TradeMgr::SetMerchantItemCount(1u);

    MerchantContextNearCoordsOptions options;
    options.log_prefix = "Test";

    GWA3_ASSERT(OpenMerchantContextNearCoords(
        0.0f,
        0.0f,
        2500.0f,
        &VendorSupport::VendorMoveResult,
        &VendorSupport::VendorNoWait,
        options));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastChangedTargetId(), 99u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedNpcId(), 99u);
})

GWA3_TEST(dungeon_vendor_open_merchant_context_prefers_merchant_beyond_eight_nearby_npcs, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    for (uint32_t i = 0u; i < 10u; ++i) {
        GWA3::TestStubs::AgentMgr::AddNpc(10u + i, 40.0f + static_cast<float>(i * 20u), 0.0f, 6u, 1.0f);
    }
    GWA3::TestStubs::AgentMgr::AddNpc(42u, 600.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::AgentMgr::SetNpcPlayerNumber(42u, 6438u);
    GWA3::TestStubs::TradeMgr::SetMerchantItemCount(1u);

    MerchantContextNearCoordsOptions options;
    options.log_prefix = "Test";
    options.preferred_player_number = 6438u;

    GWA3_ASSERT(OpenMerchantContextNearCoords(
        0.0f,
        0.0f,
        1000.0f,
        &VendorSupport::VendorMoveResult,
        &VendorSupport::VendorNoWait,
        options));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastChangedTargetId(), 42u);
})

GWA3_TEST(dungeon_vendor_maintenance_context_rejects_non_standard_trader_stock, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(99u, 40.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::TradeMgr::SetMerchantItemCount(8u);

    MerchantContextNearCoordsOptions options;
    options.log_prefix = "Test";
    options.require_standard_merchant_stock = true;

    GWA3_ASSERT(!OpenMerchantContextNearCoords(
        0.0f,
        0.0f,
        2500.0f,
        &VendorSupport::VendorMoveResult,
        &VendorSupport::VendorNoWait,
        options));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastChangedTargetId(), 99u);
})

GWA3_TEST(dungeon_vendor_maintenance_context_accepts_standard_merchant_stock, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(99u, 40.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::TradeMgr::SetMerchantItemModel(
        1u, GWA3::ItemModelIds::IDENTIFICATION_KIT);

    MerchantContextNearCoordsOptions options;
    options.log_prefix = "Test";
    options.require_standard_merchant_stock = true;

    GWA3_ASSERT(OpenMerchantContextNearCoords(
        0.0f,
        0.0f,
        2500.0f,
        &VendorSupport::VendorMoveResult,
        &VendorSupport::VendorNoWait,
        options));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastChangedTargetId(), 99u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_vendor_open_near_coords

// --- tests/test_dungeon_vendor_sell_items.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_vendor_sell_items {

using namespace GWA3::DungeonVendor;
namespace VendorSupport = GWA3::Tests::DungeonVendorSupport;

GWA3_TEST(dungeon_vendor_sell_items_at_merchant_moves_opens_and_sells, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(77u, 25.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::TradeMgr::SetMerchantItemCount(1u);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 300u, 18001u,
                                         27u, 1u, 75u, 0x1u,
                                         GWA3::DungeonInventory::RARITY_BLUE);

    const int sold = SellItemsAtMerchant(0.0f, 0.0f, &VendorSupport::VendorMove,
                                         &GWA3::DungeonItemPolicy::ShouldSellItem,
                                         &VendorSupport::VendorNoWait);
    GWA3_ASSERT_EQ(sold, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::TransactCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::LastTransactItemId(), 300u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_vendor_sell_items

// --- tests/test_dungeon_vendor_wait_context.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_vendor_wait_context {

using namespace GWA3::DungeonVendor;
namespace VendorSupport = GWA3::Tests::DungeonVendorSupport;

GWA3_TEST(dungeon_vendor_wait_for_merchant_context_accepts_visible_frame, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::UIMgr::SetFrameVisible(3613855137u, true);

    GWA3_ASSERT(WaitForMerchantContext(1u, &VendorSupport::VendorNoWait));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_vendor_wait_context
