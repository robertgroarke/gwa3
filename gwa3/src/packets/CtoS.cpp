#include <gwa3/packets/CtoS.h>
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

// Ring buffer for shellcode packet dispatch via RenderHook
// Each slot: [0..31] shellcode, [32..79] packet data (up to 12 dwords)
static constexpr int kPacketSlots = 32;
static constexpr int kPacketSlotSize = 80;
static uintptr_t s_packetShellcodeBase = 0;
static volatile LONG s_packetSlotIndex = 0;

static bool EnsurePacketShellcode() {
    if (s_packetShellcodeBase) return true;
    void* mem = VirtualAlloc(nullptr, kPacketSlots * kPacketSlotSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("CtoS: VirtualAlloc failed for packet shellcode pool");
        return false;
    }
    s_packetShellcodeBase = reinterpret_cast<uintptr_t>(mem);
    return true;
}

static uintptr_t NextPacketSlot() {
    LONG idx = InterlockedIncrement(&s_packetSlotIndex) % kPacketSlots;
    return s_packetShellcodeBase + idx * kPacketSlotSize;
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

    s_initialized = true;
    Log::Info("CtoS: Initialized (PacketSend=0x%08X, PacketLocation=0x%08X)",
              Offsets::PacketSend, s_packetLocation);
    return true;
}

// Core send: builds a packet buffer and calls the game's PacketSend.
// Prefers RenderHook shellcode dispatch; falls back to GameThread::Enqueue.
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

    // Fast path: already on game thread
    if (GameThread::IsOnGameThread()) {
        s_packetSendFn(reinterpret_cast<void*>(s_packetLocation), sizeBytes, data);
        return;
    }

    // Preferred path: RenderHook shellcode (fires every frame, reliable)
    if (RenderHook::IsInitialized() && EnsurePacketShellcode()) {
        uintptr_t slot = NextPacketSlot();
        uintptr_t dataAddr = slot + 32; // packet data starts at offset 32

        // Copy packet data into the slot
        memcpy(reinterpret_cast<void*>(dataAddr), data, sizeBytes);

        // Build shellcode:
        //   push <dataAddr>         ; data pointer
        //   push <sizeBytes>        ; size in bytes
        //   push <packetLocation>   ; PacketLocation ptr
        //   call <PacketSend>       ; __cdecl call
        //   add esp, 12             ; caller cleanup (3 args)
        //   ret
        auto* sc = reinterpret_cast<uint8_t*>(slot);
        size_t i = 0;
        auto emit8 = [&](uint8_t v) { sc[i++] = v; };
        auto emit32 = [&](uint32_t v) { memcpy(sc + i, &v, 4); i += 4; };

        emit8(0x68); emit32(static_cast<uint32_t>(dataAddr));          // push dataAddr
        emit8(0x68); emit32(sizeBytes);                                 // push sizeBytes
        emit8(0x68); emit32(static_cast<uint32_t>(s_packetLocation));  // push packetLocation
        emit8(0xE8);                                                    // call rel32
        int32_t rel = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(s_packetSendFn) - (slot + i + 4));
        emit32(*reinterpret_cast<uint32_t*>(&rel));
        emit8(0x83); emit8(0xC4); emit8(0x0C);                        // add esp, 12
        emit8(0xC3);                                                    // ret

        FlushInstructionCache(GetCurrentProcess(), sc, static_cast<DWORD>(i));

        if (RenderHook::EnqueueCommand(slot)) {
            return;
        }
        Log::Warn("CtoS: RenderHook queue full for header=0x%X, falling back to GameThread", header);
    }

    // Fallback: GameThread::Enqueue (unreliable — fires infrequently)
    uint32_t capturedSize = sizeBytes;
    uint32_t capturedData[12];
    memcpy(capturedData, data, size * sizeof(uint32_t));
    uintptr_t loc = s_packetLocation;
    PacketSendFn fn = s_packetSendFn;

    GameThread::Enqueue([fn, loc, capturedSize, capturedData]() {
        uint32_t mutableData[12];
        memcpy(mutableData, capturedData, capturedSize);
        fn(reinterpret_cast<void*>(loc), capturedSize, mutableData);
    });
}

// --- Type-safe wrappers ---

void MoveToCoord(float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    // AutoIt: SendPacket(0x8, HEADER, x, y) — size=8 bytes (matching GW wire format)
    SendPacket(2, Packets::MOVE_TO_COORD, ix, iy);
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
