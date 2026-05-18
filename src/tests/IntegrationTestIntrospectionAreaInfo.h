bool TestAreaInfoValidation() {
    IntReport("=== AreaInfo Cross-Validation ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0) {
        IntSkip("AreaInfo validation", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* current = MapMgr::GetAreaInfo(mapId);
    IntReport("  Current map %u AreaInfo: %p", mapId, current);
    IntCheck("AreaInfo for current map exists", current != nullptr);

    if (current) {
        IntReport("  campaign=%u continent=%u region=%u type=%u (%s)",
                  current->campaign,
                  current->continent,
                  current->region,
                  current->type,
                  DescribeMapRegionType(current->type));
        IntReport("  flags=0x%X min_party=%u max_party=%u",
                  current->flags,
                  current->min_party_size,
                  current->max_party_size);
        IntCheck("Campaign plausible (0-4)", current->campaign <= 4);
        IntCheck("Continent plausible (0-3)", current->continent <= 3);
        IntCheck("Max party size > 0", current->max_party_size > 0);
        IntCheck("Max party size <= 12", current->max_party_size <= 12);
    }

    struct KnownMap {
        uint32_t id;
        const char* name;
        uint32_t expectedCampaign;
        uint32_t expectedType;
    };

    const KnownMap knownMaps[] = {
        {MapIds::EMBARK_BEACH, "Embark Beach", 0, static_cast<uint32_t>(MapRegionType::Outpost)},
        {MapIds::GADDS_ENCAMPMENT, "Gadd's Encampment", 4, static_cast<uint32_t>(MapRegionType::Outpost)},
        {MapIds::SPARKFLY_SWAMP, "Sparkfly Swamp", 4, static_cast<uint32_t>(MapRegionType::ExplorableZone)},
        {248, "Great Temple of Balthazar", 0, 13},
    };

    for (const auto& km : knownMaps) {
        const AreaInfo* area = MapMgr::GetAreaInfo(km.id);
        if (!area) {
            IntSkip(km.name, "AreaInfo null");
            continue;
        }
        char checkName[128];
        snprintf(checkName, sizeof(checkName), "%s campaign=%u (expected %u)",
                 km.name, area->campaign, km.expectedCampaign);
        IntCheck(checkName, area->campaign == km.expectedCampaign);
        snprintf(checkName, sizeof(checkName), "%s type=%u (expected %u)",
                 km.name, area->type, km.expectedType);
        IntCheck(checkName, area->type == km.expectedType);
    }

    const uint32_t region = MapMgr::GetRegion();
    const uint32_t district = MapMgr::GetDistrict();
    const uint32_t instanceTime = MapMgr::GetInstanceTime();
    const bool mapLoaded = MapMgr::GetIsMapLoaded();
    IntReport("  Region=%u District=%u InstanceTime=%u MapLoaded=%d",
              region, district, instanceTime, mapLoaded);
    IntCheck("Map is loaded", mapLoaded);
    IntCheck("Region plausible (< 20)", region < 20);

    IntReport("");
    return true;
}
