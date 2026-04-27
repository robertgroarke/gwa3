#include <gwa3/dungeon/DungeonInteractions.h>

#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>

namespace GWA3::DungeonInteractions {

namespace {

constexpr uint32_t kDropBundleActionCode = 0xCDu;

bool TrySnapshotItem(uint32_t agentId, uint32_t& outItemId, float& outX, float& outY) {
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (!agent) {
        return false;
    }
    __try {
        if (agent->type != 0x400u) {
            return false;
        }
        auto* itemAgent = static_cast<AgentItem*>(agent);
        outItemId = itemAgent->item_id;
        outX = itemAgent->x;
        outY = itemAgent->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TrySnapshotItemRecord(uint32_t itemId, uint32_t& outModelId) {
    auto* item = ItemMgr::GetItemById(itemId);
    if (!item) {
        return false;
    }
    __try {
        outModelId = item->model_id;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TrySnapshotAgent(uint32_t agentId, uint32_t& outType, float& outX, float& outY) {
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (!agent) {
        return false;
    }
    __try {
        outType = agent->type;
        outX = agent->x;
        outY = agent->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TrySnapshotGadget(uint32_t agentId, float& outX, float& outY, uint32_t& outGadgetId) {
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (!agent) {
        return false;
    }
    __try {
        if (agent->type != 0x200u) {
            return false;
        }
        auto* gadget = static_cast<AgentGadget*>(agent);
        outX = gadget->x;
        outY = gadget->y;
        outGadgetId = gadget->gadget_id;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TrySnapshotNpc(uint32_t agentId, float& outX, float& outY, uint8_t& outAllegiance, float& outHp) {
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (!agent) {
        return false;
    }
    __try {
        if (agent->type != 0xDBu) {
            return false;
        }
        auto* living = static_cast<AgentLiving*>(agent);
        outX = living->x;
        outY = living->y;
        outAllegiance = living->allegiance;
        outHp = living->hp;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint32_t FindNearestAgentOfType(float x, float y, float maxDist, uint32_t type) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        uint32_t agentType = 0;
        float agentX = 0.0f;
        float agentY = 0.0f;
        if (!TrySnapshotAgent(i, agentType, agentX, agentY)) {
            continue;
        }
        if (agentType != type) {
            continue;
        }
        const float dist = AgentMgr::GetSquaredDistance(x, y, agentX, agentY);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = i;
        }
    }

    return bestId;
}

void CallWait(WaitFn wait_fn, uint32_t ms) {
    if (wait_fn) {
        wait_fn(ms);
        return;
    }
    Sleep(ms);
}

template <typename Predicate>
bool WaitForPredicate(uint32_t timeoutMs, uint32_t pollMs, WaitFn wait_fn, Predicate predicate) {
    if (predicate()) {
        return true;
    }

    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        CallWait(wait_fn, pollMs);
        if (predicate()) {
            return true;
        }
    }
    return false;
}

bool CallMove(MoveToPointResultFn move_to_point, float x, float y, float threshold) {
    if (move_to_point) {
        return move_to_point(x, y, threshold);
    }
    AgentMgr::Move(x, y);
    return false;
}

float MinFloat(float lhs, float rhs) {
    return lhs < rhs ? lhs : rhs;
}

float MaxFloat(float lhs, float rhs) {
    return lhs > rhs ? lhs : rhs;
}

} // namespace

void OpenedChestTracker::ResetForMap(uint32_t mapId) {
    if (map_id_ == mapId) {
        return;
    }
    map_id_ = mapId;
    count_ = 0;
}

bool OpenedChestTracker::IsOpened(uint32_t agentId) const {
    for (std::size_t i = 0; i < count_; ++i) {
        if (opened_ids_[i] == agentId) {
            return true;
        }
    }
    return false;
}

void OpenedChestTracker::MarkOpened(uint32_t agentId) {
    if (IsOpened(agentId) || count_ >= kMaxOpenedChests) {
        return;
    }
    opened_ids_[count_++] = agentId;
}

bool IsChestGadgetId(uint32_t gadgetId) {
    return gadgetId == 6062u || gadgetId == 4579u || gadgetId == 4582u ||
           gadgetId == 8141u || gadgetId == 74u || gadgetId == 68u ||
           gadgetId == 8932u || gadgetId == 9157u;
}

uint32_t FindNearestSignpost(float x, float y, float maxDist) {
    return FindNearestAgentOfType(x, y, maxDist, 0x200u);
}

uint32_t FindNearestChestSignpost(float x, float y, float maxDist) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0u;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        float agentX = 0.0f;
        float agentY = 0.0f;
        uint32_t gadgetId = 0u;
        if (!TrySnapshotGadget(i, agentX, agentY, gadgetId)) {
            continue;
        }
        if (!IsChestGadgetId(gadgetId)) {
            continue;
        }

        const float dist = AgentMgr::GetSquaredDistance(x, y, agentX, agentY);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = i;
        }
    }

    return bestId;
}

uint32_t ResolveGenericChestFallback(uint32_t signpostId,
                                     float chestX,
                                     float chestY,
                                     float searchRadius,
                                     const char* label,
                                     const char* log_prefix) {
    if (signpostId == 0u) {
        return 0u;
    }

    auto* signpost = AgentMgr::GetAgentByID(signpostId);
    if (!signpost) {
        return 0u;
    }

    const float distToChest = AgentMgr::GetDistance(signpost->x, signpost->y, chestX, chestY);
    if (distToChest > MaxFloat(800.0f, searchRadius * 0.5f)) {
        return 0u;
    }

    const char* prefix = log_prefix ? log_prefix : "DungeonInteractions";
    Log::Info("%s: OpenChestAt using generic signpost fallback %s agent=%u gadget=%u distToChest=%.0f",
              prefix,
              label ? label : "",
              signpostId,
              signpost->type == 0x200u ? static_cast<const AgentGadget*>(signpost)->gadget_id : 0u,
              distToChest);
    return signpostId;
}

bool IsChestStillPresentNear(float chestX, float chestY, float searchRadius) {
    const float verifyRadius = MaxFloat(800.0f, MinFloat(searchRadius * 0.5f, 1800.0f));
    const uint32_t targetChestId = FindNearestChestSignpost(chestX, chestY, verifyRadius);
    if (targetChestId != 0u) {
        return true;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return false;
    }

    const uint32_t nearbyChestId = FindNearestChestSignpost(me->x, me->y, MaxFloat(1200.0f, verifyRadius));
    if (nearbyChestId == 0u) {
        return false;
    }

    auto* chestAgent = AgentMgr::GetAgentByID(nearbyChestId);
    if (!chestAgent) {
        return false;
    }

    return AgentMgr::GetDistance(chestAgent->x, chestAgent->y, chestX, chestY) <=
        MaxFloat(800.0f, verifyRadius);
}

uint32_t FindNearestItem(float x, float y, float maxDist) {
    return FindNearestAgentOfType(x, y, maxDist, 0x400u);
}

uint32_t FindNearestItemByModel(float x, float y, float maxDist, uint32_t modelId) {
    if (modelId == 0u) {
        return 0u;
    }

    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0u;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        uint32_t itemId = 0u;
        float agentX = 0.0f;
        float agentY = 0.0f;
        if (!TrySnapshotItem(i, itemId, agentX, agentY)) {
            continue;
        }

        uint32_t itemModelId = 0u;
        if (!TrySnapshotItemRecord(itemId, itemModelId) || itemModelId != modelId) {
            continue;
        }

        const float dist = AgentMgr::GetSquaredDistance(x, y, agentX, agentY);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = i;
        }
    }

    return bestId;
}

uint32_t FindNearestNpc(float x, float y, float maxDist) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        float agentX = 0.0f;
        float agentY = 0.0f;
        uint8_t allegiance = 0u;
        float hp = 0.0f;
        if (!TrySnapshotNpc(i, agentX, agentY, allegiance, hp)) {
            continue;
        }
        if (allegiance != 6u || hp <= 0.0f) {
            continue;
        }
        const float dist = AgentMgr::GetSquaredDistance(x, y, agentX, agentY);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = i;
        }
    }

    return bestId;
}

std::size_t CollectNearestInteractCandidates(float x,
                                             float y,
                                             float signpostRadius,
                                             float npcRadius,
                                             InteractCandidate* outCandidates,
                                             std::size_t capacity) {
    if (outCandidates == nullptr || capacity == 0u) {
        return 0u;
    }

    std::size_t count = 0u;
    auto addCandidate = [&](uint32_t agentId, bool useSignpost) {
        if (agentId == 0u || count >= capacity) {
            return;
        }
        for (std::size_t i = 0u; i < count; ++i) {
            if (outCandidates[i].agent_id == agentId) {
                return;
            }
        }
        auto* agent = AgentMgr::GetAgentByID(agentId);
        if (agent == nullptr) {
            return;
        }

        InteractCandidate candidate;
        candidate.agent_id = agentId;
        candidate.use_signpost = useSignpost;
        candidate.x = agent->x;
        candidate.y = agent->y;
        candidate.dist_to_anchor = AgentMgr::GetDistance(x, y, agent->x, agent->y);
        outCandidates[count++] = candidate;
    };

    addCandidate(FindNearestSignpost(x, y, signpostRadius), true);
    addCandidate(FindNearestNpc(x, y, npcRadius), false);

    if (count > 1u && outCandidates[1].dist_to_anchor < outCandidates[0].dist_to_anchor) {
        const auto tmp = outCandidates[0];
        outCandidates[0] = outCandidates[1];
        outCandidates[1] = tmp;
    }
    return count;
}

CandidateDialogResult InteractCandidateAndSendDialog(
    const InteractCandidate& candidate,
    const CandidateDialogOptions& options) {
    CandidateDialogResult result;
    if (candidate.agent_id == 0u || options.dialog_id == 0u || options.interact_attempts <= 0) {
        return result;
    }

    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "DungeonInteractions";
    auto isStopped = [&options]() {
        return options.stop_condition != nullptr && options.stop_condition();
    };
    auto dialogReady = [&candidate]() {
        if (!DialogMgr::IsDialogOpen() || DialogMgr::GetButtonCount() == 0u) {
            return false;
        }
        const uint32_t sender = DialogMgr::GetDialogSenderAgentId();
        if (!candidate.use_signpost) {
            return sender == candidate.agent_id;
        }
        return sender == 0u || sender == candidate.agent_id || AgentMgr::GetTargetId() == candidate.agent_id;
    };

    AgentMgr::CancelAction();
    CallWait(options.wait_ms, options.prepare_wait_ms);
    DialogMgr::ClearDialog();
    DialogMgr::ResetHookState();
    DialogMgr::ResetRecentUITrace();

    for (int targetAttempt = 0; targetAttempt < options.target_attempts; ++targetAttempt) {
        AgentMgr::ChangeTarget(candidate.agent_id);
        if (WaitForPredicate(options.target_wait_ms, options.target_poll_ms, options.wait_ms, [&candidate]() {
                return AgentMgr::GetTargetId() == candidate.agent_id;
            })) {
            break;
        }
    }

    for (int attempt = 1; attempt <= options.interact_attempts && !isStopped(); ++attempt) {
        if (!dialogReady()) {
            if (candidate.use_signpost) {
                AgentMgr::InteractSignpost(candidate.agent_id);
            } else {
                CtoS::SendPacketDirect(3, Packets::INTERACT_NPC, candidate.agent_id, 0u);
            }
            result.interacted = true;
            result.interact_attempts = attempt;

            const bool ready = WaitForPredicate(options.dialog_wait_ms, options.dialog_poll_ms, options.wait_ms, dialogReady);
            Log::Info("%s: candidate=%u kind=%s attempt=%d sender=%u buttons=%u dialogOpen=%d ready=%d lastDialog=0x%X target=%u",
                      prefix,
                      static_cast<unsigned>(options.candidate_index),
                      candidate.use_signpost ? "signpost" : "npc",
                      attempt,
                      DialogMgr::GetDialogSenderAgentId(),
                      DialogMgr::GetButtonCount(),
                      DialogMgr::IsDialogOpen() ? 1 : 0,
                      ready ? 1 : 0,
                      DialogMgr::GetLastDialogId(),
                      AgentMgr::GetTargetId());
            if (!ready &&
                DialogMgr::GetDialogSenderAgentId() != 0u &&
                DialogMgr::GetDialogSenderAgentId() != candidate.agent_id) {
                Log::Info("%s: clearing stale dialog sender=%u before retry",
                          prefix,
                          DialogMgr::GetDialogSenderAgentId());
                DialogMgr::ClearDialog();
                DialogMgr::ResetHookState();
                DialogMgr::ResetRecentUITrace();
                CallWait(options.wait_ms, options.prepare_wait_ms);
            }
        }

        if (!dialogReady()) {
            continue;
        }

        QuestMgr::Dialog(options.dialog_id);
        result.dialog_sent = true;
        Log::Info("%s: accept candidate=%u kind=%s attempt=%d sender=%u buttons=%u dialogOpen=%d lastDialog=0x%X target=%u",
                  prefix,
                  static_cast<unsigned>(options.candidate_index),
                  candidate.use_signpost ? "signpost" : "npc",
                  attempt,
                  DialogMgr::GetDialogSenderAgentId(),
                  DialogMgr::GetButtonCount(),
                  DialogMgr::IsDialogOpen() ? 1 : 0,
                  DialogMgr::GetLastDialogId(),
                  AgentMgr::GetTargetId());
        CallWait(options.wait_ms, options.post_dialog_wait_ms);
    }

    result.confirmed = isStopped();
    return result;
}

DirectNpcInteractResult PulseDirectNpcInteract(uint32_t npcId, const DirectNpcInteractOptions& options) {
    DirectNpcInteractResult result;
    if (npcId == 0u || options.passes <= 0) {
        return result;
    }

    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "DungeonInteractions";
    const char* label = options.label != nullptr ? options.label : "DirectNpcInteract";

    if (options.clear_dialog) {
        DialogMgr::ClearDialog();
    }
    if (options.reset_hook_state) {
        DialogMgr::ResetHookState();
    }

    AgentMgr::ChangeTarget(npcId);
    CallWait(options.wait_ms, options.target_wait_ms);

    auto snapshot = [&]() {
        auto* me = AgentMgr::GetMyAgent();
        auto* npc = AgentMgr::GetAgentByID(npcId);
        result.final_distance = (me != nullptr && npc != nullptr)
            ? AgentMgr::GetDistance(me->x, me->y, npc->x, npc->y)
            : -1.0f;
        result.dialog_open = DialogMgr::IsDialogOpen();
        result.dialog_sender = DialogMgr::GetDialogSenderAgentId();
        result.button_count = DialogMgr::GetButtonCount();
        result.last_dialog_id = DialogMgr::GetLastDialogId();
        result.target_id = AgentMgr::GetTargetId();
        return me;
    };

    for (int pass = 1; pass <= options.passes; ++pass) {
        Log::Info("%s: %s SendPacketDirect(GoNPC) pass %d agent=%u",
                  prefix,
                  label,
                  pass,
                  npcId);
        CtoS::SendPacketDirect(3, Packets::INTERACT_NPC, npcId, 0u);
        result.interacted = true;
        result.passes = pass;
        CallWait(options.wait_ms, options.pass_wait_ms);

        auto* me = snapshot();
        Log::Info("%s: %s GoNPC pass %d: pos=(%.0f, %.0f) dist=%.0f dialogOpen=%d buttons=%u sender=%u lastDialog=0x%X target=%u",
                  prefix,
                  label,
                  pass,
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f,
                  result.final_distance,
                  result.dialog_open ? 1 : 0,
                  result.button_count,
                  result.dialog_sender,
                  result.last_dialog_id,
                  result.target_id);

        if (options.stop_condition != nullptr && options.stop_condition(npcId, options.stop_context)) {
            result.stopped = true;
            break;
        }
    }

    snapshot();
    return result;
}

std::size_t CollectNearestNpcs(float x, float y, float maxDist, uint32_t* outIds, std::size_t capacity) {
    if (outIds == nullptr || capacity == 0u) {
        return 0u;
    }

    struct Candidate {
        uint32_t id = 0u;
        float dist_sq = 0.0f;
    };

    Candidate candidates[16] = {};
    std::size_t count = 0u;
    const float maxDistSq = maxDist * maxDist;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();

    for (uint32_t i = 1; i < maxAgents; ++i) {
        float agentX = 0.0f;
        float agentY = 0.0f;
        uint8_t allegiance = 0u;
        float hp = 0.0f;
        if (!TrySnapshotNpc(i, agentX, agentY, allegiance, hp)) {
            continue;
        }
        if (allegiance != 6u || hp <= 0.0f) {
            continue;
        }

        const float distSq = AgentMgr::GetSquaredDistance(x, y, agentX, agentY);
        if (distSq > maxDistSq) {
            continue;
        }
        if (count >= _countof(candidates)) {
            break;
        }

        candidates[count].id = i;
        candidates[count].dist_sq = distSq;
        ++count;
    }

    for (std::size_t i = 0; i < count; ++i) {
        std::size_t best = i;
        for (std::size_t j = i + 1; j < count; ++j) {
            if (candidates[j].dist_sq < candidates[best].dist_sq) {
                best = j;
            }
        }
        if (best != i) {
            const Candidate tmp = candidates[i];
            candidates[i] = candidates[best];
            candidates[best] = tmp;
        }
    }

    const std::size_t emit = count < capacity ? count : capacity;
    for (std::size_t i = 0; i < emit; ++i) {
        outIds[i] = candidates[i].id;
    }
    return emit;
}

uint32_t GetHeldBundleItemId() {
    const auto* inventory = ItemMgr::GetInventory();
    if (!inventory || !inventory->bundle) {
        return 0u;
    }
    return inventory->bundle->item_id;
}

bool DropHeldBundle(bool assumeBundleHeld, bool allowInventoryFallback) {
    const uint32_t bundleItemId = GetHeldBundleItemId();
    Log::Info("DungeonInteractions: DropHeldBundle begin heldItem=%u assume=%d allowInventoryFallback=%d",
              bundleItemId,
              assumeBundleHeld ? 1 : 0,
              allowInventoryFallback ? 1 : 0);
    if (bundleItemId == 0u && !assumeBundleHeld) {
        Log::Info("DungeonInteractions: DropHeldBundle aborted no held bundle");
        return false;
    }

    const bool actionKeyQueued = UIMgr::ActionKeyDown(kDropBundleActionCode);
    Log::Info("DungeonInteractions: DropHeldBundle actionQueued=%d action=0x%X heldItem=%u path=action-key-down",
              actionKeyQueued ? 1 : 0,
              kDropBundleActionCode,
              bundleItemId);
    if (actionKeyQueued) {
        return true;
    }

    const bool directActionQueued = UIMgr::PerformUiActionDirect(kDropBundleActionCode);
    Log::Info("DungeonInteractions: DropHeldBundle actionQueued=%d action=0x%X heldItem=%u path=perform-ui-action-direct",
              directActionQueued ? 1 : 0,
              kDropBundleActionCode,
              bundleItemId);
    if (directActionQueued) {
        return true;
    }

    const bool queuedPerformAction = UIMgr::PerformUiAction(kDropBundleActionCode);
    Log::Info("DungeonInteractions: DropHeldBundle actionQueued=%d action=0x%X heldItem=%u path=perform-ui-action",
              queuedPerformAction ? 1 : 0,
              kDropBundleActionCode,
              bundleItemId);
    if (queuedPerformAction) {
        return true;
    }

    if (!allowInventoryFallback || bundleItemId == 0u) {
        Log::Info("DungeonInteractions: DropHeldBundle no inventory fallback heldItem=%u", bundleItemId);
        return false;
    }

    Log::Info("DungeonInteractions: DropHeldBundle inventory fallback item=%u", bundleItemId);
    ItemMgr::DropItem(bundleItemId);
    return true;
}

bool OpenDoorAt(float doorX,
                float doorY,
                float checkpointX,
                float checkpointY,
                MoveToPointResultFn move_to_point,
                WaitFn wait_ms,
                const DoorOpenOptions& options) {
    const char* prefix = options.log_prefix ? options.log_prefix : "DungeonInteractions";
    Log::Info("%s: OpenDungeonDoor start target=(%.0f, %.0f)", prefix, doorX, doorY);
    if (options.signpost_scan_log) {
        options.signpost_scan_log(
            doorX,
            doorY,
            options.signpost_search_radius,
            "OpenDungeonDoor signpost scan",
            false);
    }

    const uint32_t doorId = FindNearestSignpost(doorX, doorY, options.signpost_search_radius);
    if (doorId != 0u && options.agent_log) {
        options.agent_log("OpenDungeonDoor signpost", doorId);
    }

    const auto clearTarget = [&]() {
        AgentMgr::CancelAction();
        CallWait(wait_ms, options.clear_cancel_delay_ms);
        AgentMgr::ChangeTarget(0u);
        CallWait(wait_ms, options.clear_target_delay_ms);
    };

    const auto interactBurst = [&](const char* label) {
        for (int press = 0; press < options.burst_presses; ++press) {
            bool usedSignpost = false;
            if (doorId != 0u) {
                AgentMgr::InteractSignpost(doorId);
                usedSignpost = true;
            } else {
                const bool queued = AgentMgr::ActionInteract();
                Log::Info("%s: OpenDungeonDoor %s ActionInteract queued=%d press=%d target=%u",
                          prefix,
                          label,
                          queued ? 1 : 0,
                          press + 1,
                          AgentMgr::GetTargetId());
            }

            if (usedSignpost) {
                Log::Info("%s: OpenDungeonDoor %s InteractSignpost door=%u press=%d",
                          prefix,
                          label,
                          doorId,
                          press + 1);
            }
            CallWait(wait_ms, options.press_delay_ms);
        }
    };

    clearTarget();
    (void)CallMove(move_to_point, doorX, doorY, options.move_threshold);
    CallWait(wait_ms, options.settle_after_move_ms);
    clearTarget();
    interactBurst("burst1");
    CallWait(wait_ms, options.first_burst_settle_ms);

    (void)CallMove(move_to_point, doorX, doorY, options.move_threshold);
    CallWait(wait_ms, options.settle_after_move_ms);
    clearTarget();
    interactBurst("burst2");
    CallWait(wait_ms, options.second_burst_settle_ms);
    interactBurst("burst3");

    const bool pushedThrough = CallMove(move_to_point, checkpointX, checkpointY, options.move_threshold);
    auto* me = AgentMgr::GetMyAgent();
    if (!pushedThrough && options.failure_probe) {
        options.failure_probe("OpenDungeonDoor unresolved-key-scan", doorX, doorY, 6000.0f);
    }
    Log::Info("%s: OpenDungeonDoor end pushedThrough=%d player=(%.0f, %.0f) distToCheckpoint=%.0f",
              prefix,
              pushedThrough ? 1 : 0,
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              me ? AgentMgr::GetDistance(me->x, me->y, checkpointX, checkpointY) : -1.0f);
    CallWait(wait_ms, options.post_open_delay_ms);
    return pushedThrough;
}

} // namespace GWA3::DungeonInteractions
