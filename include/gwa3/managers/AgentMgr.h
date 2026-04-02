#pragma once

#include <gwa3/game/Agent.h>
#include <gwa3/game/GameTypes.h>
#include <cstdint>

namespace GWA3::AgentMgr {

    bool Initialize();

    // Movement
    void Move(float x, float y);

    // Targeting
    void ChangeTarget(uint32_t agentId);
    uint32_t GetTargetId();
    uint32_t GetTargetIdFromLog();
    uint32_t GetMyId();

    // Combat
    void Attack(uint32_t agentId);
    void CancelAction();
    void CallTarget(uint32_t agentId);

    // Interaction
    void InteractNPC(uint32_t agentId);
    void InteractPlayer(uint32_t agentId);
    void InteractSignpost(uint32_t agentId);

    // Agent data access (reads directly from game memory)
    Agent* GetAgentByID(uint32_t agentId);
    AgentLiving* GetMyAgent();
    AgentLiving* GetTargetAsLiving();
    uint32_t GetMaxAgents();

    // Deprecated — returns null. Use GetAgentByID + GetMaxAgents instead.
    GWArray<Agent*>* GetAgentArray();

    // Utility
    float GetDistance(float x1, float y1, float x2, float y2);
    float GetSquaredDistance(float x1, float y1, float x2, float y2);
    bool GetAgentExists(uint32_t agentId);

} // namespace GWA3::AgentMgr
