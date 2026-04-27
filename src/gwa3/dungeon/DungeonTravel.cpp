#include <gwa3/dungeon/DungeonTravel.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>

namespace GWA3::DungeonTravel {

namespace {

constexpr DistrictRegionLanguage kDistrictOptions[] = {
    {2, 0u, "eu-en"},
    {2, 2u, "eu-fr"},
    {2, 3u, "eu-de"},
    {2, 4u, "eu-it"},
    {2, 5u, "eu-es"},
    {2, 9u, "eu-pl"},
    {2, 10u, "eu-ru"},
    {-2, 0u, "international"},
    {1, 0u, "america"},
    {3, 0u, "asia-ko"},
    {4, 0u, "asia-jp"},
};

constexpr std::size_t kDistrictOptionCount =
    sizeof(kDistrictOptions) / sizeof(kDistrictOptions[0]);

std::size_t PoolSize(RandomDistrictPool pool) {
    switch (pool) {
    case RandomDistrictPool::EuropeOnly:
        return 7u;
    case RandomDistrictPool::EuropePlusInternational:
        return 8u;
    case RandomDistrictPool::Global:
        return kDistrictOptionCount;
    default:
        return 8u;
    }
}

uint32_t NextPseudoRandomValue() {
    static uint32_t s_state = 0u;
    if (s_state == 0u) {
        s_state = GetTickCount();
        if (s_state == 0u) {
            s_state = 0xA341316Cu;
        }
    }

    uint32_t x = s_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_state = x;
    return s_state;
}

} // namespace

std::size_t GetRandomDistrictPoolSize(RandomDistrictPool pool) {
    return PoolSize(pool);
}

const DistrictRegionLanguage& GetRandomDistrictOption(RandomDistrictPool pool, std::size_t index) {
    const std::size_t poolSize = PoolSize(pool);
    const std::size_t normalizedIndex = poolSize == 0u ? 0u : (index % poolSize);
    return kDistrictOptions[normalizedIndex];
}

RandomDistrictTravelPlan BuildRandomDistrictTravelPlan(
    uint32_t mapId,
    RandomDistrictPool pool,
    uint32_t randomValue) {
    const DistrictRegionLanguage& option =
        GetRandomDistrictOption(pool, static_cast<std::size_t>(randomValue));
    RandomDistrictTravelPlan plan;
    plan.map_id = mapId;
    plan.region = option.region;
    plan.district = kDefaultDistrict;
    plan.language = option.language;
    plan.district_name = option.name;
    return plan;
}

void TravelUsingPlan(const RandomDistrictTravelPlan& plan) {
    MapMgr::Travel(
        plan.map_id,
        static_cast<uint32_t>(plan.region),
        plan.district,
        plan.language);
}

RandomDistrictTravelPlan TravelRandomDistrict(uint32_t mapId, RandomDistrictPool pool) {
    return TravelRandomDistrict(mapId, pool, NextPseudoRandomValue());
}

RandomDistrictTravelPlan TravelRandomDistrict(
    uint32_t mapId,
    RandomDistrictPool pool,
    uint32_t randomValue) {
    RandomDistrictTravelPlan plan = BuildRandomDistrictTravelPlan(mapId, pool, randomValue);
    TravelUsingPlan(plan);
    return plan;
}

} // namespace GWA3::DungeonTravel

