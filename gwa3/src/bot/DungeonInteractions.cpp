#include <gwa3/bot/DungeonInteractions.h>

#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/UIMgr.h>

#include <Windows.h>

namespace GWA3::Bot::DungeonInteractions {

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
           gadgetId == 8141u || gadgetId == 74u || gadgetId == 68u || gadgetId == 9157u;
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

} // namespace GWA3::Bot::DungeonInteractions
