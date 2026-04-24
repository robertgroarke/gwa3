#include <gwa3/bot/DungeonVendor.h>

#include <gwa3/bot/DungeonInteractions.h>
#include <gwa3/bot/DungeonNavigation.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>

namespace GWA3::Bot::DungeonVendor {

namespace {

void CallWait(WaitFn wait_fn, uint32_t ms) {
    if (wait_fn) {
        wait_fn(ms);
        return;
    }
    Sleep(ms);
}

void MoveToPoint(float x, float y, float threshold, MoveToPointFn move_to_point) {
    if (move_to_point) {
        move_to_point(x, y, threshold);
        return;
    }
    (void)DungeonNavigation::MoveToAndWait(x, y, threshold);
}

uint32_t MoveToNearestNpc(float anchorX, float anchorY, MoveToPointFn move_to_point,
                          const NpcServiceOptions& options) {
    MoveToPoint(anchorX, anchorY, options.anchor_threshold, move_to_point);

    const uint32_t npcId =
        DungeonInteractions::FindNearestNpc(anchorX, anchorY, options.search_radius);
    if (npcId == 0u) {
        return 0u;
    }

    auto* npc = AgentMgr::GetAgentByID(npcId);
    if (npc) {
        MoveToPoint(npc->x, npc->y, options.agent_threshold, move_to_point);
    }
    return npcId;
}

} // namespace

bool WaitForMerchantContext(uint32_t timeoutMs, WaitFn wait_ms,
                            uint32_t merchant_root_hash, uint32_t poll_ms) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (TradeMgr::GetMerchantItemCount() > 0u) return true;
        if (UIMgr::IsFrameVisible(merchant_root_hash)) return true;
        CallWait(wait_ms, poll_ms);
    }
    return false;
}

bool OpenMerchantContextWithLegacyPacket(uint32_t npcId, WaitFn wait_ms,
                                         const MerchantContextOptions& options) {
    if (npcId == 0u) {
        return false;
    }

    AgentMgr::ChangeTarget(npcId);
    CallWait(wait_ms, options.change_target_delay_ms);
    for (uint32_t attempt = 0u; attempt < options.interact_attempts; ++attempt) {
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        CallWait(wait_ms, options.interact_delay_ms);
    }
    CallWait(wait_ms, options.post_interact_delay_ms);
    return WaitForMerchantContext(options.wait_timeout_ms, wait_ms,
                                  options.merchant_root_hash, options.wait_poll_ms);
}

int SellItemsAtMerchant(float anchorX, float anchorY, MoveToPointFn move_to_point,
                        DungeonItemActions::ItemFilterFn should_sell,
                        WaitFn wait_ms, const SellAtMerchantOptions& options) {
    const uint32_t npcId = MoveToNearestNpc(anchorX, anchorY, move_to_point, options.npc);
    if (npcId == 0u) {
        return 0;
    }
    if (!OpenMerchantContextWithLegacyPacket(npcId, wait_ms, options.merchant)) {
        return 0;
    }
    return DungeonItemActions::SellItems(should_sell, wait_ms, options.sell);
}

int DepositItemsAtStorage(float anchorX, float anchorY, MoveToPointFn move_to_point,
                          DungeonItemActions::ItemFilterFn should_store,
                          WaitFn wait_ms, const DepositAtStorageOptions& options) {
    const uint32_t npcId = MoveToNearestNpc(anchorX, anchorY, move_to_point, options.npc);
    if (npcId == 0u) {
        return 0;
    }

    AgentMgr::InteractNPC(npcId);
    CallWait(wait_ms, options.npc.interact_delay_ms);
    return DungeonItemActions::DepositItemsToStorage(should_store, wait_ms, options.deposit);
}

} // namespace GWA3::Bot::DungeonVendor
