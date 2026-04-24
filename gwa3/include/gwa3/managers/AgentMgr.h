#pragma once

#include <gwa3/game/Agent.h>
#include <gwa3/game/GameTypes.h>
#include <cstdint>

namespace GWA3::AgentMgr {

    enum class NpcInteractMode : uint32_t {
        WorldActionNoCallTarget,
        WorldActionCallTarget,
        NativePostCallTarget,
        NativePostNoCallTarget,
        NativePreCallTarget,
        NativePreNoCallTarget,
        PacketNpc8,
        PacketNpc12,
    };

    bool Initialize();

    // Movement
    void Move(float x, float y);
    void ResetMoveState(const char* reason = nullptr);

    // Targeting
    void ChangeTarget(uint32_t agentId);
    void ForceChangeTarget(uint32_t agentId);
    uint32_t GetTargetId();
    uint32_t GetTargetIdFromLog();
    uint32_t GetMyId();

    // Combat
    void Attack(uint32_t agentId);
    void CancelAction();
    void CallTarget(uint32_t agentId);
    bool ActionInteract();
    bool InteractAgentWorldAction(uint32_t agentId, bool callTarget = false);

    // Interaction
    void InteractItem(uint32_t agentId, bool callTarget = false);
    void InteractNPC(uint32_t agentId);
    void InteractNPCEx(uint32_t agentId, NpcInteractMode mode);
    void InteractPlayer(uint32_t agentId);
    void InteractSignpost(uint32_t agentId);
    void InteractSignpostLegacy(uint32_t agentId);

    // Agent data access (reads directly from game memory)
    Agent* GetAgentByID(uint32_t agentId);
    AgentLiving* GetMyAgent();
    AgentLiving* GetTargetAsLiving();
    uint32_t GetMaxAgents();
    bool IsCasting(const AgentLiving* agent);

    // Resolve an agent's encoded name pointer. Mirrors GWCA's
    // AgentMgr::GetAgentEncName: look in WorldContext.players by
    // login_number for players, otherwise WorldContext.agent_infos by
    // agent_id, with WorldContext.npcs by player_number as a fallback
    // (dummy agents like "Suit of Iron Armor" live only in the NPC
    // array). Returns nullptr when the agent has no resolvable name
    // yet — the caller should NOT dereference without a null check.
    //
    // Safe to call from any thread: only reads, SEH-wrapped against
    // concurrent WorldContext shuffles on map transitions.
    wchar_t* GetAgentEncName(uint32_t agentId);
    wchar_t* GetAgentEncName(const Agent* agent);

    // Plain (non-encoded) name pointer, intended as a fallback when
    // the encoded-reference form has not yet been decoded by GW's UI.
    // Currently only resolves for PLAYER agents (reads
    // WorldContext.players[login_number].name at +0x28, which is the
    // bare account name without title decoration). Returns nullptr for
    // NPCs, gadgets, items — those sources don't carry a separately-
    // stored plain form in the structs we map today.
    wchar_t* GetAgentPlainName(const Agent* agent);

    // Utility
    float GetDistance(float x1, float y1, float x2, float y2);
    float GetSquaredDistance(float x1, float y1, float x2, float y2);
    bool GetAgentExists(uint32_t agentId);

} // namespace GWA3::AgentMgr
