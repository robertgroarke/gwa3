#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
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

// ===== Engine inline hook for CtoS dispatch =====
// Hooks at Offsets::Engine (0x00C93C91) â€” a DIFFERENT function from the
// Render/FrApi function (0x00AE3D10) that GameThread hooks.
// This avoids the lock-ordering deadlock that occurs when PacketSend is
// called from within the Render function.
// Architecture is currently the sender-thread/game-thread dispatch path used by gwa3.

static constexpr uint32_t kQueueSize = 256;
static uintptr_t s_queue[kQueueSize] = {};
static volatile LONG s_queueCounter = 0;
static volatile LONG s_savedCommand = 0;
static volatile LONG s_savedESP = 0;
static volatile LONG s_heartbeat = 0;

static uintptr_t s_engineHookAddr = 0;
static uintptr_t s_engineReturnAddr = 0;
static uint8_t s_engineSavedBytes[8] = {};
static uint8_t s_enginePatchedBytes[8] = {};
static bool s_engineInitialized = false;

// Watchdog: re-patches the Engine hook when game integrity checker restores bytes
static HANDLE s_watchdogThread = nullptr;
static volatile bool s_watchdogRunning = false;

static uintptr_t s_engineReplayTrampoline = 0;

// Naked detour at Engine hook site.
// Minimal engine detour used by gwa3's current packet transport.
// It wakes or drains the queued sender-thread work without the old experimental
// AutoIt-style command lane.
// PacketSend cannot be called from within any frame hook (deadlocks).
// The sender thread runs BETWEEN frames, outside the frame lock.
static HANDLE s_packetReadyEvent = nullptr;

// Packet task ring buffer for sender thread dispatch
struct PacketTask {
    PacketSendFn fn;
    uintptr_t location;
    uint32_t sizeBytes;
    uint32_t data[12];
};
static PacketTask s_packetRing[64];
static volatile LONG s_pktHead = 0;
static volatile LONG s_pktTail = 0;

static DWORD WINAPI PacketSenderThread(LPVOID) {
    while (s_watchdogRunning) { // reuse watchdog flag for lifetime
        WaitForSingleObject(s_packetReadyEvent, 200);
        while (s_pktTail != s_pktHead) {
            LONG idx = s_pktTail % 64;
            PacketTask t = s_packetRing[idx];
            InterlockedIncrement(&s_pktTail);

            // Re-read PacketLocation fresh
            uintptr_t loc = t.location;
            if (Offsets::PacketLocation) {
                uintptr_t fresh = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
                if (fresh) loc = fresh;
            }
            __try {
                t.fn(reinterpret_cast<void*>(loc), t.sizeBytes, t.data);
            } __except(
                Log::Error("CtoS: EXCEPTION 0x%08X at 0x%08X calling PacketSend hdr=0x%X",
                    GetExceptionCode(),
                    reinterpret_cast<uintptr_t>(GetExceptionInformation()->ExceptionRecord->ExceptionAddress),
                    t.data[0]),
                EXCEPTION_EXECUTE_HANDLER
            ) {
                Log::Error("CtoS: PacketSend crashed â€” continuing");
            }
            Sleep(10);
        }
    }
    return 0;
}

// Packet transport instrumentation.
// s_engineCallTest tracks engine-thread dispatch observations for debugging.

// Pointers used by the dispatch path â€” set during Initialize from Offsets
static volatile LONG s_engineCallTest = 0;
static void (__stdcall* s_engineDispatchOnePtr)() = nullptr;

static void __stdcall EngineDispatchOne() {
    if (s_pktTail == s_pktHead) return;

    LONG idx = s_pktTail % 64;
    PacketTask t = s_packetRing[idx];
    InterlockedIncrement(&s_pktTail);

    uintptr_t loc = t.location;
    if (Offsets::PacketLocation) {
        uintptr_t fresh = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
        if (fresh) loc = fresh;
    }

    InterlockedIncrement(&s_engineCallTest);
    t.fn(reinterpret_cast<void*>(loc), t.sizeBytes, t.data);
}

static __declspec(naked) void EngineDetourNaked() {
    __asm {
        pushad
        pushfd

        inc dword ptr [s_heartbeat]


        // Check if normal packets pending — if so, dispatch one from game thread
        mov eax, dword ptr [s_pktTail]
        cmp eax, dword ptr [s_pktHead]
        je no_packet
        call dword ptr [s_engineDispatchOnePtr]
    no_packet:

        popfd
        popad
        jmp [s_engineReplayTrampoline]
    }
}

static DWORD WINAPI EngineWatchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(5);
        if (!s_engineInitialized || !s_engineHookAddr) continue;

        const uint8_t* cur = reinterpret_cast<const uint8_t*>(s_engineHookAddr);
        if (memcmp(cur, s_enginePatchedBytes, 5) != 0) {
            DWORD oldProtect;
            if (VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                               PAGE_EXECUTE_READWRITE, &oldProtect)) {
                memcpy(reinterpret_cast<void*>(s_engineHookAddr), s_enginePatchedBytes, 5);
                FlushInstructionCache(GetCurrentProcess(),
                                     reinterpret_cast<void*>(s_engineHookAddr), 5);
                VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                               oldProtect, &oldProtect);
                Log::Info("CtoS: [WATCHDOG] Engine hook re-patched");
            }
        }
    }
    return 0;
}

static bool InstallEngineHook() {
    s_engineHookAddr = Offsets::Engine;
    if (!s_engineHookAddr) {
        Log::Error("CtoS: Offsets::Engine not resolved");
        return false;
    }

    s_engineReturnAddr = s_engineHookAddr + 5;

    // Save original 5 bytes
    memcpy(s_engineSavedBytes, reinterpret_cast<void*>(s_engineHookAddr), 5);
    Log::Info("CtoS: Engine at 0x%08X, original bytes: %02X %02X %02X %02X %02X",
              s_engineHookAddr,
              s_engineSavedBytes[0], s_engineSavedBytes[1], s_engineSavedBytes[2],
              s_engineSavedBytes[3], s_engineSavedBytes[4]);

    // Build replay trampoline: original 5 bytes + JMP to returnAddr
    void* tramp = VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) {
        Log::Error("CtoS: VirtualAlloc for Engine trampoline failed");
        return false;
    }
    uint8_t* t = reinterpret_cast<uint8_t*>(tramp);
    memcpy(t, s_engineSavedBytes, 5);
    t[5] = 0xE9; // JMP rel32
    int32_t jmpRel = static_cast<int32_t>(s_engineReturnAddr - reinterpret_cast<uintptr_t>(t + 5 + 4));
    memcpy(t + 6, &jmpRel, 4);
    FlushInstructionCache(GetCurrentProcess(), t, 10);
    s_engineReplayTrampoline = reinterpret_cast<uintptr_t>(tramp);
    s_engineDispatchOnePtr = &EngineDispatchOne;

    // Write 5-byte JMP to our detour
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t patch[5];
    patch[0] = 0xE9;
    int32_t hookRel = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(&EngineDetourNaked) - (s_engineHookAddr + 5));
    memcpy(patch + 1, &hookRel, 4);
    memcpy(reinterpret_cast<void*>(s_engineHookAddr), patch, 5);
    memcpy(s_enginePatchedBytes, patch, 5);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(s_engineHookAddr), 5);

    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   oldProtect, &oldProtect);

    // Start watchdog
    s_watchdogRunning = true;
    s_watchdogThread = CreateThread(nullptr, 0, EngineWatchdog, nullptr, 0, nullptr);

    // Start packet sender thread + event
    s_packetReadyEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    CreateThread(nullptr, 0, PacketSenderThread, nullptr, 0, nullptr);

    s_engineInitialized = true;
    Log::Info("CtoS: Engine hook installed at 0x%08X, sender thread started",
              s_engineHookAddr);
    return true;
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

    // Install Engine inline hook for packet dispatch
    if (!InstallEngineHook()) {
        Log::Warn("CtoS: Engine hook failed â€” packets will be dropped");
    }

    return true;
}

// Core send: queue a packet for the sender-thread/game-thread transport.
void SendPacket(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacket dropped (header=0x%X) â€” not initialized", header);
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
    const bool traceHeroKick = header == Packets::HERO_KICK;

    // Re-read PacketLocation every call
    if (Offsets::PacketLocation) {
        uintptr_t fresh = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
        if (fresh) s_packetLocation = fresh;
    }

    // Enqueue to sender-thread ring buffer
    if (s_engineInitialized && s_packetReadyEvent) {
        LONG idx = InterlockedIncrement(&s_pktHead) - 1;
        PacketTask& t = s_packetRing[idx % 64];
        t.fn = s_packetSendFn;
        t.location = s_packetLocation;
        t.sizeBytes = sizeBytes;
        memcpy(t.data, data, sizeBytes);
        if (traceHeroKick) {
            const uint32_t arg1 = size > 1 ? data[1] : 0u;
            Log::Info("CtoS: HERO_KICK queued arg=0x%X pktIdx=%ld loc=0x%08X hb=%ld engineCalls=%ld head=%ld tail=%ld",
                      arg1,
                      idx,
                      static_cast<unsigned>(s_packetLocation),
                      s_heartbeat,
                      s_engineCallTest,
                      s_pktHead,
                      s_pktTail);
        }
        SetEvent(s_packetReadyEvent);
        return;
    }

    Log::Warn("CtoS: SendPacket dropped header=0x%X â€” not ready", header);
}

// --- Type-safe wrappers ---


void MoveToCoord(float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(3, Packets::MOVE_TO_COORD, ix, iy);
}

void Dialog(uint32_t dialogId) {
    // Current AutoIt GWA2 logic uses 0x3A for dialog sends.
    SendPacket(2, Packets::DIALOG_SEND_LIVING, dialogId);
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



















