#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstdarg>
#include <cstring>

namespace GWA3::CtoS {

// Game's PacketSend calling convention (from ASM analysis):
//   void __cdecl PacketSend(void* packetLocationPtr, uint32_t size, uint32_t* data)
// where data starts with the header opcode, followed by params.
using PacketSendFn = void(__cdecl*)(void*, uint32_t, uint32_t*);

static PacketSendFn s_packetSendFn = nullptr;
static uintptr_t s_packetLocation = 0;
static bool s_initialized = false;

// POD task for packet queue
struct PacketTask {
    PacketSendFn fn;
    uintptr_t location;
    uint32_t sizeBytes;
    uint32_t data[12];
};

// Dedicated packet sender thread — calls PacketSend outside any hook context
static CRITICAL_SECTION s_packetCS;
static bool s_packetCSInit = false;
static HANDLE s_packetEvent = nullptr;
static HANDLE s_packetThread = nullptr;
static volatile bool s_packetThreadRunning = false;
static PacketTask s_packetQueue[64];
static uint32_t s_packetQueueCount = 0;

static DWORD WINAPI PacketSenderThread(LPVOID) {
    while (s_packetThreadRunning) {
        WaitForSingleObject(s_packetEvent, 100);
        if (!s_packetThreadRunning) break;

        EnterCriticalSection(&s_packetCS);
        uint32_t count = s_packetQueueCount;
        if (count > 64) count = 64;
        for (uint32_t i = 0; i < count; i++) {
            PacketTask t = s_packetQueue[i]; // copy under lock
            LeaveCriticalSection(&s_packetCS);
            // Re-read PacketLocation
            uintptr_t loc = t.location;
            if (Offsets::PacketLocation) {
                uintptr_t fresh = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
                if (fresh) loc = fresh;
            }
            t.fn(reinterpret_cast<void*>(loc), t.sizeBytes, t.data);
            EnterCriticalSection(&s_packetCS);
        }
        s_packetQueueCount = 0;
        LeaveCriticalSection(&s_packetCS);
    }
    return 0;
}

bool Initialize() {
    if (s_initialized) return true;

    if (!Offsets::PacketSend || !Offsets::PacketLocation) {
        Log::Error("CtoS: PacketSend or PacketLocation offset not resolved");
        return false;
    }

    s_packetSendFn = reinterpret_cast<PacketSendFn>(Offsets::PacketSend);

    // PacketLocation is a pointer — dereference to get the actual location object
    s_packetLocation = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
    if (!s_packetLocation) {
        Log::Error("CtoS: PacketLocation dereference returned null");
        return false;
    }

    // Start dedicated packet sender thread
    if (!s_packetCSInit) {
        InitializeCriticalSection(&s_packetCS);
        s_packetCSInit = true;
    }
    if (!s_packetEvent) {
        s_packetEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    }
    if (!s_packetThread) {
        s_packetThreadRunning = true;
        s_packetThread = CreateThread(nullptr, 0, PacketSenderThread, nullptr, 0, nullptr);
    }

    s_initialized = true;
    Log::Info("CtoS: Initialized (PacketSend=0x%08X, PacketLocation=0x%08X)",
              Offsets::PacketSend, s_packetLocation);
    return true;
}

// Core send: builds a packet buffer and calls the game's PacketSend.
void SendPacket(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacket dropped (header=0x%X) because transport is not initialized", header);
        return;
    }

    uint32_t data[12];
    data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12; i++) {
        data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const uint32_t sizeBytes = size * 4;

    // Re-read PacketLocation every call — it changes after zone transitions
    uintptr_t freshLocation = 0;
    if (Offsets::PacketLocation) {
        freshLocation = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
    }
    if (freshLocation && freshLocation != s_packetLocation) {
        s_packetLocation = freshLocation;
    }

    // Dispatch via PacketSenderThread — calls PacketSendFn directly.
    // (CtoSHook shellcode dispatch was removed: the mid-function detour is a
    //  bisect stub that never processes the queue, so all packets were silently
    //  dropped.  The PacketSenderThread calls the native PacketSendFn safely.)
    if (s_packetCSInit && s_packetEvent) {
        EnterCriticalSection(&s_packetCS);
        if (s_packetQueueCount < 64) {
            PacketTask& t = s_packetQueue[s_packetQueueCount++];
            t.fn = s_packetSendFn;
            t.location = s_packetLocation;
            t.sizeBytes = sizeBytes;
            memcpy(t.data, data, sizeBytes);
        } else {
            Log::Warn("CtoS: PacketSenderThread queue full, dropped header=0x%X", header);
        }
        LeaveCriticalSection(&s_packetCS);
        SetEvent(s_packetEvent);
        return;
    }

    Log::Warn("CtoS: SendPacket dropped header=0x%X — no dispatch", header);
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
