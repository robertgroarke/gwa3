#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstdarg>
#include <cstring>

namespace GWA3::CtoS {

// Game's PacketSend calling convention:
//   void __cdecl PacketSend(void* packetLocationPtr, uint32_t size, uint32_t* data)
using PacketSendFn = void(__cdecl*)(void*, uint32_t, uint32_t*);

static PacketSendFn s_packetSendFn = nullptr;
static uintptr_t s_packetLocation = 0;
static bool s_initialized = false;

// POD task for GameThread post-queue dispatch (fits InlineTask's 64-byte storage)
struct PacketTask {
    PacketSendFn fn;       // 4
    uintptr_t location;    // 4
    uint32_t sizeBytes;    // 4
    uint32_t data[12];     // 48  => total 60
};
static_assert(sizeof(PacketTask) <= 64, "PacketTask exceeds InlineTask storage");

static void PacketTaskInvoker(void* storage) {
    auto& t = *reinterpret_cast<PacketTask*>(storage);
    // Re-read PacketLocation — it changes after zone transitions
    uintptr_t loc = t.location;
    if (Offsets::PacketLocation) {
        uintptr_t fresh = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
        if (fresh) loc = fresh;
    }
    t.fn(reinterpret_cast<void*>(loc), t.sizeBytes, t.data);
}

bool Initialize() {
    if (s_initialized) return true;

    if (!Offsets::PacketSend || !Offsets::PacketLocation) {
        Log::Error("CtoS: PacketSend or PacketLocation offset not resolved");
        return false;
    }

    s_packetSendFn = reinterpret_cast<PacketSendFn>(Offsets::PacketSend);

    s_packetLocation = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
    if (!s_packetLocation) {
        Log::Error("CtoS: PacketLocation dereference returned null");
        return false;
    }

    s_initialized = true;
    Log::Info("CtoS: Initialized (PacketSend=0x%08X, PacketLocation=0x%08X)",
              Offsets::PacketSend, s_packetLocation);
    return true;
}

// Dispatch via GameThread post-queue (one packet per frame).
// The VEH INT3 hook's re-entrancy guard makes this safe: if PacketSend
// internally hits the hook address, the handler just emulates the original
// instruction without dispatching queues.
void SendPacket(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacket dropped (header=0x%X) — not initialized", header);
        return;
    }

    PacketTask task = {};
    task.fn = s_packetSendFn;
    task.location = s_packetLocation;
    task.sizeBytes = size * 4;
    task.data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12; i++) {
        task.data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    // Re-read PacketLocation
    if (Offsets::PacketLocation) {
        uintptr_t fresh = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
        if (fresh) task.location = fresh;
    }

    // Dispatch via GameThread post-queue — the VEH INT3 handler will
    // call PacketSend on the game thread during the next frame.
    if (GameThread::IsInitialized()) {
        GameThread::EnqueuePostRaw(PacketTaskInvoker, &task, sizeof(task));
        return;
    }

    Log::Warn("CtoS: SendPacket dropped header=0x%X — GameThread not initialized", header);
}

// --- Type-safe wrappers ---

void MoveToCoord(float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(3, Packets::MOVE_TO_COORD, ix, iy);
}

void Dialog(uint32_t dialogId) {
    SendPacket(2, Packets::DIALOG_SEND, dialogId);
}

void ChangeTarget(uint32_t agentId) {
    SendPacket(2, Packets::TARGET_AGENT, agentId);
}

void AttackAgent(uint32_t agentId) {
    SendPacket(2, Packets::ATTACK_AGENT, agentId);
}

void CancelAction() {
    SendPacket(1, Packets::ACTION_CANCEL);
}

void MapTravel(uint32_t mapId, uint32_t region, uint32_t district, uint32_t language) {
    SendPacket(6, Packets::MAP_TRAVEL, mapId, region, district, language, 0u);
}

void HeroAdd(uint32_t heroId) {
    SendPacket(2, Packets::HERO_ADD, heroId);
}

void HeroKick(uint32_t heroId) {
    SendPacket(2, Packets::HERO_KICK, heroId);
}

void HeroBehavior(uint32_t heroIndex, uint32_t behavior) {
    SendPacket(3, Packets::HERO_BEHAVIOR, heroIndex, behavior);
}

void HeroFlagSingle(uint32_t heroIndex, float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(4, Packets::HERO_FLAG_SINGLE, heroIndex, ix, iy);
}

void HeroFlagAll(float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(3, Packets::HERO_FLAG_ALL, ix, iy);
}

void UseItem(uint32_t itemId) {
    SendPacket(2, Packets::ITEM_USE, itemId);
}

void EquipItem(uint32_t itemId) {
    SendPacket(2, Packets::ITEM_EQUIP, itemId);
}

void DropItem(uint32_t itemId) {
    SendPacket(2, Packets::DROP_ITEM, itemId);
}

void PickUpItem(uint32_t itemAgentId) {
    SendPacket(3, Packets::ITEM_INTERACT, itemAgentId, 0u);
}

void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot) {
    SendPacket(4, Packets::ITEM_MOVE, itemId, bagId, slot);
}

void QuestAbandon(uint32_t questId) {
    SendPacket(2, Packets::QUEST_ABANDON, questId);
}

void QuestSetActive(uint32_t questId) {
    SendPacket(2, Packets::QUEST_SET_ACTIVE, questId);
}

void UseSkill(uint32_t skillSlot, uint32_t targetAgentId, uint32_t callTarget) {
    SendPacket(4, Packets::USE_SKILL, skillSlot, targetAgentId, callTarget);
}

void SwitchWeaponSet(uint32_t setIndex) {
    SendPacket(2, Packets::SWITCH_SET, setIndex);
}

void TradePlayer(uint32_t agentId) {
    SendPacket(2, Packets::TRADE_INITIATE, agentId);
}

void TradeCancel() {
    SendPacket(1, Packets::TRADE_CANCEL);
}

void TradeAccept() {
    SendPacket(1, Packets::TRADE_ACCEPT);
}

} // namespace GWA3::CtoS
