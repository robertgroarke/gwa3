#include <gwa3/managers/AgentMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cmath>
#include <cstring>

namespace GWA3::AgentMgr {

using MoveFn = void(__cdecl*)(const void*);
using ChangeTargetFn = void(__cdecl*)(uint32_t, uint32_t);
using InteractItemFn = void(__cdecl*)(uint32_t, uint32_t);

struct MoveData {
    float x;
    float y;
    uint32_t plane;
};

// Ring buffers to prevent shellcode overwrite during rapid commands
static constexpr int kShellcodeSlots = 16;
static constexpr int kMoveSlotSize = 64; // shellcode + data in one slot
static constexpr int kTargetSlotSize = 32;
static uintptr_t s_moveShellcodeBase = 0;
static uintptr_t s_targetShellcodeBase = 0;
static volatile LONG s_moveSlotIndex = 0;
static volatile LONG s_targetSlotIndex = 0;

static MoveFn s_moveFn = nullptr;
static ChangeTargetFn s_changeTargetFn = nullptr;
static InteractItemFn s_interactItemFn = nullptr;
static bool s_initialized = false;
static bool s_loggedCurrentTargetRead = false;
static bool s_loggedCurrentTargetFault = false;
static bool s_loggedTargetLogRead = false;
static bool s_loggedTargetLogStats = false;

static bool EnsureMoveShellcode() {
    if (s_moveShellcodeBase) return true;

    void* mem = VirtualAlloc(nullptr, kShellcodeSlots * kMoveSlotSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("AgentMgr: VirtualAlloc failed for move shellcode pool");
        return false;
    }

    s_moveShellcodeBase = reinterpret_cast<uintptr_t>(mem);
    return true;
}

static bool EnsureTargetShellcode() {
    if (s_targetShellcodeBase) return true;

    void* mem = VirtualAlloc(nullptr, kShellcodeSlots * kTargetSlotSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("AgentMgr: VirtualAlloc failed for target shellcode pool");
        return false;
    }

    s_targetShellcodeBase = reinterpret_cast<uintptr_t>(mem);
    return true;
}

static uintptr_t NextMoveSlot() {
    LONG idx = InterlockedIncrement(&s_moveSlotIndex) % kShellcodeSlots;
    return s_moveShellcodeBase + idx * kMoveSlotSize;
}

static uintptr_t NextTargetSlot() {
    LONG idx = InterlockedIncrement(&s_targetSlotIndex) % kShellcodeSlots;
    return s_targetShellcodeBase + idx * kTargetSlotSize;
}

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
            uintptr_t interactItemFn = FindNearCallTarget(interactAgentFn + 0xF8, 8, 8);
            if (!interactItemFn) {
                interactItemFn = FindNearCallTarget(interactAgentFn + 0xF0, 24, 24);
            }
            if (interactItemFn) {
                s_interactItemFn = reinterpret_cast<InteractItemFn>(interactItemFn);
            }
            Log::Info("AgentMgr: Interact scan anchor=0x%08X agentFn=0x%08X itemFn=0x%08X",
                      static_cast<unsigned>(interactAgentCall),
                      static_cast<unsigned>(interactAgentFn),
                      static_cast<unsigned>(interactItemFn));
        }
    }

    s_initialized = true;
    Log::Info("AgentMgr: Initialized (Move=0x%08X, ChangeTarget=0x%08X, InteractItem=0x%08X)",
              Offsets::Move, Offsets::ChangeTarget, static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactItemFn)));
    return true;
}

static bool s_loggedMoveOnce = false;

void Move(float x, float y) {
    if (!s_moveFn) {
        if (!s_loggedMoveOnce) {
            Log::Info("AgentMgr: Move via CtoS (no scanned fn)");
            s_loggedMoveOnce = true;
        }
        CtoS::MoveToCoord(x, y);
        return;
    }
    if (!GameThread::IsInitialized()) {
        Log::Warn("AgentMgr: Move falling back to packet path (GameThread not ready)");
        CtoS::MoveToCoord(x, y);
        return;
    }

    // Direct function call on the game thread. IMPORTANT: the caller must
    // ensure this is called from within the game thread context (e.g., via
    // GameThread::Enqueue wrapping the call). The fn() only works during
    // the game's own frame callback execution.
    static MoveData s_moveData;
    s_moveData.x = x;
    s_moveData.y = y;
    s_moveData.plane = 0;
    s_moveFn(&s_moveData);
}

void ChangeTarget(uint32_t agentId) {
    if (!s_changeTargetFn) {
        CtoS::ChangeTarget(agentId);
        return;
    }
    if (!GameThread::IsInitialized()) {
        Log::Warn("AgentMgr: ChangeTarget falling back to packet path");
        CtoS::ChangeTarget(agentId);
        return;
    }

    auto fn = s_changeTargetFn;
    GameThread::EnqueuePost([fn, agentId]() {
        fn(agentId, 0);
    });
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
    CtoS::SendPacket(2, Packets::CALL_TARGET, agentId);
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
    CtoS::SendPacket(2, Packets::INTERACT_NPC, agentId);
}

void InteractPlayer(uint32_t agentId) {
    CtoS::SendPacket(2, Packets::INTERACT_PLAYER, agentId);
}

void InteractSignpost(uint32_t agentId) {
    CtoS::SendPacket(2, Packets::SIGNPOST_RUN, agentId);
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
