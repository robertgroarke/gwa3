#include <gwa3/bot/DungeonTravel.h>
#include <gwa3/testing/TestFramework.h>

#include <cstring>

using namespace GWA3::Bot::DungeonTravel;

GWA3_TEST(dungeon_travel_europe_pool_size, {
    GWA3_ASSERT_EQ(GetRandomDistrictPoolSize(RandomDistrictPool::EuropeOnly), 7u);
})

GWA3_TEST(dungeon_travel_europe_plus_international_pool_size, {
    GWA3_ASSERT_EQ(GetRandomDistrictPoolSize(RandomDistrictPool::EuropePlusInternational), 8u);
})

GWA3_TEST(dungeon_travel_global_pool_size, {
    GWA3_ASSERT_EQ(GetRandomDistrictPoolSize(RandomDistrictPool::Global), 11u);
})

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

GWA3_TEST(dungeon_travel_build_plan_uses_default_district_zero, {
    const auto plan = BuildRandomDistrictTravelPlan(857u, RandomDistrictPool::EuropePlusInternational, 3u);

    GWA3_ASSERT_EQ(plan.map_id, 857u);
    GWA3_ASSERT_EQ(plan.region, 2);
    GWA3_ASSERT_EQ(plan.district, 0u);
    GWA3_ASSERT_EQ(plan.language, 4u);
    GWA3_ASSERT(std::strcmp(plan.district_name, "eu-it") == 0);
})

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

