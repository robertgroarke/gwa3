#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

namespace GWA3::RenderHook {

static constexpr uint32_t kQueueSize = 256;
static constexpr uint32_t kPatchSize = 5;
static constexpr uint32_t kReplaySize = 10; // full instruction group to replay

static volatile LONG s_queueCounter = 0;
static volatile LONG s_savedCommand = 0;
static volatile LONG s_mapLoaded = 0;
static volatile LONG s_disableRendering = 0;
static volatile LONG s_heartbeat = 0;

// Full FP/SSE/MXCSR save area — fxsave needs 512 bytes, 16-byte aligned
__declspec(align(16)) static uint8_t s_fxState[512] = {};
static uintptr_t s_queue[kQueueSize] = {};
static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;
static uintptr_t s_trampoline = 0;  // executable stub: original 10 bytes + JMP return
static uint8_t s_savedBytes[kPatchSize] = {};

// Naked detour — called via JMP from mid-function patch.
// Mirrors the AutoIt RenderingModProc path:
// process one queued command during pre-game, then replay the original
// overwritten instructions before jumping back to Render+0xA.
static volatile LONG s_savedESP = 0;

// Set to 1 to test MINIMAL hook: only heartbeat inc, no pushad/popfd/queue
// Set to 2 for normal operation with full command dispatch
// Mode 1: minimal (heartbeat + trampoline only) — PROVEN STABLE at 12309 hb
// Mode 2: full + FPU save/restore — testing FPU corruption theory
// Mode 3: pushad/popfd NO queue — PROVEN STABLE at 12338 hb
// Mode 4: pushad + queue drain (no call) — PROVEN STABLE at 12345 hb
// Mode 2: full dispatch. Commands called from render hook detour.
// After SetMapLoaded(true), command dispatch is disabled to test if
// the crash is from in-game shellcode calls or something else.
#define RENDERHOOK_MODE 2

static __declspec(naked) void RenderDetourNaked() {
    __asm {
#if RENDERHOOK_MODE == 1
        // MINIMAL: only heartbeat, no register save/restore, no queue
        inc dword ptr [s_heartbeat]
        jmp [s_trampoline]
#elif RENDERHOOK_MODE == 3
        // PUSHAD TEST: save/restore regs but NO queue dispatch
        mov dword ptr [s_savedESP], esp
        pushad
        pushfd
        inc dword ptr [s_heartbeat]
        popfd
        popad
        mov esp, dword ptr [s_savedESP]
        jmp [s_trampoline]
#elif RENDERHOOK_MODE == 4
        // QUEUE DRAIN TEST: read queue, clear slots, advance counter, but NO call
        mov dword ptr [s_savedESP], esp
        pushad
        pushfd

        mov eax, dword ptr [s_queueCounter]
        mov ebx, dword ptr [s_queue + eax * 4]
        test ebx, ebx
        jz skip_queue

        mov dword ptr [s_queue + eax * 4], 0
        // Skip: mov [s_savedCommand], ebx / call [s_savedCommand]
        inc eax
        cmp eax, kQueueSize
        jnz no_reset
        xor eax, eax
no_reset:
        mov dword ptr [s_queueCounter], eax

skip_queue:
        inc dword ptr [s_heartbeat]
        popfd
        popad
        mov esp, dword ptr [s_savedESP]
        jmp [s_trampoline]
#else
        // After map load, do NOTHING except heartbeat + trampoline (mode 1 equivalent)
        cmp dword ptr [s_mapLoaded], 1
        jz ingame_minimal

        // PRE-GAME: full dispatch for bootstrap ButtonClick
        mov dword ptr [s_savedESP], esp
        pushad
        pushfd

        mov eax, dword ptr [s_queueCounter]
        mov ecx, eax
        mov ebx, dword ptr [s_queue + eax * 4]
        test ebx, ebx
        jz skip_queue

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
        jmp [s_trampoline]

ingame_minimal:
        // IN-GAME: absolute minimum — just heartbeat, no pushad, no queue
        inc dword ptr [s_heartbeat]
#endif
        // Jump to trampoline: original 10 bytes + JMP to hookAddr+0xA
        jmp [s_trampoline]
    }
}

bool Initialize() {
    if (s_initialized) return true;
    if (!Offsets::Render) {
        Log::Error("RenderHook: Render offset not resolved");
        return false;
    }

    uintptr_t hookAddr = Offsets::Render;
    s_returnAddr = hookAddr + 0xA; // AutoIt uses RenderingModReturn = Render + 0xA
    s_queueCounter = 0;
    s_savedCommand = 0;
    s_mapLoaded = 0;
    s_disableRendering = 0;
    ZeroMemory(s_queue, sizeof(s_queue));

    // Dump the original bytes at the hook site for debugging
    {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(hookAddr);
        Log::Info("RenderHook: Original bytes at 0x%08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                  hookAddr, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14]);
        // Also dump 10 bytes BEFORE hook site for context
        const uint8_t* before = p - 10;
        Log::Info("RenderHook: 10 bytes before: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                  before[0], before[1], before[2], before[3], before[4],
                  before[5], before[6], before[7], before[8], before[9]);
    }

    // AutoIt's WriteDetour writes a 5-byte JMP at the hook site and returns
    // to Render+0xA from the detour body. Keep bytes +5..+9 intact.
    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    // Build trampoline: copy original 10 bytes + JMP to hookAddr+0xA
    {
        void* tramMem = VirtualAlloc(nullptr, 32, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!tramMem) {
            Log::Error("RenderHook: VirtualAlloc failed for trampoline");
            return false;
        }
        auto* t = reinterpret_cast<uint8_t*>(tramMem);
        // Copy original 10 bytes
        memcpy(t, reinterpret_cast<void*>(hookAddr), kReplaySize);
        // Append JMP to hookAddr+0xA
        t[10] = 0xE9; // JMP rel32
        int32_t tramRel = static_cast<int32_t>(s_returnAddr - (reinterpret_cast<uintptr_t>(tramMem) + 15));
        memcpy(t + 11, &tramRel, 4);
        FlushInstructionCache(GetCurrentProcess(), tramMem, 15);
        s_trampoline = reinterpret_cast<uintptr_t>(tramMem);
        Log::Info("RenderHook: Trampoline at 0x%08X", s_trampoline);
    }

    // Write only the 5-byte JMP detour, matching AutoIt's behavior.
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9; // JMP rel32
    int32_t rel = reinterpret_cast<uintptr_t>(&RenderDetourNaked) - (hookAddr + 5);
    memcpy(patch + 1, &rel, 4);

    memcpy(reinterpret_cast<void*>(hookAddr), patch, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);

    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    s_initialized = true;
    Log::Info("RenderHook: Installed at 0x%08X -> return at 0x%08X",
              hookAddr, s_returnAddr);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Restore original bytes
    uintptr_t hookAddr = Offsets::Render;
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    ZeroMemory(s_queue, sizeof(s_queue));
    s_queueCounter = 0;
    s_savedCommand = 0;
    s_mapLoaded = 0;
    s_initialized = false;
    Log::Info("RenderHook: Shutdown");
}

bool EnqueueCommand(uintptr_t command) {
    if (!s_initialized || command < 0x10000) return false;

    for (uint32_t attempt = 0; attempt < kQueueSize; ++attempt) {
        uint32_t index = (static_cast<uint32_t>(s_queueCounter) + attempt) % kQueueSize;
        if (s_queue[index] != 0) continue;
        s_queue[index] = command;
        return true;
    }

    Log::Warn("RenderHook: queue full, dropping command 0x%08X", command);
    return false;
}

void SetMapLoaded(bool loaded) {
    s_mapLoaded = loaded ? 1 : 0;
    Log::Info("RenderHook: MapLoaded=%d", loaded ? 1 : 0);
}

bool IsInitialized() {
    return s_initialized;
}

uint32_t GetQueueCounter() {
    return static_cast<uint32_t>(s_queueCounter);
}

uint32_t GetPendingCount() {
    uint32_t count = 0;
    for (uint32_t i = 0; i < kQueueSize; i++) {
        if (s_queue[i] != 0) count++;
    }
    return count;
}

uint32_t GetHeartbeat() {
    return static_cast<uint32_t>(s_heartbeat);
}

bool IsCrashDetected() {
    return false;
}

bool IsHookIntact() {
    if (!s_initialized) return false;
    uintptr_t hookAddr = Offsets::Render;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(hookAddr);
    // Check if our E9 JMP is still the first byte
    return (p[0] == 0xE9);
}

} // namespace GWA3::RenderHook
