#include "IntegrationTestInternal.h"

#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>

#include <Windows.h>
#include <cstdint>

namespace GWA3::SmokeTest {

InventorySnapshot CaptureInventorySnapshot() {
    InventorySnapshot snapshot{};

    __try {
        Inventory* inv = ItemMgr::GetInventory();
        if (!inv) return snapshot;

        snapshot.goldCharacter = inv->gold_character;
        snapshot.goldStorage = inv->gold_storage;

        for (uint32_t bagIndex = 0; bagIndex < 23; ++bagIndex) {
            Bag* bag = inv->bags[bagIndex];
            if (!bag || !bag->items.buffer) continue;

            for (uint32_t i = 0; i < bag->items.size; ++i) {
                Item* item = bag->items.buffer[i];
                if (!item) continue;

                snapshot.count++;
                snapshot.itemIdSum += item->item_id;
                snapshot.modelIdSum += item->model_id;
                snapshot.quantitySum += item->quantity;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return InventorySnapshot{};
    }

    return snapshot;
}

bool InventoryChangedMeaningfully(const InventorySnapshot& before, const InventorySnapshot& after) {
    return after.goldCharacter != before.goldCharacter ||
           after.goldStorage != before.goldStorage ||
           after.count != before.count ||
           after.itemIdSum != before.itemIdSum ||
           after.modelIdSum != before.modelIdSum ||
           after.quantitySum != before.quantitySum;
}

AgentItem* FindGroundItemByAgentId(uint32_t agentId) {
    if (agentId == 0 || Offsets::AgentBase <= 0x10000) return nullptr;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        if (agentArr <= 0x10000) return nullptr;

        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + agentId * 4);
        if (agentPtr <= 0x10000) return nullptr;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if ((base->type & 0x400) == 0) return nullptr;
        return reinterpret_cast<AgentItem*>(agentPtr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

AgentItem* FindNearbyGroundItem(float maxDistance) {
    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return nullptr;

    if (Offsets::AgentBase <= 0x10000) return nullptr;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr <= 0x10000 || maxAgents == 0) return nullptr;

        const float maxDistSq = maxDistance * maxDistance;
        float bestDistSq = maxDistSq;
        AgentItem* bestItem = nullptr;
        static bool s_loggedItemTypes = false;
        uint32_t typeCounts[8] = {}; // bucket agent types for diagnostics

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;

            auto* base = reinterpret_cast<Agent*>(agentPtr);
            // Log type distribution once
            uint32_t t = base->type;
            if (t == 0xDB) typeCounts[0]++;       // Living
            else if (t == 0x400) typeCounts[1]++;  // Item (classic)
            else if (t & 0x400) typeCounts[2]++;   // Item-like
            else if (t == 0x200) typeCounts[3]++;  // Gadget
            else typeCounts[4]++;                   // Other

            if ((base->type & 0x400) == 0) continue;

            auto* item = reinterpret_cast<AgentItem*>(agentPtr);
            const float distSq = AgentMgr::GetSquaredDistance(myX, myY, item->x, item->y);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestItem = item;
            }
        }

        if (!s_loggedItemTypes || bestItem) {
            Log::Info("[INTG] FindItem types: living(0xDB)=%u item(0x400)=%u itemLike=%u gadget(0x200)=%u other=%u bestItem=%p",
                      typeCounts[0], typeCounts[1], typeCounts[2], typeCounts[3], typeCounts[4], bestItem);
            if (bestItem) {
                Log::Info("[INTG] Found item agent: type=0x%X agent_id=%u item_id=%u pos=(%.0f, %.0f)",
                          reinterpret_cast<Agent*>(bestItem)->type,
                          bestItem->agent_id, bestItem->item_id, bestItem->x, bestItem->y);
            }
            s_loggedItemTypes = true;
        }

        return bestItem;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

} // namespace GWA3::SmokeTest