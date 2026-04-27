#pragma once

#include <cstddef>
#include <cstdint>

namespace GWA3::DungeonTravel {

enum class RandomDistrictPool : uint8_t {
    EuropeOnly,
    EuropePlusInternational,
    Global,
};

struct DistrictRegionLanguage {
    int32_t region;
    uint32_t language;
    const char* name;
};

struct RandomDistrictTravelPlan {
    uint32_t map_id = 0;
    int32_t region = 0;
    uint32_t district = 0;
    uint32_t language = 0;
    const char* district_name = "";
};

constexpr uint32_t kDefaultDistrict = 0u;

std::size_t GetRandomDistrictPoolSize(RandomDistrictPool pool);
const DistrictRegionLanguage& GetRandomDistrictOption(RandomDistrictPool pool, std::size_t index);
RandomDistrictTravelPlan BuildRandomDistrictTravelPlan(
    uint32_t mapId,
    RandomDistrictPool pool,
    uint32_t randomValue);
void TravelUsingPlan(const RandomDistrictTravelPlan& plan);
RandomDistrictTravelPlan TravelRandomDistrict(
    uint32_t mapId,
    RandomDistrictPool pool = RandomDistrictPool::EuropePlusInternational);
RandomDistrictTravelPlan TravelRandomDistrict(
    uint32_t mapId,
    RandomDistrictPool pool,
    uint32_t randomValue);

} // namespace GWA3::DungeonTravel

