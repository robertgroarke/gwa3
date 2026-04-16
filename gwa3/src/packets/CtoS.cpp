#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <MinHook.h>
#include <cstdarg>
#include <cstring>

namespace GWA3::CtoS {

// Game's PacketSend calling convention:
//   void __cdecl PacketSend(void* packetLocationPtr, uint32_t size, uint32_t* data)
using PacketSendFn = void(__cdecl*)(void*, uint32_t, uint32_t*);

static PacketSendFn s_packetSendFn = nullptr;
static PacketSendFn s_packetSendOriginal = nullptr;  // trampoline for packet tap
static bool s_packetTapEnabled = false;
static uintptr_t s_packetLocation = 0;
static bool s_initialized = false;

// Packet tap: intercept ALL outgoing CtoS packets (including game UI actions)
static void __cdecl PacketSendTap(void* loc, uint32_t sizeBytes, uint32_t* data) {
    if (s_packetTapEnabled && data && sizeBytes >= 4) {
        const uint32_t hdr = data[0];
        const uint32_t numDwords = sizeBytes / 4;
        // Log header + first 4 data dwords for identification
        if (numDwords >= 4) {
            Log::Info("[PACKET-TAP] hdr=0x%02X size=%u d1=0x%08X d2=0x%08X d3=0x%08X",
                      hdr, sizeBytes, data[1], data[2], data[3]);
        } else if (numDwords >= 3) {
            Log::Info("[PACKET-TAP] hdr=0x%02X size=%u d1=0x%08X d2=0x%08X",
                      hdr, sizeBytes, data[1], data[2]);
        } else if (numDwords >= 2) {
            Log::Info("[PACKET-TAP] hdr=0x%02X size=%u d1=0x%08X",
                      hdr, sizeBytes, data[1]);
        } else {
            Log::Info("[PACKET-TAP] hdr=0x%02X size=%u", hdr, sizeBytes);
        }
    }
    if (s_packetSendOriginal) {
        s_packetSendOriginal(loc, sizeBytes, data);
    }
}

static void IssuePacketSend(const uint32_t* data, uint32_t sizeBytes);

// ===== Engine inline hook for CtoS dispatch =====
// Hooks at Offsets::Engine (0x00C93C91) -- a DIFFERENT function from the
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
static volatile bool s_engineSuspended = false;

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
                Log::Error("CtoS: PacketSend crashed -- continuing");
            }
            Sleep(10);
        }
    }
    return 0;
}

// Packet transport instrumentation.
// s_engineCallTest tracks engine-thread dispatch observations for debugging.

// Pointers used by the dispatch path — set during Initialize from Offsets
static volatile LONG s_engineCallTest = 0;
static void (__stdcall* s_engineDispatchOnePtr)() = nullptr;
static int (__stdcall* s_shouldDeferBotshubCommandsPtr)() = nullptr;

// Cached offset pointers for the inline asm environment gate (avoids C++
// function calls on the engine hook hot path — C++ calls corrupt EBP and
// the game's frame context).
static uintptr_t s_cachedBasePointer = 0;     // = Offsets::BasePointer
static uintptr_t s_cachedEnvironment = 0;     // = Offsets::Environment

// Diagnostic counters for HandleCase vs RegularFlow frequency
static volatile LONG s_deferCount = 0;      // HandleCase ticks (command deferred)
static volatile LONG s_deferWithCmd = 0;     // HandleCase ticks where a command was actually waiting
static volatile LONG s_regularFlowExec = 0;  // RegularFlow ticks where a command was executed

// ===== Game Command Queue (GWA3-184) =====
// Secondary ring buffer for game function calls that must execute in the
// Engine hook context (same hook point as AutoIt's command queue).
// Used for operations like Salvage that need the game's internal context.
typedef void (*GameCommandFn)(void* params);
struct GameCommand {
    GameCommandFn fn;
    uint8_t params[248]; // 256 - 8 bytes for fn + padding
};
static constexpr LONG kGameCommandQueueSize = 64;
static GameCommand s_cmdRing[kGameCommandQueueSize];
static volatile LONG s_cmdHead = 0;
static volatile LONG s_cmdTail = 0;
static volatile LONG s_cmdWriteLock = 0;

struct BotshubCommandSlot {
    uint8_t bytes[256];
};
static constexpr LONG kBotshubCommandQueueSize = 64;
static BotshubCommandSlot s_botshubCmdRing[kBotshubCommandQueueSize];
static volatile LONG s_botshubCmdHead = 0;
static volatile LONG s_botshubCmdTail = 0;
static volatile LONG s_botshubCmdWriteLock = 0;

static void __stdcall EngineDispatchCommand() {
    if (s_cmdTail == s_cmdHead) return;
    LONG idx = s_cmdTail % kGameCommandQueueSize;
    GameCommand cmd = s_cmdRing[idx];
    InterlockedIncrement(&s_cmdTail);
    if (cmd.fn) {
        cmd.fn(cmd.params);
    }
}

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

static void (__stdcall* s_engineDispatchCmdPtr)() = nullptr;
static int __stdcall ShouldDeferBotshubCommands() {
    if (!Offsets::BasePointer || !Offsets::Environment) return 0;

    __try {
        uintptr_t world = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (world <= 0x10000) return 0;

        world = *reinterpret_cast<uintptr_t*>(world);
        if (world <= 0x10000) return 0;

        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(world + 0x18);
        if (p1 <= 0x10000) return 0;

        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x44);
        if (p2 <= 0x10000) return 0;

        const uint32_t state_guard = *reinterpret_cast<uint32_t*>(p2 + 0x19C);
        if (state_guard == 0u) return 0;

        const uint32_t env_index = *reinterpret_cast<uint32_t*>(p2 + 0x198);
        if (env_index == 0u) return 1;

        // Offsets::Environment is the CODE address of the ADD EAX,imm32 operand.
        // We must dereference to get the actual environment array base pointer
        // (same as upstream's `add ebx, dword[Environment]`).
        const uintptr_t env_base = *reinterpret_cast<uintptr_t*>(Offsets::Environment);
        if (env_base <= 0x10000) return 0;

        const uintptr_t env_entry = env_base + static_cast<uintptr_t>(env_index) * 0x7Cu;
        if (env_entry <= 0x10000) return 0;

        const uint32_t flags = *reinterpret_cast<uint32_t*>(env_entry + 0x10);
        return (flags & 0x40001u) ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// 108-byte buffer for fsave/frstor (x87 FPU state, 28 dwords).
// Statically allocated — the engine detour is single-threaded per process.
static __declspec(align(16)) uint8_t s_fpuSaveArea[108];

namespace {

struct TradeOfferItemBotshubCommand {
    uintptr_t fn;
    uint32_t item_id;
    uint32_t quantity;
};

void __cdecl TradeOfferItemCommandThunk(uint32_t itemId, uint32_t quantity) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: TradeOfferItemCommandThunk dropped item=%u qty=%u -- not initialized", itemId, quantity);
        return;
    }
    const uint32_t data[3] = { Packets::TRADE_ADD_ITEM, itemId, quantity };
    IssuePacketSend(data, sizeof(data));
}

__declspec(naked) void BotshubTradeOfferItemCommandStub() {
    __asm {
        push dword ptr [eax+8]
        push dword ptr [eax+4]
        call TradeOfferItemCommandThunk
        add esp, 8
        jmp GWA3BotshubCommandReturnThunk
    }
}

} // namespace

extern "C" void __declspec(naked) GWA3BotshubCommandReturnThunk() {
    __asm {
        pop eax
        mov ecx, dword ptr [s_botshubCmdTail]
        cmp ecx, eax
        jne skip_tail_advance
        inc eax
        mov dword ptr [s_botshubCmdTail], eax
    skip_tail_advance:
        // Restore FPU state saved at EngineDetourNaked entry.
        // Botshub commands bypass the normal exit path, so without this
        // the fld below operates on a corrupted FPU stack.
        frstor [s_fpuSaveArea]
        popfd
        popad
        // Hardcoded exit: replay original bytes inline, matching upstream
        // MainProc.  No trampoline indirection.
        mov ebp, esp
        fld dword ptr [ebp + 8]
        jmp [s_engineReturnAddr]
    }
}

static __declspec(naked) void EngineDetourNaked() {
    __asm {
        pushad
        pushfd

        // Save x87 FPU state BEFORE dispatching any commands.
        // Commands (Move, ChangeTarget, UseSkill) use floating point and
        // corrupt the FPU stack. The exit replays `fld dword ptr [ebp+8]`
        // which pushes onto the FPU stack — corruption here crashes GW
        // after ~10 minutes of active command dispatch.
        fsave [s_fpuSaveArea]

        inc dword ptr [s_heartbeat]

        // === Botshub command dispatch ===
        mov ecx, dword ptr [s_botshubCmdTail]
        cmp ecx, dword ptr [s_botshubCmdHead]
        je no_botshub_command
        mov eax, ecx
        and eax, 63
        shl eax, 8
        mov edx, offset s_botshubCmdRing
        add eax, edx
        mov edx, dword ptr [eax]
        test edx, edx
        jz no_botshub_command
        inc dword ptr [s_regularFlowExec]
        push ecx
        mov dword ptr [eax], 0
        jmp edx
    no_botshub_command:

        // Game command dispatch (rare — salvage etc.)
        mov eax, dword ptr [s_cmdTail]
        cmp eax, dword ptr [s_cmdHead]
        je no_command
        call dword ptr [s_engineDispatchCmdPtr]
    no_command:

        // Restore x87 FPU state to what it was before command dispatch.
        frstor [s_fpuSaveArea]

        popfd
        popad
        // Hardcoded exit: replay original bytes inline, matching upstream
        // MainProc.  No trampoline indirection.
        mov ebp, esp
        fld dword ptr [ebp + 8]
        jmp [s_engineReturnAddr]
    }
}

static DWORD WINAPI EngineWatchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(5);
        if (!s_engineInitialized || !s_engineHookAddr) continue;
        if (s_engineSuspended) continue;  // Don't re-patch while suspended

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
    s_engineDispatchCmdPtr = &EngineDispatchCommand;

    // Cache offset pointers for the inline asm environment gate.
    // BasePointer is already dereferenced in PostProcessOffsets.
    // Environment is a code address — read the embedded immediate to get the actual base.
    s_cachedBasePointer = Offsets::BasePointer;
    if (Offsets::Environment > 0x10000) {
        s_cachedEnvironment = *reinterpret_cast<uintptr_t*>(Offsets::Environment);
    }
    Log::Info("CtoS: env gate cached BasePointer=0x%08X Environment=0x%08X",
              s_cachedBasePointer, s_cachedEnvironment);

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


bool EnqueueGameCommand(GameCommandFn fn, const void* params, size_t paramSize) {
    if (!s_engineInitialized || !fn) return false;
    if (paramSize > sizeof(GameCommand::params)) {
        Log::Warn("CtoS: EnqueueGameCommand param too large (%u > %u)",
                  (uint32_t)paramSize, (uint32_t)sizeof(GameCommand::params));
        return false;
    }

    while (InterlockedCompareExchange(&s_cmdWriteLock, 1, 0) != 0) {
        Sleep(0);
    }

    const LONG head = s_cmdHead;
    const LONG tail = s_cmdTail;
    if (head - tail >= kGameCommandQueueSize) {
        InterlockedExchange(&s_cmdWriteLock, 0);
        Log::Warn("CtoS: EnqueueGameCommand queue full (head=%ld tail=%ld size=%ld)",
                  head, tail, kGameCommandQueueSize);
        return false;
    }

    GameCommand& cmd = s_cmdRing[head % kGameCommandQueueSize];
    ZeroMemory(cmd.params, sizeof(cmd.params));
    if (params && paramSize > 0) {
        memcpy(cmd.params, params, paramSize);
    }
    MemoryBarrier();
    cmd.fn = fn;
    MemoryBarrier();
    InterlockedExchange(&s_cmdHead, head + 1);
    InterlockedExchange(&s_cmdWriteLock, 0);

    Log::Info("CtoS: EnqueueGameCommand fn=0x%08X paramSize=%u idx=%ld",
              reinterpret_cast<uintptr_t>(fn), (uint32_t)paramSize, head);
    return true;
}

bool EnqueueBotshubCommand(const void* slot, size_t slotSize) {
    if (!s_engineInitialized || !slot || slotSize == 0u || slotSize > sizeof(BotshubCommandSlot::bytes)) {
        return false;
    }

    while (InterlockedCompareExchange(&s_botshubCmdWriteLock, 1, 0) != 0) {
        Sleep(0);
    }

    const LONG head = s_botshubCmdHead;
    const LONG tail = s_botshubCmdTail;
    if (head - tail >= kBotshubCommandQueueSize) {
        InterlockedExchange(&s_botshubCmdWriteLock, 0);
        Log::Warn("CtoS: EnqueueBotshubCommand queue full (head=%ld tail=%ld size=%ld)",
                  head, tail, kBotshubCommandQueueSize);
        return false;
    }

    BotshubCommandSlot& cmd = s_botshubCmdRing[head % kBotshubCommandQueueSize];
    ZeroMemory(cmd.bytes, sizeof(cmd.bytes));
    memcpy(cmd.bytes, slot, slotSize);
    MemoryBarrier();
    InterlockedExchange(&s_botshubCmdHead, head + 1);
    InterlockedExchange(&s_botshubCmdWriteLock, 0);

    Log::Info("CtoS: EnqueueBotshubCommand size=%u idx=%ld fn=0x%08X defer=%ld deferCmd=%ld exec=%ld hb=%ld",
              static_cast<uint32_t>(slotSize),
              head,
              static_cast<unsigned>(*reinterpret_cast<const uintptr_t*>(slot)),
              s_deferCount, s_deferWithCmd, s_regularFlowExec, s_heartbeat);
    return true;
}

bool IsBotshubQueueIdle() {
    return s_botshubCmdHead == s_botshubCmdTail;
}

bool IsBotshubCommandLaneAvailable() {
    return s_engineInitialized && !s_engineSuspended;
}

void SuspendEngineHook() {
    if (!s_engineInitialized || !s_engineHookAddr) return;
    s_engineSuspended = true;  // Tell watchdog to stop re-patching
    Sleep(10);  // Let watchdog cycle pass
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(s_engineHookAddr), s_engineSavedBytes, 5);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(s_engineHookAddr), 5);
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   oldProtect, &oldProtect);
    Log::Info("CtoS: Engine hook SUSPENDED (original bytes restored at 0x%08X)", s_engineHookAddr);
}

void ResumeEngineHook() {
    if (!s_engineInitialized || !s_engineHookAddr) return;

    // Re-read current original bytes (may have changed after map transition)
    memcpy(s_engineSavedBytes, reinterpret_cast<void*>(s_engineHookAddr), 5);
    Log::Info("CtoS: ResumeEngineHook re-read original bytes: %02X %02X %02X %02X %02X",
              s_engineSavedBytes[0], s_engineSavedBytes[1], s_engineSavedBytes[2],
              s_engineSavedBytes[3], s_engineSavedBytes[4]);

    // Rebuild trampoline with fresh original bytes
    if (s_engineReplayTrampoline) {
        uint8_t* t = reinterpret_cast<uint8_t*>(s_engineReplayTrampoline);
        memcpy(t, s_engineSavedBytes, 5);
        // JMP back to engineHookAddr + 5 (unchanged)
        t[5] = 0xE9;
        int32_t jmpRel = static_cast<int32_t>(s_engineReturnAddr - reinterpret_cast<uintptr_t>(t + 5 + 4));
        memcpy(t + 6, &jmpRel, 4);
        FlushInstructionCache(GetCurrentProcess(), t, 10);
        Log::Info("CtoS: Trampoline rebuilt with fresh bytes");
    }

    // Re-patch the engine hook
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(s_engineHookAddr), s_enginePatchedBytes, 5);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(s_engineHookAddr), 5);
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   oldProtect, &oldProtect);
    s_engineSuspended = false;
    Log::Info("CtoS: Engine hook RESUMED at 0x%08X", s_engineHookAddr);
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

    // Packet tap hook disabled — was used for opcode capture, now removed
    // to eliminate it as a crash source.

    // Install Engine inline hook for packet dispatch
    if (!InstallEngineHook()) {
        Log::Warn("CtoS: Engine hook failed -- packets will be dropped");
    }

    return true;
}

// Issue PacketSend on the current thread (must be game thread).
static void IssuePacketSend(const uint32_t* data, uint32_t sizeBytes) {
    // Re-read PacketLocation fresh every call
    uintptr_t loc = s_packetLocation;
    if (Offsets::PacketLocation) {
        uintptr_t fresh = *reinterpret_cast<uintptr_t*>(Offsets::PacketLocation);
        if (fresh) { loc = fresh; s_packetLocation = fresh; }
    }
    Log::Info("CtoS: IssuePacketSend hdr=0x%X size=%u loc=0x%08X fn=0x%08X",
              data[0], sizeBytes, static_cast<unsigned>(loc),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_packetSendFn)));
    __try {
        s_packetSendFn(reinterpret_cast<void*>(loc), sizeBytes, const_cast<uint32_t*>(data));
        Log::Info("CtoS: IssuePacketSend returned hdr=0x%X", data[0]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Error("CtoS: PacketSend exception 0x%08X hdr=0x%X",
                   GetExceptionCode(), data[0]);
    }
}

// Core send: GWCA pattern — call PacketSend on game thread only.
// If already on game thread, call directly.  Otherwise copy the packet
// and enqueue via GameThread::Enqueue.  NEVER call from a background
// thread (PacketSend is not thread-safe).
void SendPacket(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacket dropped (header=0x%X) -- not initialized", header);
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

    if (GameThread::IsOnGameThread()) {
        // Fast path: already on game thread, call directly
        IssuePacketSend(data, sizeBytes);
        return;
    }

    if (GameThread::IsInitialized()) {
        // Off game thread: copy packet data into the lambda capture and
        // enqueue for the game thread to dispatch.
        struct PktCopy { uint32_t d[12]; uint32_t sz; };
        PktCopy copy{};
        memcpy(copy.d, data, sizeBytes);
        copy.sz = sizeBytes;
        Log::Info("CtoS: SendPacket queueing on GameThread hdr=0x%X size=%u", header, sizeBytes);
        GameThread::Enqueue([copy]() {
            IssuePacketSend(copy.d, copy.sz);
        });
        return;
    }

    Log::Warn("CtoS: SendPacket dropped header=0x%X -- GameThread not ready", header);
}

void SendPacketDirect(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacketDirect dropped (header=0x%X) -- not initialized", header);
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
    Log::Info("CtoS: SendPacketDirect hdr=0x%X size=%u (bypassing engine hook)", header, sizeBytes);
    IssuePacketSend(data, sizeBytes);
}

// --- Type-safe wrappers ---


void MoveToCoord(float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(3, Packets::MOVE_TO_COORD, ix, iy);
}

void Dialog(uint32_t dialogId) {
    // Upstream Botshub/AutoIt uses HEADER_DIALOG_SEND (0x3B) for quest/NPC
    // dialog actions such as AcceptQuest and QuestReward.
    SendPacket(2, Packets::DIALOG_SEND, dialogId);
}

void ChangeTarget(uint32_t agentId) {
    SendPacket(2, Packets::TARGET_AGENT, agentId);
}

void ActionAttack(uint32_t agentId, uint32_t callTarget) {
    SendPacket(3, Packets::ACTION_ATTACK, agentId, callTarget);
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

void TradeOfferItem(uint32_t itemId, uint32_t quantity) {
    SendPacket(3, Packets::TRADE_ADD_ITEM, itemId, quantity);
}

bool TradeOfferItemBotshub(uint32_t itemId, uint32_t quantity) {
    if (!Initialize()) {
        Log::Warn("CtoS: TradeOfferItemBotshub dropped item=%u qty=%u -- not initialized", itemId, quantity);
        return false;
    }
    TradeOfferItemBotshubCommand cmd{};
    cmd.fn = reinterpret_cast<uintptr_t>(&BotshubTradeOfferItemCommandStub);
    cmd.item_id = itemId;
    cmd.quantity = quantity;
    const bool queued = EnqueueBotshubCommand(&cmd, sizeof(cmd));
    Log::Info("CtoS: TradeOfferItemBotshub item=%u qty=%u queued=%d", itemId, quantity, queued ? 1 : 0);
    return queued;
}

void TradeCancel() {
    SendPacket(1, Packets::TRADE_CANCEL);
}

void TradeAccept() {
    SendPacket(1, Packets::TRADE_ACCEPT);
}

// Stub packet tap diagnostics (referenced by IntegrationTestSession)
PacketTapSnapshot GetPacketTapSnapshot() {
    return PacketTapSnapshot{};
}

void ResetPacketTap() {
}

} // namespace GWA3::CtoS




















