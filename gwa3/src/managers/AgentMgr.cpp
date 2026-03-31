#include <gwa3/managers/AgentMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <cmath>
#include <cstring>

namespace GWA3::AgentMgr {

// Game function signatures from scanned offsets
using MoveFn = void(__cdecl*)(float, float);
using ChangeTargetFn = void(__cdecl*)(uint32_t);

static MoveFn s_moveFn = nullptr;
static ChangeTargetFn s_changeTargetFn = nullptr;
static bool s_initialized = false;

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
    if (s_moveFn) {
        GameThread::Enqueue([x, y]() { s_moveFn(x, y); });
    } else {
        CtoS::MoveToCoord(x, y);
    }
}

void ChangeTarget(uint32_t agentId) {
    if (s_changeTargetFn) {
        GameThread::Enqueue([agentId]() { s_changeTargetFn(agentId); });
    } else {
        CtoS::ChangeTarget(agentId);
    }
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
