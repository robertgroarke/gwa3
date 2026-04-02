#include <gwa3/managers/AgentMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cmath>
#include <cstring>

namespace GWA3::AgentMgr {

using MoveFn = void(__cdecl*)(const void*);
using ChangeTargetFn = void(__cdecl*)(uint32_t, uint32_t);

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

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::Move) s_moveFn = reinterpret_cast<MoveFn>(Offsets::Move);
    if (Offsets::ChangeTarget) s_changeTargetFn = reinterpret_cast<ChangeTargetFn>(Offsets::ChangeTarget);

    s_initialized = true;
    Log::Info("AgentMgr: Initialized (Move=0x%08X, ChangeTarget=0x%08X)",
              Offsets::Move, Offsets::ChangeTarget);
    return true;
}

void Move(float x, float y) {
    if (!s_moveFn) {
        CtoS::MoveToCoord(x, y);
        return;
    }
    if (!RenderHook::IsInitialized() || !EnsureMoveShellcode()) {
        Log::Warn("AgentMgr: Move falling back to packet path");
        CtoS::MoveToCoord(x, y);
        return;
    }

    uintptr_t slot = NextMoveSlot();
    // Layout: [0..15] shellcode, [32..43] MoveData
    uintptr_t dataAddr = slot + 32;

    MoveData move{x, y, 0};
    memcpy(reinterpret_cast<void*>(dataAddr), &move, sizeof(move));

    auto* sc = reinterpret_cast<uint8_t*>(slot);
    sc[0] = 0xB8; // mov eax, imm32
    uint32_t dataAddr32 = static_cast<uint32_t>(dataAddr);
    memcpy(sc + 1, &dataAddr32, sizeof(uint32_t));
    sc[5] = 0x50; // push eax
    sc[6] = 0xE8; // call rel32
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(s_moveFn) - (slot + 11));
    memcpy(sc + 7, &rel, sizeof(rel));
    sc[11] = 0x58; // pop eax
    sc[12] = 0xC3; // ret
    FlushInstructionCache(GetCurrentProcess(), sc, 13);

    RenderHook::EnqueueCommand(slot);
}

void ChangeTarget(uint32_t agentId) {
    if (!s_changeTargetFn) {
        CtoS::ChangeTarget(agentId);
        return;
    }
    if (!RenderHook::IsInitialized() || !EnsureTargetShellcode()) {
        Log::Warn("AgentMgr: ChangeTarget falling back to packet path");
        CtoS::ChangeTarget(agentId);
        return;
    }

    uintptr_t slot = NextTargetSlot();
    auto* sc = reinterpret_cast<uint8_t*>(slot);
    sc[0] = 0x33; // xor edx, edx
    sc[1] = 0xD2;
    sc[2] = 0x52; // push edx
    sc[3] = 0xB8; // mov eax, imm32
    memcpy(sc + 4, &agentId, sizeof(agentId));
    sc[8] = 0x50; // push eax
    sc[9] = 0xE8; // call rel32
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(s_changeTargetFn) - (slot + 14));
    memcpy(sc + 10, &rel, sizeof(rel));
    sc[14] = 0x83; // add esp, 8
    sc[15] = 0xC4;
    sc[16] = 0x08;
    sc[17] = 0xC3; // ret
    FlushInstructionCache(GetCurrentProcess(), sc, 18);

    RenderHook::EnqueueCommand(slot);
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

void InteractNPC(uint32_t agentId) {
    CtoS::SendPacket(2, Packets::INTERACT_NPC, agentId);
}

void InteractPlayer(uint32_t agentId) {
    CtoS::SendPacket(2, Packets::INTERACT_PLAYER, agentId);
}

void InteractSignpost(uint32_t agentId) {
    CtoS::SendPacket(2, Packets::SIGNPOST_RUN, agentId);
}

Agent* GetAgentByID(uint32_t agentId) {
    auto* arr = GetAgentArray();
    if (!arr || !arr->buffer || agentId >= arr->size) return nullptr;
    return arr->buffer[agentId];
}

AgentLiving* GetMyAgent() {
    uint32_t id = GetMyId();
    if (!id) return nullptr;
    Agent* agent = GetAgentByID(id);
    if (!agent || agent->type != 0xDB) return nullptr; // 0xDB = Living
    return static_cast<AgentLiving*>(agent);
}

AgentLiving* GetTargetAsLiving() {
    uint32_t id = GetTargetId();
    if (!id) return nullptr;
    Agent* agent = GetAgentByID(id);
    if (!agent || agent->type != 0xDB) return nullptr;
    return static_cast<AgentLiving*>(agent);
}

GWArray<Agent*>* GetAgentArray() {
    if (!Offsets::AgentBase) return nullptr;
    auto* ptr = reinterpret_cast<GWArray<Agent*>**>(Offsets::AgentBase);
    return ptr ? *ptr : nullptr;
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
