#pragma once

#include <cstddef>
#include <cstdint>

namespace GWA3::Bot::DungeonInteractions {

class OpenedChestTracker {
public:
    void ResetForMap(uint32_t mapId);
    bool IsOpened(uint32_t agentId) const;
    void MarkOpened(uint32_t agentId);
    uint32_t map_id() const { return map_id_; }
    std::size_t count() const { return count_; }

private:
    static constexpr std::size_t kMaxOpenedChests = 64;
    uint32_t map_id_ = 0;
    uint32_t opened_ids_[kMaxOpenedChests] = {};
    std::size_t count_ = 0;
};

bool IsChestGadgetId(uint32_t gadgetId);
uint32_t FindNearestSignpost(float x, float y, float maxDist);
uint32_t FindNearestChestSignpost(float x, float y, float maxDist);
uint32_t FindNearestItem(float x, float y, float maxDist);
uint32_t FindNearestItemByModel(float x, float y, float maxDist, uint32_t modelId);
uint32_t FindNearestNpc(float x, float y, float maxDist);
std::size_t CollectNearestNpcs(float x, float y, float maxDist, uint32_t* outIds, std::size_t capacity);
uint32_t GetHeldBundleItemId();
bool DropHeldBundle(bool assumeBundleHeld = false, bool allowInventoryFallback = true);

} // namespace GWA3::Bot::DungeonInteractions
