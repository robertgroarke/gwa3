#include <gwa3/managers/AgentMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
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

static uintptr_t s_moveShellcode = 0;
static uintptr_t s_moveDataAddr = 0;
static uintptr_t s_targetShellcode = 0;

static MoveFn s_moveFn = nullptr;
static ChangeTargetFn s_changeTargetFn = nullptr;
static bool s_initialized = false;

static bool EnsureMoveShellcode() {
    if (s_moveShellcode && s_moveDataAddr) return true;

    void* mem = VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("AgentMgr: VirtualAlloc failed for move shellcode");
        return false;
    }

    s_moveShellcode = reinterpret_cast<uintptr_t>(mem);
    s_moveDataAddr = s_moveShellcode + 32;
    return true;
}

static bool EnsureTargetShellcode() {
    if (s_targetShellcode) return true;

    void* mem = VirtualAlloc(nullptr, 32, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("AgentMgr: VirtualAlloc failed for target shellcode");
        return false;
    }

    s_targetShellcode = reinterpret_cast<uintptr_t>(mem);
    return true;
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

    MoveData move{x, y, 0};
    memcpy(reinterpret_cast<void*>(s_moveDataAddr), &move, sizeof(move));

    auto* sc = reinterpret_cast<uint8_t*>(s_moveShellcode);
    sc[0] = 0xB8; // mov eax, imm32
    memcpy(sc + 1, &s_moveDataAddr, sizeof(uint32_t));
    sc[5] = 0x50; // push eax
    sc[6] = 0xE8; // call rel32
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(s_moveFn) - (s_moveShellcode + 11));
    memcpy(sc + 7, &rel, sizeof(rel));
    sc[11] = 0x58; // pop eax
    sc[12] = 0xC3; // ret
    FlushInstructionCache(GetCurrentProcess(), sc, 13);

    RenderHook::EnqueueCommand(s_moveShellcode);
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

    auto* sc = reinterpret_cast<uint8_t*>(s_targetShellcode);
    sc[0] = 0x33; // xor edx, edx
    sc[1] = 0xD2;
    sc[2] = 0x52; // push edx
    sc[3] = 0xB8; // mov eax, imm32
    memcpy(sc + 4, &agentId, sizeof(agentId));
    sc[8] = 0x50; // push eax
    sc[9] = 0xE8; // call rel32
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(s_changeTargetFn) - (s_targetShellcode + 14));
    memcpy(sc + 10, &rel, sizeof(rel));
    sc[14] = 0x83; // add esp, 8
    sc[15] = 0xC4;
    sc[16] = 0x08;
    sc[17] = 0xC3; // ret
    FlushInstructionCache(GetCurrentProcess(), sc, 18);

    RenderHook::EnqueueCommand(s_targetShellcode);
}

uint32_t GetTargetId() {
    if (!Offsets::CurrentTarget) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::CurrentTarget);
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
