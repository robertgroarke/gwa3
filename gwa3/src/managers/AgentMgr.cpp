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

enum class CallTargetType : uint32_t {
    Following = 0x3,
    Morale = 0x7,
    AttackingOrTargetting = 0xA,
    None = 0xFF
};

struct CallTargetPacket {
    CallTargetType call_type;
    uint32_t agent_id;
};

using MoveFn = void(__cdecl*)(const void*);
using ChangeTargetFn = void(__cdecl*)(uint32_t, uint32_t);
using InteractItemFn = void(__cdecl*)(uint32_t, uint32_t);
using InteractNPCFn = void(__cdecl*)(uint32_t, uint32_t);
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
static CallTargetFn s_callTargetFn = nullptr;
static bool s_initialized = false;
static bool s_loggedCurrentTargetRead = false;
static bool s_loggedCurrentTargetFault = false;
static bool s_loggedTargetLogRead = false;
static bool s_loggedTargetLogStats = false;
static bool s_loggedInteractNpcNative = false;
static bool s_loggedInteractNpcFallback = false;
static bool s_loggedMoveQueuedOnce = false;
static bool s_loggedSparkflyMoveLane = false;
static bool s_loggedChangeTargetEngineLane = false;
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

namespace {

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

    uintptr_t fn = reinterpret_cast<uintptr_t>(s_moveFn);
    const void* movePtr = move;
    __asm {
        push eax
        mov eax, movePtr
        push eax
        call dword ptr [fn]
        add esp, 4
        pop eax
    }
}

void InvokeChangeTargetRaw(uint32_t agentId) {
    if (!s_changeTargetFn) return;

    uintptr_t fn = reinterpret_cast<uintptr_t>(s_changeTargetFn);
    __asm {
        push eax
        push edx
        xor edx, edx
        push edx
        mov eax, agentId
        push eax
        call dword ptr [fn]
        add esp, 8
        pop edx
        pop eax
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

void IssueNativeMove(float x, float y) {
    // Safety: don't call native move during zone transitions or when agent is invalid.
    // The native fn crashes if called while the world state is being torn down/rebuilt.
    if (!MapMgr::GetIsMapLoaded() || GetMyId() == 0) {
        return;  // silently skip - caller will retry on next tick
    }

    MoveData moveData{};
    moveData.x = x;
    moveData.y = y;
    moveData.plane = 0;
    InvokeSparkflyMoveRaw(&moveData);
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
    if (CtoS::Initialize()) {
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
    }
    if (!GameThread::IsInitialized()) {
        Log::Warn("AgentMgr: Move falling back to packet path (GameThread not ready)");
        CtoS::MoveToCoord(x, y);
        return;
    }

    // Native movement must run on the post-dispatch game thread. GWA2 queued
    // movement through the game's dispatcher; direct off-thread calls can
    // desync animation/state and make the player glide without walking.
    if (!GameThread::IsOnGameThread()) {
        if (!s_loggedMoveQueuedOnce) {
            Log::Info("AgentMgr: Move queuing native move on GameThread post-dispatch");
            s_loggedMoveQueuedOnce = true;
        }
        GameThread::EnqueuePost([x, y]() {
            IssueNativeMove(x, y);
        });
        return;
    }

    IssueNativeMove(x, y);
}

static bool s_loggedChangeTargetSEH = false;

void ChangeTarget(uint32_t agentId) {
    if (!s_changeTargetFn) {
        Log::Warn("AgentMgr: ChangeTarget skipped — no native fn resolved");
        return;
    }

    // Dispatch via GameThread::EnqueuePost — runs AFTER the game's own
    // frame callback, outside the engine tick's lock scope.
    //
    // Why not engine hook / GameCommand / EnqueuePre:
    //   The native ChangeTarget tries to acquire a lock the engine tick
    //   already holds → deadlock ("Not Responding").
    // Why not GameThread::Enqueue (pre-dispatch):
    //   Can collide with an engine-lane Move executing in the same frame
    //   → crash after the detour returns.
    // EnqueuePost runs after the game finishes its own render-frame work,
    // so both locks are released and movement state has settled.
    if (GameThread::IsInitialized()) {
        GameThread::EnqueuePost([agentId]() {
            if (!s_changeTargetFn) return;

            // Check if the character is actively moving — if so, skip this
            // call.  The bot retries ChangeTarget on the next combat tick.
            // Calling native ChangeTarget during movement causes a crash or
            // deadlock depending on the dispatch context.
            const AgentLiving* me = GetMyAgent();
            if (me && (me->move_x != 0.0f || me->move_y != 0.0f)) {
                Log::Info("AgentMgr: ChangeTarget deferred (agent moving) agentId=%u", agentId);
                return;
            }

            __try {
                s_changeTargetFn(agentId, 0u);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                if (!s_loggedChangeTargetSEH) {
                    Log::Error("AgentMgr: ChangeTarget SEH 0x%08X agentId=%u",
                               GetExceptionCode(), agentId);
                    s_loggedChangeTargetSEH = true;
                }
            }
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
    CtoS::AttackAgent(agentId);
}

void CancelAction() {
    CtoS::CancelAction();
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
    if (s_interactNpcFn && GameThread::IsInitialized()) {
        if (!s_loggedInteractNpcNative) {
            Log::Info("AgentMgr: InteractNPC using native GameThread path fn=0x%08X",
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactNpcFn)));
            s_loggedInteractNpcNative = true;
        }
        auto fn = s_interactNpcFn;
        GameThread::EnqueuePost([fn, agentId]() {
            fn(agentId, 0u);
        });
        return;
    }

    // Current AutoIt GWA2 logic uses INTERACT_LIVING (0x38) for NPCs.
    // Keep the native path above first, but make the packet fallback match
    // the modern working merchant-interaction packet shape.
    if (!s_loggedInteractNpcFallback) {
        Log::Warn("AgentMgr: InteractNPC falling back to raw packet path");
        s_loggedInteractNpcFallback = true;
    }
    CtoS::SendPacket(3, Packets::INTERACT_LIVING, agentId, 0u);
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

GWArray<Agent*>* GetAgentArray() {
    // Legacy API — returns null because agent array is not a GWArray
    // Use GetAgentByID + GetMaxAgents instead
    return nullptr;
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
