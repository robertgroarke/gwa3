#include <gwa3/dungeon/DungeonDiagnostics.h>

#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>

#include <Windows.h>
#include <cmath>

namespace GWA3::DungeonDiagnostics {

std::size_t CollectNearbyNpcCandidates(
    float x,
    float y,
    float maxDist,
    NearbyNpcCandidate* out,
    std::size_t maxOut) {
    if (!out || maxOut == 0) return 0;
    for (std::size_t i = 0; i < maxOut; ++i) {
        out[i] = {};
    }

    auto* me = AgentMgr::GetMyAgent();
    const float meX = me ? me->x : x;
    const float meY = me ? me->y : y;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    const float maxDistSq = maxDist * maxDist;
    std::size_t count = 0;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDB) continue;

        NearbyNpcCandidate candidate = {};
        __try {
            auto* living = static_cast<AgentLiving*>(agent);
            if (living->allegiance != 6u) continue;
            if (living->hp <= 0.0f) continue;
            if ((living->effects & 0x0010u) != 0u) continue;

            const float distSq = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
            if (distSq > maxDistSq) continue;

            candidate.agentId = living->agent_id;
            candidate.playerNumber = living->player_number;
            candidate.npcId = living->transmog_npc_id;
            candidate.effects = living->effects;
            candidate.x = living->x;
            candidate.y = living->y;
            candidate.distanceToSearch = sqrtf(distSq);
            candidate.distanceToPlayer = AgentMgr::GetDistance(meX, meY, living->x, living->y);
            candidate.score = static_cast<uint32_t>(candidate.distanceToSearch);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }

        std::size_t insertAt = maxOut;
        for (std::size_t slot = 0; slot < maxOut; ++slot) {
            if (out[slot].agentId == 0 || candidate.score < out[slot].score) {
                insertAt = slot;
                break;
            }
        }
        if (insertAt == maxOut) continue;

        for (std::size_t slot = maxOut - 1; slot > insertAt; --slot) {
            out[slot] = out[slot - 1];
        }
        out[insertAt] = candidate;
        if (count < maxOut) ++count;
    }

    return count;
}

void LogNearbyNpcCandidates(
    const char* label,
    float x,
    float y,
    float maxDist,
    NearbyNpcCandidate* candidates,
    std::size_t count) {
    Log::Info("DungeonDiagnostics: %s NPC candidates near (%.0f, %.0f) within %.0f: %u",
              label,
              x,
              y,
              maxDist,
              static_cast<unsigned>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const auto& c = candidates[i];
        Log::Info("DungeonDiagnostics:   cand[%u]: agent=%u player=%u npc_id=%u effects=0x%08X searchDist=%.0f playerDist=%.0f pos=(%.0f, %.0f)",
                  static_cast<unsigned>(i),
                  c.agentId,
                  c.playerNumber,
                  c.npcId,
                  c.effects,
                  c.distanceToSearch,
                  c.distanceToPlayer,
                  c.x,
                  c.y);
    }
}

void LogAgentIdentity(const char* label, uint32_t agentId) {
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (!agent) {
        Log::Info("DungeonDiagnostics: %s agent=%u <null>", label, agentId);
        return;
    }

    __try {
        Log::Info("DungeonDiagnostics: %s agent=%u type=0x%X pos=(%.0f, %.0f)",
                  label,
                  agentId,
                  agent->type,
                  agent->x,
                  agent->y);
        if (agent->type == 0xDB) {
            auto* living = static_cast<AgentLiving*>(agent);
            Log::Info("DungeonDiagnostics: %s living agent=%u allegiance=%u hp=%.2f effects=0x%08X player=%u npc_id=%u model_type=%u",
                      label,
                      agentId,
                      living->allegiance,
                      living->hp,
                      living->effects,
                      living->player_number,
                      living->transmog_npc_id,
                      living->agent_model_type);
        } else if (agent->type == 0x200) {
            auto* gadget = static_cast<AgentGadget*>(agent);
            Log::Info("DungeonDiagnostics: %s gadget agent=%u gadget_id=%u extra_type=%u",
                      label,
                      agentId,
                      gadget->gadget_id,
                      gadget->extra_type);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Info("DungeonDiagnostics: %s agent=%u <read fault>", label, agentId);
    }
}

void LogNearbySignposts(float x, float y, float maxDist, const char* label, bool chestOnly) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    if (maxAgents == 0) {
        Log::Info("DungeonDiagnostics: %s maxAgents=0", label);
        return;
    }

    uint32_t matches = 0;
    uint32_t logged = 0;
    const float maxDistSq = maxDist * maxDist;
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0x200) continue;
        auto* gadget = static_cast<const AgentGadget*>(a);
        if (chestOnly && !DungeonInteractions::IsChestGadgetId(gadget->gadget_id)) continue;
        const float distSq = AgentMgr::GetSquaredDistance(x, y, a->x, a->y);
        if (distSq > maxDistSq) continue;
        ++matches;
        if (logged < 12) {
            Log::Info("DungeonDiagnostics: %s agent=%u pos=(%.0f, %.0f) dist=%.0f gadget=%u extra=%u chest=%d",
                      label,
                      a->agent_id,
                      a->x,
                      a->y,
                      sqrtf(distSq),
                      gadget->gadget_id,
                      gadget->extra_type,
                      DungeonInteractions::IsChestGadgetId(gadget->gadget_id) ? 1 : 0);
            ++logged;
        }
    }

    Log::Info("DungeonDiagnostics: %s matches=%u logged=%u radius=%.0f chestOnly=%d",
              label,
              matches,
              logged,
              maxDist,
              chestOnly ? 1 : 0);
}

} // namespace GWA3::DungeonDiagnostics
