#include <gwa3/packets/CtoSHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

namespace GWA3::CtoSHook {

static constexpr uint32_t kQueueSize = 256;
static constexpr uint32_t kPatchSize = 5;
static constexpr uint32_t kReplaySize = 10; // original instruction group

static volatile LONG s_heartbeat = 0;
static volatile LONG s_queueCounter = 0;
static volatile LONG s_savedCommand = 0;
static volatile LONG s_savedESP = 0;

static uintptr_t s_queue[kQueueSize] = {};
static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;
static uintptr_t s_trampoline = 0;
static uint8_t s_savedBytes[kPatchSize] = {};
static uint8_t s_patchedBytes[kPatchSize] = {};

// Watchdog for re-patching when the game's integrity checker restores original bytes
static HANDLE s_watchdogThread = nullptr;
static volatile bool s_watchdogRunning = false;

// Naked detour — processes one CtoS shellcode command per frame, then
// replays the original overwritten instructions via trampoline.
static __declspec(naked) void CtoSDetourNaked() {
    __asm {
        // Save ESP and all registers
        mov dword ptr [s_savedESP], esp
        pushad
        pushfd

        // Check queue
        mov eax, dword ptr [s_queueCounter]
        mov ecx, eax
        mov ebx, dword ptr [s_queue + eax * 4]
        test ebx, ebx
        jz skip_queue

        // Consume and execute one command
        mov dword ptr [s_queue + eax * 4], 0
        mov dword ptr [s_savedCommand], ebx

        mov eax, ecx
        inc eax
        cmp eax, kQueueSize
        jnz no_reset
        xor eax, eax
    no_reset:
        mov dword ptr [s_queueCounter], eax
        call dword ptr [s_savedCommand]

    skip_queue:
        inc dword ptr [s_heartbeat]
        popfd
        popad
        mov esp, dword ptr [s_savedESP]
        // Jump to trampoline: original 10 bytes + JMP to hookAddr+0xA
        jmp [s_trampoline]
    }
}

// Watchdog: re-patches the mid-function hook when the game's integrity
// checker restores original bytes (~38s cycle).
static DWORD WINAPI Watchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(500);
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

    // Save original bytes
    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    // Build trampoline: copy original 10 bytes + JMP to hookAddr+0xA
    {
        void* tramMem = VirtualAlloc(nullptr, 32, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!tramMem) {
            Log::Error("CtoSHook: VirtualAlloc failed for trampoline");
            return false;
        }
        auto* t = reinterpret_cast<uint8_t*>(tramMem);
        memcpy(t, reinterpret_cast<void*>(hookAddr), kReplaySize);
        t[kReplaySize] = 0xE9; // JMP rel32
        int32_t tramRel = static_cast<int32_t>(
            s_returnAddr - (reinterpret_cast<uintptr_t>(tramMem) + kReplaySize + 5));
        memcpy(t + kReplaySize + 1, &tramRel, 4);
        FlushInstructionCache(GetCurrentProcess(), tramMem, kReplaySize + 5);
        s_trampoline = reinterpret_cast<uintptr_t>(tramMem);
    }

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
    Log::Info("CtoSHook: Installed at 0x%08X (trampoline=0x%08X, watchdog active)",
              hookAddr, s_trampoline);
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

    // Free trampoline
    if (s_trampoline) {
        VirtualFree(reinterpret_cast<void*>(s_trampoline), 0, MEM_RELEASE);
        s_trampoline = 0;
    }

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
