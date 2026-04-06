#include <gwa3/packets/CtoSHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

namespace GWA3::CtoSHook {

static constexpr uint32_t kQueueSize = 256;
static constexpr uint32_t kPatchSize = 5;
static constexpr uint32_t kReplaySize = 10; // add esp,4 (3) + cmp dword ptr [addr],0 (7)

static volatile LONG s_heartbeat = 0;
static volatile LONG s_queueCounter = 0;
static volatile LONG s_savedCommand = 0;
static volatile LONG s_savedESP = 0;

static uintptr_t s_queue[kQueueSize] = {};
static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;   // hookAddr + 10
static uintptr_t s_cmpAddr = 0;      // operand for replicated cmp instruction
static uintptr_t s_replayCode = 0;   // VirtualAlloc'd: original 10 bytes + JMP return
static uint8_t s_savedBytes[kPatchSize] = {};
static uint8_t s_patchedBytes[kPatchSize] = {};

// Watchdog for re-patching when the game's integrity checker restores original bytes
static HANDLE s_watchdogThread = nullptr;
static volatile bool s_watchdogRunning = false;

// Naked detour — processes one CtoS shellcode command per frame, then
// replicates the overwritten instructions inline (no trampoline).
// Original 10 bytes: 83 C4 04 (add esp,4) + 83 3D [addr] 00 (cmp dword ptr [addr],0)
// We replicate both instructions, then JMP to hookAddr+10.
static __declspec(naked) void CtoSDetourNaked() {
    __asm {
        // BISECT: absolute minimum — just replay original instructions, no queue
        add esp, 4
        push eax
        mov eax, dword ptr [s_cmpAddr]
        cmp dword ptr [eax], 0
        pop eax
        jmp [s_returnAddr]
    }
}

// Watchdog: re-patches the mid-function hook when the game's integrity
// checker restores original bytes (~38s cycle).
static DWORD WINAPI Watchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(5); // fast poll — must re-patch before the next render frame
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

    uintptr_t hookAddr = Offsets::Render;
    s_returnAddr = hookAddr + kReplaySize;
    s_queueCounter = 0;
    s_savedCommand = 0;
    ZeroMemory(s_queue, sizeof(s_queue));

    // Save original bytes and extract cmp operand address.
    // Original 10 bytes: 83 C4 04 83 3D [4-byte addr] 00
    //   add esp, 4        (bytes 0-2)
    //   cmp dword ptr [addr], 0  (bytes 3-9, addr at bytes 5-8)
    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    const uint8_t* orig = reinterpret_cast<const uint8_t*>(hookAddr);
    // Verify expected instruction pattern
    if (orig[0] != 0x83 || orig[1] != 0xC4 || orig[2] != 0x04 ||
        orig[3] != 0x83 || orig[4] != 0x3D) {
        Log::Error("CtoSHook: Unexpected bytes at hook site: %02X %02X %02X %02X %02X",
                   orig[0], orig[1], orig[2], orig[3], orig[4]);
        return false;
    }
    // Extract the absolute address from the cmp instruction (bytes 5-8)
    memcpy(&s_cmpAddr, orig + 5, 4);
    Log::Info("CtoSHook: cmp operand addr=0x%08X", s_cmpAddr);

    // Write 5-byte JMP detour
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9;
    int32_t rel = reinterpret_cast<uintptr_t>(&CtoSDetourNaked) - (hookAddr + 5);
    memcpy(patch + 1, &rel, 4);
    memcpy(reinterpret_cast<void*>(hookAddr), patch, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);

    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    // Save patched bytes for watchdog comparison
    memcpy(s_patchedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    // Start watchdog thread
    s_watchdogRunning = true;
    s_watchdogThread = CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr);

    s_initialized = true;
    Log::Info("CtoSHook: Installed at 0x%08X (inline replay, watchdog active)", hookAddr);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Stop watchdog
    s_watchdogRunning = false;
    if (s_watchdogThread) {
        WaitForSingleObject(s_watchdogThread, 2000);
        CloseHandle(s_watchdogThread);
        s_watchdogThread = nullptr;
    }

    // Restore original bytes
    uintptr_t hookAddr = Offsets::Render;
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    ZeroMemory(s_queue, sizeof(s_queue));
    s_queueCounter = 0;
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
