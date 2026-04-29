#pragma once

#include <cstddef>
#include <cstdint>

namespace GWA3::DungeonInteractions {

using WaitFn = void(*)(uint32_t ms);
using MoveToPointResultFn = bool(*)(float x, float y, float threshold);
using AgentLogFn = void(*)(const char* label, uint32_t agentId);
using SignpostScanLogFn = void(*)(float x, float y, float maxDist, const char* label, bool chestOnly);
using FailureProbeFn = void(*)(const char* label, float x, float y, float maxRange);
using BoolFn = bool(*)();
using DirectNpcInteractStopFn = bool(*)(uint32_t npcId, void* context);

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

struct DoorOpenOptions {
    float signpost_search_radius = 1500.0f;
    float move_threshold = 200.0f;
    uint32_t settle_after_move_ms = 1000u;
    uint32_t first_burst_settle_ms = 500u;
    uint32_t second_burst_settle_ms = 1000u;
    uint32_t press_delay_ms = 150u;
    uint32_t clear_cancel_delay_ms = 100u;
    uint32_t clear_target_delay_ms = 150u;
    uint32_t post_open_delay_ms = 500u;
    int burst_presses = 2;
    const char* log_prefix = nullptr;
    SignpostScanLogFn signpost_scan_log = nullptr;
    AgentLogFn agent_log = nullptr;
    FailureProbeFn failure_probe = nullptr;
};

struct InteractCandidate {
    uint32_t agent_id = 0u;
    bool use_signpost = false;
    float x = 0.0f;
    float y = 0.0f;
    float dist_to_anchor = 0.0f;
};

struct CandidateDialogOptions {
    uint32_t dialog_id = 0u;
    uint32_t prepare_wait_ms = 250u;
    uint32_t target_wait_ms = 600u;
    uint32_t target_poll_ms = 50u;
    uint32_t dialog_wait_ms = 1500u;
    uint32_t dialog_poll_ms = 50u;
    uint32_t post_dialog_wait_ms = 1200u;
    int target_attempts = 3;
    int interact_attempts = 3;
    std::size_t candidate_index = 0u;
    const char* log_prefix = nullptr;
    WaitFn wait_ms = nullptr;
    BoolFn stop_condition = nullptr;
};

struct CandidateDialogResult {
    bool interacted = false;
    bool dialog_sent = false;
    bool confirmed = false;
    int interact_attempts = 0;
};

struct DirectNpcInteractOptions {
    uint32_t target_wait_ms = 500u;
    uint32_t pass_wait_ms = 1500u;
    int passes = 3;
    bool clear_dialog = false;
    bool reset_hook_state = false;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
    WaitFn wait_ms = nullptr;
    DirectNpcInteractStopFn stop_condition = nullptr;
    void* stop_context = nullptr;
};

struct DirectNpcInteractResult {
    bool interacted = false;
    bool stopped = false;
    int passes = 0;
    bool dialog_open = false;
    uint32_t dialog_sender = 0u;
    uint32_t button_count = 0u;
    uint32_t last_dialog_id = 0u;
    uint32_t target_id = 0u;
    float final_distance = -1.0f;
};

bool IsChestGadgetId(uint32_t gadgetId);
uint32_t FindNearestSignpost(float x, float y, float maxDist);
uint32_t FindNearestChestSignpost(float x, float y, float maxDist);
uint32_t ResolveGenericChestFallback(uint32_t signpostId,
                                     float chestX,
                                     float chestY,
                                     float searchRadius,
                                     const char* label = nullptr,
                                     const char* log_prefix = nullptr);
bool IsChestStillPresentNear(float chestX, float chestY, float searchRadius);
uint32_t FindNearestItem(float x, float y, float maxDist);
uint32_t FindNearestItemByModel(float x, float y, float maxDist, uint32_t modelId);
uint32_t FindNearestNpc(float x, float y, float maxDist);
std::size_t CollectNearestNpcs(float x, float y, float maxDist, uint32_t* outIds, std::size_t capacity);
std::size_t CollectNearestInteractCandidates(float x,
                                             float y,
                                             float signpostRadius,
                                             float npcRadius,
                                             InteractCandidate* outCandidates,
                                             std::size_t capacity);
CandidateDialogResult InteractCandidateAndSendDialog(
    const InteractCandidate& candidate,
    const CandidateDialogOptions& options);
DirectNpcInteractResult PulseDirectNpcInteract(uint32_t npcId, const DirectNpcInteractOptions& options = {});
uint32_t GetHeldBundleItemId();
bool DropHeldBundle(bool assumeBundleHeld = false, bool allowInventoryFallback = true);
void ResetOpenedChestTrackerForCurrentMap(OpenedChestTracker& tracker,
                                          const char* reason = nullptr,
                                          const char* log_prefix = nullptr);
bool OpenDoorAt(float doorX,
                float doorY,
                float checkpointX,
                float checkpointY,
                MoveToPointResultFn move_to_point,
                WaitFn wait_ms = nullptr,
                const DoorOpenOptions& options = {});
bool OpenDoorAtWithProbe(float doorX,
                         float doorY,
                         float probeX,
                         float probeY,
                         MoveToPointResultFn move_to_point,
                         WaitFn wait_ms = nullptr,
                         const DoorOpenOptions& options = {});

} // namespace GWA3::DungeonInteractions
