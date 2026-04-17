#include <gwa3/managers/AgentMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/UIMgr.h>

#include <Windows.h>
#include <cmath>
#include <cstring>

namespace GWA3::AgentMgr {

static constexpr uint32_t kSendCallTargetUiMessage = 0x30000013u;
static constexpr uint32_t kSendWorldActionUiMessage = 0x30000020u;
static constexpr uint32_t kActionInteractCode = 0x80u;

enum class CallTargetType : uint32_t {
    Following = 0x3,
    Morale = 0x7,
    AttackingOrTargetting = 0xA,
    None = 0xFF
};

enum class WorldActionId : uint32_t {
    InteractEnemy = 0,
    InteractPlayerOrOther = 1,
    InteractNPC = 2,
    InteractItem = 3,
    InteractTrade = 4,
    InteractGadget = 5
};

struct CallTargetPacket {
    CallTargetType call_type;
    uint32_t agent_id;
};

struct WorldActionUIPacket {
    uint32_t action_id;
    uint32_t agent_id;
    uint32_t suppress_call_target;
};

using MoveFn = void(__cdecl*)(const void*);
using ChangeTargetFn = void(__cdecl*)(uint32_t, uint32_t);
using InteractItemFn = void(__cdecl*)(uint32_t, uint32_t);
using InteractNPCFn = void(__cdecl*)(uint32_t, uint32_t);
using WorldActionFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t);
using CallTargetFn = void(__cdecl*)(CallTargetType, uint32_t);

struct MoveData {
    float x;
    float y;
    uint32_t plane;
};

static MoveFn s_moveFn = nullptr;
static ChangeTargetFn s_changeTargetFn = nullptr;
static InteractItemFn s_interactItemFn = nullptr;
static InteractNPCFn s_interactNpcFn = nullptr;
static WorldActionFn s_worldActionFn = nullptr;
static CallTargetFn s_callTargetFn = nullptr;
static bool s_initialized = false;
static bool s_loggedCurrentTargetRead = false;
static bool s_loggedCurrentTargetFault = false;
static bool s_loggedTargetLogRead = false;
static bool s_loggedTargetLogStats = false;
static bool s_loggedInteractNpcNative = false;
static bool s_loggedInteractNpcFallback = false;
static bool s_loggedInteractNpcVariant = false;
static bool s_loggedInteractNpcWorldAction = false;
static bool s_loggedMoveQueuedOnce = false;
static bool s_loggedSparkflyMoveLane = false;
static bool s_loggedSparkflyMoveLaneUnavailable = false;
static bool s_loggedChangeTargetEngineLane = false;
static SRWLOCK s_moveQueueLock = SRWLOCK_INIT;
static MoveData s_pendingQueuedMove{};
static bool s_pendingQueuedMoveValid = false;
static volatile LONG s_moveDrainQueued = 0;
static MoveData s_lastIssuedMove{};
static DWORD s_lastIssuedMoveAt = 0;
static bool s_haveLastIssuedMove = false;
static constexpr LONG kRenderCommandSlots = 32;
static constexpr size_t kRenderCommandSlotSize = 32;
static uintptr_t s_renderCommandPool = 0;
static volatile LONG s_renderCommandSlotIndex = 0;

static uintptr_t FindNearCallTarget(uintptr_t center, int backward, int forward) {
    if (!center) return 0;
    uintptr_t start = center > static_cast<uintptr_t>(backward) ? center - backward : center;
    uintptr_t end = center + forward;
    for (uintptr_t p = start; p <= end; ++p) {
        __try {
            if (*reinterpret_cast<uint8_t*>(p) != 0xE8) continue;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
        uintptr_t fn = Scanner::FunctionFromNearCall(p);
        if (fn > 0x10000) return fn;
    }
    return 0;
}

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::Move) s_moveFn = reinterpret_cast<MoveFn>(Offsets::Move);
    if (Offsets::ChangeTarget) s_changeTargetFn = reinterpret_cast<ChangeTargetFn>(Offsets::ChangeTarget);
    uintptr_t interactAgentCall = Scanner::Find("\xC7\x45\xF0\x98\x3A\x00\x00", "xxxxxxx", 0x41);
    if (interactAgentCall) {
        uintptr_t interactAgentFn = FindNearCallTarget(interactAgentCall, 8, 8);
        if (interactAgentFn) {
            uintptr_t callTargetFn = FindNearCallTarget(interactAgentFn + 0xD6, 8, 8);
            if (callTargetFn) {
                s_callTargetFn = reinterpret_cast<CallTargetFn>(callTargetFn);
            }
            uintptr_t interactItemFn = FindNearCallTarget(interactAgentFn + 0xF8, 8, 8);
            if (!interactItemFn) {
                interactItemFn = FindNearCallTarget(interactAgentFn + 0xF0, 24, 24);
            }
            if (interactItemFn) {
                s_interactItemFn = reinterpret_cast<InteractItemFn>(interactItemFn);
            }
            uintptr_t interactNpcFn = FindNearCallTarget(interactAgentFn + 0xE7, 24, 24);
            if (interactNpcFn) {
                s_interactNpcFn = reinterpret_cast<InteractNPCFn>(interactNpcFn);
            }
            Log::Info("AgentMgr: Interact scan anchor=0x%08X agentFn=0x%08X itemFn=0x%08X",
                      static_cast<unsigned>(interactAgentCall),
                      static_cast<unsigned>(interactAgentFn),
                      static_cast<unsigned>(interactItemFn));
        }
    }

    // If local scan didn't find CallTarget, try the Offsets::CallTargetFunc from PostProcessOffsets
    if (!s_callTargetFn && Offsets::CallTargetFunc > 0x10000) {
        s_callTargetFn = reinterpret_cast<CallTargetFn>(Offsets::CallTargetFunc);
        Log::Info("AgentMgr: CallTarget resolved from Offsets::CallTargetFunc=0x%08X", Offsets::CallTargetFunc);
    }
    if (!s_worldActionFn && Offsets::WorldActionFunc > 0x10000) {
        s_worldActionFn = reinterpret_cast<WorldActionFn>(Offsets::WorldActionFunc);
        Log::Info("AgentMgr: WorldAction resolved from Offsets::WorldActionFunc=0x%08X", Offsets::WorldActionFunc);
    }
    if (!s_interactNpcFn && Offsets::InteractNPCFunc > 0x10000) {
        s_interactNpcFn = reinterpret_cast<InteractNPCFn>(Offsets::InteractNPCFunc);
        Log::Info("AgentMgr: InteractNPC resolved from Offsets::InteractNPCFunc=0x%08X", Offsets::InteractNPCFunc);
    }

    s_initialized = true;
    Log::Info("AgentMgr: Initialized (Move=0x%08X, ChangeTarget=0x%08X, CallTarget=0x%08X, InteractNPC=0x%08X, InteractItem=0x%08X)",
              Offsets::Move, Offsets::ChangeTarget,
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_callTargetFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactNpcFn)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactItemFn)));
    return true;
}

static bool s_loggedMoveOnce = false;
struct SendChangeTargetUIMsg {
    uint32_t target_id;
    uint32_t auto_target_id;
};

namespace {

void IssueNativeMove(float x, float y);

const char* NpcInteractModeName(NpcInteractMode mode) {
    switch (mode) {
    case NpcInteractMode::WorldActionNoCallTarget: return "world-action-ct0";
    case NpcInteractMode::WorldActionCallTarget: return "world-action-ct1";
    case NpcInteractMode::NativePostCallTarget: return "native-post-ct1";
    case NpcInteractMode::NativePostNoCallTarget: return "native-post-ct0";
    case NpcInteractMode::NativePreCallTarget: return "native-pre-ct1";
    case NpcInteractMode::NativePreNoCallTarget: return "native-pre-ct0";
    case NpcInteractMode::PacketNpc8: return "packet-0x39-8";
    case NpcInteractMode::PacketNpc12: return "packet-0x39-12";
    default: return "unknown";
    }
}

WorldActionId ResolveWorldActionId(uint32_t agentId) {
    auto* agent = GetAgentByID(agentId);
    if (!agent) {
        return WorldActionId::InteractPlayerOrOther;
    }
    if (agent->type == 0x400u) {
        return WorldActionId::InteractItem;
    }
    if (agent->type == 0x200u) {
        return WorldActionId::InteractGadget;
    }
    if (agent->type != 0xDBu) {
        return WorldActionId::InteractPlayerOrOther;
    }

    auto* living = static_cast<AgentLiving*>(agent);
    if (living->allegiance == 3u) {
        return WorldActionId::InteractEnemy;
    }
    if (living->allegiance == 6u) {
        return WorldActionId::InteractNPC;
    }
    return WorldActionId::InteractPlayerOrOther;
}

void InvokeWorldActionRaw(uint32_t agentId, uint32_t callTarget) {
    if (!s_worldActionFn) return;
    const auto actionId = static_cast<uint32_t>(ResolveWorldActionId(agentId));
    s_worldActionFn(actionId, agentId, callTarget);
}

bool EnsureRenderCommandPool() {
    if (s_renderCommandPool) return true;

    void* mem = VirtualAlloc(nullptr,
                             kRenderCommandSlots * kRenderCommandSlotSize,
                             MEM_RESERVE | MEM_COMMIT,
                             PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("AgentMgr: VirtualAlloc failed for render command pool");
        return false;
    }

    s_renderCommandPool = reinterpret_cast<uintptr_t>(mem);
    return true;
}

uintptr_t NextRenderCommandSlot() {
    const LONG idx = InterlockedIncrement(&s_renderCommandSlotIndex) - 1;
    return s_renderCommandPool + (static_cast<size_t>(idx % kRenderCommandSlots) * kRenderCommandSlotSize);
}

bool IsCastingState(const AgentLiving* agent) {
    if (!agent) return false;
    return agent->skill != 0u ||
           agent->model_state == 0x41u ||
           agent->model_state == 0x245u ||
           agent->model_state == 0x645u;
}

struct SparkflyMoveCommand {
    uintptr_t fn;
    MoveData move;
};

struct ChangeTargetCommand {
    uintptr_t fn;
    uint32_t agent_id;
};

__declspec(naked) void BotshubMoveCommandStub() {
    __asm {
        lea eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_moveFn]
        add esp, 4
        jmp GWA3BotshubCommandReturnThunk
    }
}

__declspec(naked) void RenderMoveCommandStub() {
    __asm {
        lea eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_moveFn]
        pop eax
        jmp GWA3CtoSHookCommandReturnThunk
    }
}

__declspec(naked) void BotshubChangeTargetCommandStub() {
    __asm {
        xor edx, edx
        push edx
        mov eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_changeTargetFn]
        add esp, 8
        jmp GWA3BotshubCommandReturnThunk
    }
}

__declspec(naked) void RenderChangeTargetCommandStub() {
    __asm {
        xor edx, edx
        push edx
        mov eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_changeTargetFn]
        add esp, 8
        jmp GWA3CtoSHookCommandReturnThunk
    }
}

void InvokeSparkflyMoveRaw(const MoveData* move) {
    if (!s_moveFn || !move) return;
    // Use inline asm to call the native Move function.
    // The direct C function pointer call (s_moveFn(move)) was crashing
    // in LLM mode — likely due to MSVC generating a call that clobbers
    // registers the game expects preserved across the call.
    uintptr_t moveAddr = reinterpret_cast<uintptr_t>(move);
    uintptr_t fn = reinterpret_cast<uintptr_t>(s_moveFn);
    __asm {
        mov eax, moveAddr
        push eax
        mov eax, fn
        call eax
        add esp, 4
    }
}

void InvokeChangeTargetRaw(uint32_t agentId) {
    if (!s_changeTargetFn) return;
    // Direct C function pointer call — the inline asm version was suspected
    // of stack corruption (same pattern as the Move fix at InvokeSparkflyMoveRaw).
    __try {
        s_changeTargetFn(agentId, 0u);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Error("AgentMgr: InvokeChangeTargetRaw exception 0x%08X agentId=%u",
                   GetExceptionCode(), agentId);
    }
}

void InvokeSparkflyMove(void* raw) {
    if (!s_moveFn || !raw) return;
    auto* cmd = reinterpret_cast<SparkflyMoveCommand*>(raw);
    if (!MapMgr::GetIsMapLoaded() || GetMyId() == 0) {
        return;
    }
    InvokeSparkflyMoveRaw(&cmd->move);
}

void InvokeChangeTarget(void* raw) {
    if (!s_changeTargetFn || !raw) return;
    const auto* cmd = reinterpret_cast<const ChangeTargetCommand*>(raw);
    InvokeChangeTargetRaw(cmd->agent_id);
}

bool ShouldSuppressMove(float x, float y) {
    if (!s_haveLastIssuedMove) {
        return false;
    }

    const float delta = std::hypot(x - s_lastIssuedMove.x, y - s_lastIssuedMove.y);
    return delta < 150.0f && (GetTickCount() - s_lastIssuedMoveAt) < 1500;
}

void DrainQueuedMove() {
    for (;;) {
        MoveData nextMove{};
        bool haveMove = false;

        AcquireSRWLockExclusive(&s_moveQueueLock);
        if (s_pendingQueuedMoveValid) {
            nextMove = s_pendingQueuedMove;
            s_pendingQueuedMoveValid = false;
            haveMove = true;
        }
        ReleaseSRWLockExclusive(&s_moveQueueLock);

        if (!haveMove) {
            break;
        }

        IssueNativeMove(nextMove.x, nextMove.y);
    }

    InterlockedExchange(&s_moveDrainQueued, 0);

    AcquireSRWLockShared(&s_moveQueueLock);
    const bool needsAnotherDrain = s_pendingQueuedMoveValid;
    ReleaseSRWLockShared(&s_moveQueueLock);

    if (needsAnotherDrain && InterlockedCompareExchange(&s_moveDrainQueued, 1, 0) == 0) {
        GameThread::EnqueuePost([]() {
            DrainQueuedMove();
        });
    }
}

void IssueNativeMove(float x, float y) {
    // Safety: don't call native move during zone transitions or when agent is invalid.
    // The native fn crashes if called while the world state is being torn down/rebuilt.
    const bool mapLoaded = MapMgr::GetIsMapLoaded();
    const uint32_t myId = GetMyId();
    Log::Info("AgentMgr: IssueNativeMove begin target=(%.0f, %.0f) map=%u loaded=%d myId=%u moveFn=0x%p",
              x,
              y,
              MapMgr::GetMapId(),
              mapLoaded ? 1 : 0,
              myId,
              s_moveFn);
    if (!mapLoaded || myId == 0) {
        Log::Info("AgentMgr: IssueNativeMove skipped because world is not ready");
        return;  // silently skip - caller will retry on next tick
    }

    // Reject obviously invalid coordinates (corrupted agent reads, NaN, etc.)
    // GW map coordinates are typically in [-30000, 30000] range.
    if (x < -50000.0f || x > 50000.0f || y < -50000.0f || y > 50000.0f ||
        x != x || y != y) { // NaN check
        Log::Warn("AgentMgr: IssueNativeMove rejected invalid coords (%.0f, %.0f)", x, y);
        return;
    }

    if (ShouldSuppressMove(x, y)) {
        return;
    }

    MoveData moveData{};
    moveData.x = x;
    moveData.y = y;
    moveData.plane = 0;
    InvokeSparkflyMoveRaw(&moveData);
    s_lastIssuedMove = moveData;
    s_lastIssuedMoveAt = GetTickCount();
    s_haveLastIssuedMove = true;
    Log::Info("AgentMgr: IssueNativeMove returned target=(%.0f, %.0f)", x, y);
}

} // namespace

bool IsCasting(const AgentLiving* agent) {
    return IsCastingState(agent);
}

void Move(float x, float y) {
    if (!s_moveFn) {
        if (!s_loggedMoveOnce) {
            Log::Info("AgentMgr: Move via CtoS (no scanned fn)");
            s_loggedMoveOnce = true;
        }
        CtoS::MoveToCoord(x, y);
        return;
    }
    if (IsCastingState(GetMyAgent())) {
        return;
    }
    if (GameThread::IsInitialized()) {
        // Prefer native post-dispatch movement once GameThread is live.
        // The Botshub lane is useful as an early fallback, but repeated route
        // movement through that queue is a crash/saturation vector.
        if (!GameThread::IsOnGameThread()) {
            if (!s_loggedMoveQueuedOnce) {
                Log::Info("AgentMgr: Move queuing native move on GameThread post-dispatch");
                s_loggedMoveQueuedOnce = true;
            }
            AcquireSRWLockExclusive(&s_moveQueueLock);
            s_pendingQueuedMove.x = x;
            s_pendingQueuedMove.y = y;
            s_pendingQueuedMove.plane = 0;
            s_pendingQueuedMoveValid = true;
            ReleaseSRWLockExclusive(&s_moveQueueLock);

            if (InterlockedCompareExchange(&s_moveDrainQueued, 1, 0) == 0) {
                GameThread::EnqueuePost([]() {
                    DrainQueuedMove();
                });
            }
            return;
        }

        IssueNativeMove(x, y);
        return;
    }

    if (CtoS::Initialize() && CtoS::IsBotshubCommandLaneAvailable()) {
        if (!s_loggedSparkflyMoveLane) {
            Log::Info("AgentMgr: Move using engine command lane");
            s_loggedSparkflyMoveLane = true;
        }
        SparkflyMoveCommand cmd{};
        cmd.fn = reinterpret_cast<uintptr_t>(&BotshubMoveCommandStub);
        cmd.move.x = x;
        cmd.move.y = y;
        cmd.move.plane = 0u;
        if (CtoS::EnqueueBotshubCommand(&cmd, sizeof(cmd))) {
            return;
        }
        Log::Warn("AgentMgr: Botshub command queue rejected move, falling back");
    } else if (CtoS::Initialize() && !s_loggedSparkflyMoveLaneUnavailable) {
        Log::Info("AgentMgr: Move skipping engine command lane because it is unavailable");
        s_loggedSparkflyMoveLaneUnavailable = true;
    }

    Log::Warn("AgentMgr: Move falling back to packet path (GameThread not ready)");
    CtoS::MoveToCoord(x, y);
}

static bool s_loggedChangeTargetDeferred = false;
static bool s_loggedChangeTargetNative = false;

void ChangeTarget(uint32_t agentId) {
    // Native ChangeTarget is stable only when stationary. Defer until
    // movement settles and the botshub move queue drains, matching the
    // upstream Move -> settle -> ChangeTarget sequencing.
    const auto* me = GetMyAgent();
    if ((me && (me->move_x != 0.0f || me->move_y != 0.0f)) || !CtoS::IsBotshubQueueIdle()) {
        if (!s_loggedChangeTargetDeferred) {
            Log::Info("AgentMgr: ChangeTarget deferred until movement settles");
            s_loggedChangeTargetDeferred = true;
        }
        return;
    }

    if (!s_loggedChangeTargetNative) {
        Log::Info("AgentMgr: ChangeTarget using native GameThread path agentId=%u", agentId);
        s_loggedChangeTargetNative = true;
    }

    if (GameThread::IsOnGameThread()) {
        InvokeChangeTargetRaw(agentId);
        return;
    }

    if (GameThread::IsInitialized()) {
        Log::Info("AgentMgr: ChangeTarget queueing native dispatch agentId=%u", agentId);
        GameThread::Enqueue([agentId]() {
            InvokeChangeTargetRaw(agentId);
        });
        return;
    }

    InvokeChangeTargetRaw(agentId);
}

uint32_t GetTargetId() {
    if (Offsets::CurrentTarget) {
        __try {
            const uint32_t value = *reinterpret_cast<uint32_t*>(Offsets::CurrentTarget);
            if (!s_loggedCurrentTargetRead) {
                Log::Info("AgentMgr: CurrentTarget ptr=0x%08X value=%u",
                          static_cast<unsigned>(Offsets::CurrentTarget), value);
                s_loggedCurrentTargetRead = true;
            }
            if (value != 0) return value;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            if (!s_loggedCurrentTargetFault) {
                Log::Warn("AgentMgr: CurrentTarget read fault at 0x%08X",
                          static_cast<unsigned>(Offsets::CurrentTarget));
                s_loggedCurrentTargetFault = true;
            }
        }
    }

    return GetTargetIdFromLog();
}

uint32_t GetTargetIdFromLog() {
    const uint32_t myId = GetMyId();
    if (!myId) return 0;

    if (!s_loggedTargetLogStats) {
        Log::Info("AgentMgr: TargetLog stats calls=%u stores=%u",
                  TargetLogHook::GetCallCount(),
                  TargetLogHook::GetStoreCount());
        s_loggedTargetLogStats = true;
    }

    const uint32_t value = TargetLogHook::GetTarget(myId);
    if (value && !s_loggedTargetLogRead) {
        Log::Info("AgentMgr: TargetLog[MyID=%u] = %u", myId, value);
        s_loggedTargetLogRead = true;
    }
    return value;
}

uint32_t GetMyId() {
    if (!Offsets::MyID) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::MyID);
}

void Attack(uint32_t agentId) {
    static bool s_loggedActionAttack = false;
    if (!s_loggedActionAttack) {
        s_loggedActionAttack = true;
        Log::Info("AgentMgr: Attack using ACTION_ATTACK packet path with call-target");
    }
    CtoS::ActionAttack(agentId, 1u);
}

void CancelAction() {
    CtoS::CancelAction();
}

bool ActionInteract() {
    if (!UIMgr::HasControlActionKeypress()) {
        Log::Warn("AgentMgr: ActionInteract unavailable (DoAction/action context missing)");
        return false;
    }
    return UIMgr::ActionKeyPress(kActionInteractCode);
}

void CallTarget(uint32_t agentId) {
    // Packet path — proven working, matches AutoIt: SendPacket(0xC, CALL_TARGET, 0xA, agentId)
    // Native function path is resolved but the dispatcher+offset may not point to the
    // correct CallTarget sub-function in all GW builds. See GWA3-090 kanban ticket.
    CtoS::SendPacket(3, Packets::CALL_TARGET,
                     static_cast<uint32_t>(CallTargetType::AttackingOrTargetting),
                     agentId);
}

void InteractItem(uint32_t agentId, bool callTarget) {
    if (!s_interactItemFn) {
        CtoS::PickUpItem(agentId);
        return;
    }
    if (!GameThread::IsInitialized()) {
        Log::Warn("AgentMgr: InteractItem falling back to packet path (GameThread not ready)");
        CtoS::PickUpItem(agentId);
        return;
    }

    auto fn = s_interactItemFn;
    const uint32_t ct = callTarget ? 1u : 0u;
    GameThread::Enqueue([fn, agentId, ct]() {
        fn(agentId, ct);
    });
}

void InteractNPC(uint32_t agentId) {
    InteractNPCEx(agentId, NpcInteractMode::NativePostCallTarget);
}

void InteractNPCEx(uint32_t agentId, NpcInteractMode mode) {
    const bool worldActionMode =
        mode == NpcInteractMode::WorldActionNoCallTarget ||
        mode == NpcInteractMode::WorldActionCallTarget;
    if (worldActionMode && GameThread::IsInitialized() && Offsets::UIMessage > 0x10000) {
        const uint32_t ct = mode == NpcInteractMode::WorldActionCallTarget ? 1u : 0u;
        if (!s_loggedInteractNpcWorldAction) {
            Log::Info("AgentMgr: InteractNPC using WorldAction UI message path msg=0x%08X",
                      kSendWorldActionUiMessage);
            s_loggedInteractNpcWorldAction = true;
        }
        if (!s_loggedInteractNpcVariant) {
            Log::Info("AgentMgr: InteractNPC variant mode=%s msg=0x%08X",
                      NpcInteractModeName(mode),
                      kSendWorldActionUiMessage);
            s_loggedInteractNpcVariant = true;
        }
        GameThread::Enqueue([agentId, ct]() {
            WorldActionUIPacket packet{
                static_cast<uint32_t>(ResolveWorldActionId(agentId)),
                agentId,
                ct
            };
            UIMgr::SendUIMessage(kSendWorldActionUiMessage, &packet, nullptr);
        });
        return;
    }

    const bool nativeMode =
        mode == NpcInteractMode::NativePostCallTarget ||
        mode == NpcInteractMode::NativePostNoCallTarget ||
        mode == NpcInteractMode::NativePreCallTarget ||
        mode == NpcInteractMode::NativePreNoCallTarget;
    if (nativeMode && s_interactNpcFn && GameThread::IsInitialized()) {
        const uint32_t ct =
            (mode == NpcInteractMode::NativePostCallTarget ||
             mode == NpcInteractMode::NativePreCallTarget) ? 1u : 0u;
        if (!s_loggedInteractNpcNative) {
            Log::Info("AgentMgr: InteractNPC using native GameThread path fn=0x%08X callTarget=1",
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactNpcFn)));
            s_loggedInteractNpcNative = true;
        }
        if (!s_loggedInteractNpcVariant && mode != NpcInteractMode::NativePostCallTarget) {
            Log::Info("AgentMgr: InteractNPC variant mode=%s fn=0x%08X",
                      NpcInteractModeName(mode),
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactNpcFn)));
            s_loggedInteractNpcVariant = true;
        }
        auto fn = s_interactNpcFn;
        if (mode == NpcInteractMode::NativePreCallTarget ||
            mode == NpcInteractMode::NativePreNoCallTarget) {
            GameThread::Enqueue([fn, agentId, ct]() {
                fn(agentId, ct);
            });
        } else {
            GameThread::EnqueuePost([fn, agentId, ct]() {
                fn(agentId, ct);
            });
        }
        return;
    }

    if (!s_loggedInteractNpcFallback) {
        Log::Warn("AgentMgr: InteractNPC falling back to raw packet path");
        s_loggedInteractNpcFallback = true;
    }
    if (!s_loggedInteractNpcVariant && mode != NpcInteractMode::NativePostCallTarget) {
        Log::Info("AgentMgr: InteractNPC packet variant mode=%s", NpcInteractModeName(mode));
        s_loggedInteractNpcVariant = true;
    }

    if (mode == NpcInteractMode::PacketNpc12) {
        CtoS::SendPacket(3, Packets::INTERACT_NPC, agentId, 0u);
        return;
    }
    CtoS::SendPacket(3, Packets::INTERACT_NPC, agentId, 0u);
}

void InteractPlayer(uint32_t agentId) {
    CtoS::SendPacket(2, Packets::INTERACT_PLAYER, agentId);
}

void InteractSignpost(uint32_t agentId) {
    CtoS::SendPacket(3, Packets::SIGNPOST_RUN, agentId, 0u);
}

// Agent access via flat pointer chain: *AgentBase = agent_ptr_array, *(AgentBase+8) = maxAgents
Agent* GetAgentByID(uint32_t agentId) {
    if (!Offsets::AgentBase || agentId == 0) return nullptr;
    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        if (agentArr <= 0x10000) return nullptr;
        uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentId >= maxAgents) return nullptr;
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + agentId * 4);
        if (agentPtr <= 0x10000) return nullptr;
        return reinterpret_cast<Agent*>(agentPtr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

AgentLiving* GetMyAgent() {
    uint32_t id = GetMyId();
    if (!id) return nullptr;
    Agent* agent = GetAgentByID(id);
    if (!agent) return nullptr;
    // Check type == 0xDB (Living), but also accept if type field is valid
    if (agent->type == 0xDB) return static_cast<AgentLiving*>(agent);
    // In GW Reforged the type field may differ — check at struct level
    return nullptr;
}

AgentLiving* GetTargetAsLiving() {
    uint32_t id = GetTargetId();
    if (!id) return nullptr;
    Agent* agent = GetAgentByID(id);
    if (!agent || agent->type != 0xDB) return nullptr;
    return static_cast<AgentLiving*>(agent);
}

uint32_t GetMaxAgents() {
    if (!Offsets::AgentBase) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

float GetSquaredDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;
}

float GetDistance(float x1, float y1, float x2, float y2) {
    return sqrtf(GetSquaredDistance(x1, y1, x2, y2));
}

bool GetAgentExists(uint32_t agentId) {
    return GetAgentByID(agentId) != nullptr;
}

} // namespace GWA3::AgentMgr
