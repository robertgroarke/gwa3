#include <gwa3/packets/CtoSHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::CtoSHook {

static constexpr uint32_t kQueueSize = 256;
static constexpr uint32_t kPatchSize = 5;
static constexpr uint32_t kReplaySize = 10; // add esp,4 (3) + cmp dword ptr [addr],0 (7)
static constexpr uint32_t kCommandSlotCount = 32;

using PacketSendFn = void(__cdecl*)(void*, uint32_t, uint32_t*);

struct PacketCommand {
    uintptr_t entry;
    uint32_t sizeBytes;
    uint32_t data[12];
};
static_assert(sizeof(PacketCommand) <= 64, "PacketCommand must remain compact");

static volatile LONG s_heartbeat = 0;
static volatile LONG s_queueCounter = 0;
static volatile LONG s_savedCommand = 0;
static volatile LONG s_savedESP = 0;
static volatile LONG s_savedIndex = -1;
static volatile LONG s_packetCommandSlot = 0;

static uintptr_t s_queue[kQueueSize] = {};
static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;   // hookAddr + 10
static uintptr_t s_cmpAddr = 0;      // operand for replicated cmp instruction
static uint8_t s_savedBytes[kPatchSize] = {};
static uint8_t s_patchedBytes[kPatchSize] = {};

static PacketSendFn s_packetSendFn = nullptr;
static uintptr_t s_packetLocation = 0;
static uintptr_t s_packetCommandPool = 0;
static uintptr_t s_commandReturnAddr = 0;

// Watchdog for re-patching when the game's integrity checker restores original bytes
static HANDLE s_watchdogThread = nullptr;
static volatile bool s_watchdogRunning = false;

static bool EnsurePacketCommandPool() {
    if (s_packetCommandPool) return true;

    void* mem = VirtualAlloc(nullptr,
                             sizeof(PacketCommand) * kCommandSlotCount,
                             MEM_RESERVE | MEM_COMMIT,
                             PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("CtoSHook: VirtualAlloc failed for packet command pool");
        return false;
    }

    s_packetCommandPool = reinterpret_cast<uintptr_t>(mem);
    ZeroMemory(mem, sizeof(PacketCommand) * kCommandSlotCount);
    return true;
}

static uintptr_t NextPacketCommandSlot() {
    if (!EnsurePacketCommandPool()) return 0;
    LONG idx = InterlockedIncrement(&s_packetCommandSlot) - 1;
    return s_packetCommandPool + (static_cast<uint32_t>(idx) % kCommandSlotCount) * sizeof(PacketCommand);
}

// AutoIt-style packet command entry:
//   EAX -> PacketCommand blob
//   [eax+4] = packet size in bytes
//   [eax+8] = packet data
static __declspec(naked) void CommandPacketSendNaked() {
    __asm {
        lea edx, [eax + 8]
        push edx
        mov ebx, dword ptr [eax + 4]
        push ebx
        mov ebx, dword ptr [s_packetLocation]
        push ebx
        mov ebx, dword ptr [s_packetSendFn]
        call ebx
        add esp, 0x0C
        jmp [s_commandReturnAddr]
    }
}

extern "C" void __declspec(naked) GWA3CtoSHookCommandReturnThunk() {
    __asm {
        mov ecx, dword ptr [s_savedIndex]
        mov edx, dword ptr [s_queueCounter]
        cmp edx, ecx
        jnz exit_path
        mov eax, ecx
        inc eax
        cmp eax, kQueueSize
        jnz no_reset
        xor eax, eax
    no_reset:
        mov dword ptr [s_queueCounter], eax
    exit_path:
        popfd
        popad
        mov esp, dword ptr [s_savedESP]
        add esp, 4
        push eax
        mov eax, dword ptr [s_cmpAddr]
        cmp dword ptr [eax], 0
        pop eax
        jmp [s_returnAddr]
    }
}

// Naked detour mirroring AutoIt's command-object execution shape more closely:
// queue slot holds a pointer to a command blob, EAX points at the blob on entry,
// and control transfers via JMP into the command entrypoint stored at [EAX].
static __declspec(naked) void CtoSDetourNaked() {
    __asm {
        mov dword ptr [s_savedESP], esp
        pushad
        pushfd

        inc dword ptr [s_heartbeat]

        mov eax, dword ptr [s_queueCounter]
        mov ecx, eax
        mov ebx, dword ptr [s_queue + eax * 4]
        test ebx, ebx
        jz no_command

        mov dword ptr [s_queue + eax * 4], 0
        mov dword ptr [s_savedCommand], ebx
        mov dword ptr [s_savedIndex], ecx
        mov eax, ebx
        mov ebx, dword ptr [eax]
        jmp ebx

    no_command:
        popfd
        popad
        mov esp, dword ptr [s_savedESP]
        add esp, 4
        push eax
        mov eax, dword ptr [s_cmpAddr]
        cmp dword ptr [eax], 0
        pop eax
        jmp [s_returnAddr]
    }
}

static DWORD WINAPI Watchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(5);
        if (!s_initialized) continue;

        uintptr_t hookAddr = Offsets::Render;
        if (hookAddr < 0x10000) continue;

        const uint8_t* cur = reinterpret_cast<const uint8_t*>(hookAddr);
        if (memcmp(cur, s_patchedBytes, kPatchSize) != 0) {
            DWORD oldProtect;
            if (VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize,
                               PAGE_EXECUTE_READWRITE, &oldProtect)) {
                memcpy(reinterpret_cast<void*>(hookAddr), s_patchedBytes, kPatchSize);
                FlushInstructionCache(GetCurrentProcess(),
                                      reinterpret_cast<void*>(hookAddr), kPatchSize);
                VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize,
                               oldProtect, &oldProtect);
                Log::Info("CtoSHook: [WATCHDOG] Re-patched mid-function hook");
            }
        }
    }
    return 0;
}

bool Initialize() {
    if (s_initialized) return true;
    if (!Offsets::Render) {
        Log::Error("CtoSHook: Render offset not resolved");
        return false;
    }
    if (!Offsets::PacketSend || !Offsets::PacketLocation) {
        Log::Error("CtoSHook: PacketSend or PacketLocation offset not resolved");
        return false;
    }

    s_packetSendFn = reinterpret_cast<PacketSendFn>(Offsets::PacketSend);
    s_packetLocation = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
    if (!s_packetLocation) {
        Log::Error("CtoSHook: PacketLocation dereference returned null");
        return false;
    }

    uintptr_t hookAddr = Offsets::Render;
    s_returnAddr = hookAddr + kReplaySize;
    s_commandReturnAddr = reinterpret_cast<uintptr_t>(&GWA3CtoSHookCommandReturnThunk);
    s_queueCounter = 0;
    s_savedCommand = 0;
    s_savedIndex = -1;
    ZeroMemory(s_queue, sizeof(s_queue));

    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    const uint8_t* orig = reinterpret_cast<const uint8_t*>(hookAddr);
    if (orig[0] != 0x83 || orig[1] != 0xC4 || orig[2] != 0x04 ||
        orig[3] != 0x83 || orig[4] != 0x3D) {
        Log::Error("CtoSHook: Unexpected bytes at hook site: %02X %02X %02X %02X %02X",
                   orig[0], orig[1], orig[2], orig[3], orig[4]);
        return false;
    }

    memcpy(&s_cmpAddr, orig + 5, 4);
    Log::Info("CtoSHook: cmp operand addr=0x%08X PacketSend=0x%08X PacketLocation=0x%08X",
              s_cmpAddr,
              static_cast<unsigned>(Offsets::PacketSend),
              static_cast<unsigned>(s_packetLocation));

    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9;
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&CtoSDetourNaked) - (hookAddr + 5));
    memcpy(patch + 1, &rel, 4);
    memcpy(reinterpret_cast<void*>(hookAddr), patch, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);

    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    memcpy(s_patchedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    s_watchdogRunning = true;
    s_watchdogThread = CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr);

    s_initialized = true;
    Log::Info("CtoSHook: Installed at 0x%08X (AutoIt-style command lane experiment)", hookAddr);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    s_watchdogRunning = false;
    if (s_watchdogThread) {
        WaitForSingleObject(s_watchdogThread, 2000);
        CloseHandle(s_watchdogThread);
        s_watchdogThread = nullptr;
    }

    uintptr_t hookAddr = Offsets::Render;
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    ZeroMemory(s_queue, sizeof(s_queue));
    s_queueCounter = 0;
    s_savedIndex = -1;
    s_initialized = false;
    Log::Info("CtoSHook: Shutdown");
}

bool EnqueueCommand(uintptr_t command) {
    if (!s_initialized || command < 0x10000) return false;

    for (uint32_t attempt = 0; attempt < kQueueSize; ++attempt) {
        uint32_t index = (static_cast<uint32_t>(s_queueCounter) + attempt) % kQueueSize;
        if (s_queue[index] != 0) continue;
        s_queue[index] = command;
        return true;
    }

    Log::Warn("CtoSHook: queue full, dropping command 0x%08X", command);
    return false;
}

bool IsInitialized() {
    return s_initialized;
}

uint32_t GetHeartbeat() {
    return static_cast<uint32_t>(s_heartbeat);
}

} // namespace GWA3::CtoSHook
