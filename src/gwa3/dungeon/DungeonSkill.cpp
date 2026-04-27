#include <gwa3/dungeon/DungeonSkill.h>

#include <gwa3/game/SkillIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <cstddef>
#include <cstring>

namespace GWA3::DungeonSkill {

namespace {

constexpr uint32_t DEBUFF_DIVERSION            = 11u;
constexpr uint32_t DEBUFF_BACKFIRE            = 73u;
constexpr uint32_t DEBUFF_SOUL_LEECH          = 844u;
constexpr uint32_t DEBUFF_MISTRUST            = 2065u;
constexpr uint32_t DEBUFF_VISIONS_OF_REGRET   = 2042u;
constexpr uint32_t DEBUFF_MARK_OF_SUBVERSION  = 654u;
constexpr uint32_t DEBUFF_SPITEFUL_SPIRIT     = 653u;
constexpr uint32_t DEBUFF_INEPTITUDE          = 60u;
constexpr uint32_t DEBUFF_CLUMSINESS          = 51u;
constexpr uint32_t DEBUFF_WANDERING_EYE       = 1039u;
constexpr uint32_t DEBUFF_WELL_OF_SILENCE     = 668u;
constexpr uint32_t DEBUFF_IGNORANCE           = 56u;
constexpr uint32_t EFFECT_QUICKENING_ZEPHYR   = 475u;
constexpr uint32_t EFFECTS_SKIP_AGGRO_FOE     = 0x0014u;
constexpr uint32_t TYPE_MAP_VANISHED_HOSTILE_MINION = 262152u;

bool IsHardInterruptId(uint32_t id) {
    switch (id) {
    case 5: case 64: case 99: case 116: case 170:
    case 218: case 312: case 332: case 782: case 783:
    case 838: case 950: case 1041: case 1338: case 1489:
    case 2013: case 2162: case 2370:
        return true;
    default:
        return false;
    }
}

bool IsCondRemovalId(uint32_t id) {
    switch (id) {
    case 25: case 31: case 53: case 222: case 270:
    case 280: case 289: case 291: case 331: case 838:
    case 951: case 1258: case SkillIds::SHRINKING_ARMOR: case 2059: case 2145:
    case 2179: case 2286: case 2362: case 2451:
        return true;
    default:
        return false;
    }
}

bool IsHexRemovalId(uint32_t id) {
    switch (id) {
    case 24: case 25: case 156: case 270: case 280:
    case 304: case 331: case 838: case 944: case 1258:
    case 2145: case 2179: case 2451:
        return true;
    default:
        return false;
    }
}

bool IsSurvivalId(uint32_t id) {
    switch (id) {
    case 2358: case 312: case 826: case 828: case 867:
    case 878: case 1338: case 2370: case 2371:
        return true;
    default:
        return false;
    }
}

bool IsSpeedBoostId(uint32_t id) {
    switch (id) {
    case 312: case 452: case 826: case 828: case 856:
    case 867: case 878: case 947: case 1003: case 1338:
    case 2370: case 2371: case SkillIds::SUMMON_SPIRITS_LUXON: case SkillIds::SHADOW_FANG: case SkillIds::HEROIC_REFRAIN:
        return true;
    default:
        return false;
    }
}

bool IsBindingId(uint32_t id) {
    switch (id) {
    case 2233:
    case 2100:
    case 1228:
    case 1232:
    case 1238:
    case 1239:
    case 1253:
    case 2110:
    case 1742:
    case 1217:
    case 1240:
    case 1884:
    case 786: case 787: case 788: case 789: case 790:
    case 791: case 792: case 793: case 794: case 795:
    case 960: case 961: case 962: case 963: case 964:
    case 965: case 966: case 967: case 2083: case 2084:
    case 2085: case 2087: case 2088: case 2089:
        return true;
    default:
        return false;
    }
}

bool IsPressureSpiritId(uint32_t id) {
    switch (id) {
    case 1239:
    case 1253:
    case 2110:
    case 2233:
        return true;
    default:
        return false;
    }
}

bool IsPrecastId(uint32_t id) {
    if (IsPressureSpiritId(id)) return true;
    switch (id) {
    case 1230:
    case 1232:
    case 1228:
    case 2100:
        return true;
    default:
        return false;
    }
}

uint32_t CountAgentEffectsByPredicate(uint32_t agentId, bool (*predicate)(const Skill*)) {
    if (agentId == 0u || predicate == nullptr) return 0u;
    auto* effectArray = EffectMgr::GetAgentEffectArray(agentId);
    if (!effectArray || !effectArray->buffer) return 0u;

    uint32_t matches = 0u;
    for (uint32_t i = 0u; i < effectArray->size; ++i) {
        const Effect& effect = effectArray->buffer[i];
        if (effect.skill_id == 0u) continue;
        const Skill* effectSkill = SkillMgr::GetSkillConstantData(effect.skill_id);
        if (!effectSkill) continue;
        if (predicate(effectSkill)) {
            ++matches;
        }
    }
    return matches;
}

bool IsHexEffectSkill(const Skill* skill) {
    return skill && skill->type == 1u;
}

bool IsConditionEffectSkill(const Skill* skill) {
    return skill && skill->condition != 0u;
}

uint32_t GetMostAffectedAlly(bool excludeSelf,
                             uint32_t (*scoreFn)(uint32_t),
                             float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || !scoreFn) return 0u;

    uint32_t bestId = 0u;
    uint32_t bestScore = 0u;
    float bestHp = 1.1f;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 1u || living->hp <= 0.0f) continue;
        if (excludeSelf && living->agent_id == me->agent_id) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq > maxRange * maxRange) continue;

        const uint32_t score = scoreFn(living->agent_id);
        if (score == 0u) continue;
        if (score > bestScore || (score == bestScore && living->hp < bestHp)) {
            bestScore = score;
            bestHp = living->hp;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

bool HasEnchantment(uint32_t agentId) {
    auto* agentEffects = EffectMgr::GetAgentEffects(agentId);
    if (!agentEffects || !agentEffects->effects.buffer) return false;
    for (uint32_t effectIndex = 0u; effectIndex < agentEffects->effects.size; ++effectIndex) {
        const auto& effect = agentEffects->effects.buffer[effectIndex];
        if (effect.skill_id == 0u) continue;
        const auto* skillData = SkillMgr::GetSkillConstantData(effect.skill_id);
        if (skillData && (skillData->type == 3u || skillData->type == 16u)) {
            return true;
        }
    }
    return false;
}

bool IsValidAggroEnemy(const AgentLiving* me,
                       const AgentLiving* living,
                       float maxRange,
                       bool castingOnly,
                       bool noHexOnly,
                       bool enchantedOnly) {
    if (!me || !living) return false;
    if (living->allegiance != 3u) return false;
    if (living->agent_id == 0u || living->agent_id == me->agent_id) return false;
    if (living->hp <= 0.0f) return false;
    if ((living->effects & EFFECTS_SKIP_AGGRO_FOE) != 0u) return false;
    if (living->type_map == TYPE_MAP_VANISHED_HOSTILE_MINION) return false;

    const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
    if (distSq > maxRange * maxRange) return false;

    if (castingOnly && living->skill == 0u) return false;
    if (noHexOnly && living->hex != 0u) return false;
    if (enchantedOnly && !HasEnchantment(living->agent_id)) return false;

    return true;
}

} // namespace

bool BuildSkillCache(CachedSkill outCache[8]) {
    if (!outCache) {
        return false;
    }

    std::memset(outCache, 0, sizeof(CachedSkill) * 8u);
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) {
        return false;
    }

    for (int i = 0; i < 8; ++i) {
        auto& cached = outCache[i];
        cached.skill_id = bar->skills[i].skill_id;
        cached.slot = static_cast<uint8_t>(i);
        if (cached.skill_id == 0u) {
            continue;
        }

        const auto* data = SkillMgr::GetSkillConstantData(cached.skill_id);
        if (!data) {
            continue;
        }

        cached.target_type = data->target;
        cached.energy_cost = data->energy_cost;
        cached.skill_type = static_cast<uint8_t>(data->type);
        cached.activation = data->activation;
        cached.recharge_time = static_cast<float>(data->recharge);

        if (data->target == 6u) {
            cached.roles |= ROLE_RESURRECT;
        }

        if (IsHardInterruptId(cached.skill_id))  cached.roles |= ROLE_INTERRUPT_HARD;
        if (IsCondRemovalId(cached.skill_id))    cached.roles |= ROLE_COND_REMOVE;
        if (IsHexRemovalId(cached.skill_id))     cached.roles |= ROLE_HEX_REMOVE;
        if (IsSurvivalId(cached.skill_id))       cached.roles |= ROLE_SURVIVAL;
        if (IsSpeedBoostId(cached.skill_id))     cached.roles |= ROLE_SPEED_BOOST;
        if (IsBindingId(cached.skill_id))        cached.roles |= ROLE_BINDING;
        if (IsPressureSpiritId(cached.skill_id)) cached.roles |= ROLE_PRESSURE | ROLE_PRECAST;
        if (IsPrecastId(cached.skill_id))        cached.roles |= ROLE_PRECAST;

        switch (data->type) {
        case 1:
            cached.roles |= ROLE_HEX | ROLE_PRESSURE;
            if (data->target == 5u) cached.roles |= ROLE_OFFENSIVE;
            break;
        case 2:
            if (data->target == 3u || data->target == 4u) {
                cached.roles |= ROLE_HEAL_SINGLE;
            } else if (data->target == 5u) {
                cached.roles |= ROLE_OFFENSIVE;
            } else if (data->target == 0u) {
                cached.roles |= ROLE_HEAL_SELF;
            }
            if (data->activation <= 0.5f && data->target == 5u &&
                (cached.roles & ROLE_INTERRUPT_HARD) == 0u) {
                cached.roles |= ROLE_INTERRUPT_SOFT;
            }
            break;
        case 3:
        case 16:
            if (data->target == 0u || data->target == 3u) {
                cached.roles |= ROLE_DEFENSIVE | ROLE_PROT;
            }
            if (data->target == 5u) cached.roles |= ROLE_OFFENSIVE;
            break;
        case 0:
            cached.roles |= ROLE_PRECAST | ROLE_DEFENSIVE;
            break;
        case 4:
            if (data->target == 5u) cached.roles |= ROLE_OFFENSIVE;
            else cached.roles |= ROLE_DEFENSIVE;
            break;
        case 5:
        case 7:
            cached.roles |= ROLE_PRECAST | ROLE_DEFENSIVE;
            break;
        case 6:
            if (data->target == 5u) cached.roles |= ROLE_OFFENSIVE;
            else cached.roles |= ROLE_DEFENSIVE;
            break;
        case 8:
            cached.roles |= ROLE_PRECAST;
            break;
        case 9:
        case 17:
            cached.roles |= ROLE_ATTACK | ROLE_OFFENSIVE;
            break;
        case 10:
        case 20:
            cached.roles |= ROLE_SHOUT | ROLE_PRECAST;
            break;
        case 11:
        case 12:
            cached.roles |= ROLE_PRECAST;
            break;
        case 13:
            cached.roles |= ROLE_BINDING | ROLE_PRECAST;
            break;
        case 22:
            if (cached.roles == ROLE_NONE) cached.roles |= ROLE_BINDING | ROLE_PRECAST;
            else cached.roles |= ROLE_PRECAST;
            break;
        case 14:
        case 15:
            if (data->target == 3u) cached.roles |= ROLE_DEFENSIVE;
            else if (data->target == 5u) cached.roles |= ROLE_OFFENSIVE;
            break;
        default:
            break;
        }
    }

    return true;
}

bool CacheSkillBar(CachedSkill outCache[8],
                   bool& outCached,
                   const SkillCacheLogCallbacks* logCallbacks) {
    outCached = BuildSkillCache(outCache);
    if (!outCached) {
        return false;
    }

    if (logCallbacks) {
        if (logCallbacks->summary) {
            logCallbacks->summary(outCache);
        }
        if (logCallbacks->slot) {
            for (int i = 0; i < 8; ++i) {
                if (outCache[i].skill_id == 0u) {
                    continue;
                }
                logCallbacks->slot(i, outCache[i]);
            }
        }
    }
    return true;
}

bool SkillTargetTypeRequiresResolvedTarget(uint8_t targetType) {
    return targetType == 1u ||
           targetType == 4u ||
           targetType == 5u ||
           targetType == 6u ||
           targetType == 14u;
}

uint32_t GetLowestHealthAlly(float maxRange, bool excludeSelf) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    float bestHp = 1.0f;
    uint32_t bestId = 0u;
    const float maxRangeSq = maxRange * maxRange;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 1u || living->hp <= 0.0f) continue;
        if (excludeSelf && living->agent_id == me->agent_id) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq > maxRangeSq) continue;
        if (living->hp < bestHp) {
            bestHp = living->hp;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

uint32_t CountHexEffects(uint32_t agentId) {
    return CountAgentEffectsByPredicate(agentId, &IsHexEffectSkill);
}

uint32_t CountConditionEffects(uint32_t agentId) {
    return CountAgentEffectsByPredicate(agentId, &IsConditionEffectSkill);
}

uint32_t GetMostHexedAlly(bool excludeSelf, float maxRange) {
    return GetMostAffectedAlly(excludeSelf, &CountHexEffects, maxRange);
}

uint32_t GetMostConditionedAlly(bool excludeSelf, float maxRange) {
    return GetMostAffectedAlly(excludeSelf, &CountConditionEffects, maxRange);
}

uint32_t GetDeadAlly(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    float bestDistSq = maxRange * maxRange;
    uint32_t bestId = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 1u || living->hp > 0.0f) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

uint32_t GetNearestLivingAgentByAllegiance(uint8_t allegiance, float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    float bestDistSq = maxRange * maxRange;
    uint32_t bestId = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != allegiance || living->hp <= 0.0f) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

uint32_t GetNearestSpiritAlly(float maxRange) {
    return GetNearestLivingAgentByAllegiance(4u, maxRange);
}

uint32_t GetNearestMinionAlly(float maxRange) {
    return GetNearestLivingAgentByAllegiance(5u, maxRange);
}

uint32_t SelectMostBalledEnemy(float maxRange,
                               bool castingOnly,
                               bool noHexOnly,
                               bool enchantedOnly) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    constexpr size_t kMaxCandidates = 512u;
    uint32_t candidateIds[kMaxCandidates] = {};
    size_t candidateCount = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1u; i < maxAgents && candidateCount < kMaxCandidates; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (!IsValidAggroEnemy(me, living, maxRange, castingOnly, noHexOnly, enchantedOnly)) continue;
        candidateIds[candidateCount++] = living->agent_id;
    }

    float bestSumDistances = 999999999.0f;
    uint32_t bestId = 0u;
    for (size_t i = 0u; i < candidateCount; ++i) {
        auto* src = AgentMgr::GetAgentByID(candidateIds[i]);
        if (!src) continue;

        float sumDistances = 0.0f;
        for (size_t j = 0u; j < candidateCount; ++j) {
            if (i == j) continue;
            auto* dst = AgentMgr::GetAgentByID(candidateIds[j]);
            if (!dst) continue;
            sumDistances += AgentMgr::GetDistance(src->x, src->y, dst->x, dst->y);
        }

        if (sumDistances < bestSumDistances) {
            bestSumDistances = sumDistances;
            bestId = candidateIds[i];
        }
    }
    return bestId;
}

uint32_t GetBestBalledEnemy(float maxRange) {
    return SelectMostBalledEnemy(maxRange, false, false, false);
}

uint32_t GetUnhexedBalledEnemy(float maxRange) {
    return SelectMostBalledEnemy(maxRange, false, true, false);
}

uint32_t GetCastingBalledEnemy(float maxRange) {
    return SelectMostBalledEnemy(maxRange, true, false, false);
}

uint32_t GetEnchantedBalledEnemy(float maxRange) {
    return SelectMostBalledEnemy(maxRange, false, false, true);
}

uint32_t GetMeleeBalledEnemy(float maxRange) {
    return SelectMostBalledEnemy(maxRange, false, false, false);
}

uint32_t GetUnhexedEnemy(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    float bestDistSq = maxRange * maxRange;
    uint32_t bestId = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 3u || living->hp <= 0.0f || living->hex != 0u) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

uint32_t GetCastingEnemy(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    float bestDistSq = maxRange * maxRange;
    uint32_t bestId = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 3u || living->hp <= 0.0f || living->skill == 0u) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

uint32_t GetEnchantedEnemy(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    float bestDistSq = maxRange * maxRange;
    uint32_t bestId = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 3u || living->hp <= 0.0f) continue;

        auto* effects = EffectMgr::GetAgentEffects(living->agent_id);
        if (!effects || !effects->effects.buffer) continue;

        bool hasEnchant = false;
        for (uint32_t effectIndex = 0; effectIndex < effects->effects.size; ++effectIndex) {
            const auto& effect = effects->effects.buffer[effectIndex];
            if (effect.skill_id == 0u) continue;
            const auto* skillData = SkillMgr::GetSkillConstantData(effect.skill_id);
            if (skillData && (skillData->type == 3u || skillData->type == 16u)) {
                hasEnchant = true;
                break;
            }
        }
        if (!hasEnchant) continue;

        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

uint32_t GetMeleeRangeEnemy(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    float bestDistSq = maxRange * maxRange;
    uint32_t bestId = 0u;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 3u || living->hp <= 0.0f) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

uint32_t ResolveSkillTarget(const CachedSkill& skill, uint32_t defaultFoeId, float aggroRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0u;

    if (skill.hasRole(ROLE_RESURRECT)) {
        return GetDeadAlly();
    }

    if (skill.target_type == 0u) {
        if (skill.skill_type == 7u) {
            auto* foe = defaultFoeId ? AgentMgr::GetAgentByID(defaultFoeId) : nullptr;
            if (!foe || foe->type != 0xDBu) return 0u;
            const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, foe->x, foe->y);
            if (distSq > aggroRange * aggroRange) return 0u;
        }
        return me->agent_id;
    }

    if (skill.target_type == 1u) {
        const uint32_t spiritId = GetNearestSpiritAlly();
        return spiritId ? spiritId : 0u;
    }

    if (skill.target_type == 3u || skill.target_type == 4u) {
        const bool excludeSelf = skill.target_type == 4u;
        if (skill.hasRole(ROLE_ANY_HEAL)) {
            const uint32_t targetId = GetLowestHealthAlly(2500.0f, excludeSelf);
            return targetId ? targetId : (excludeSelf ? 0u : me->agent_id);
        }
        if (skill.hasRole(ROLE_COND_REMOVE)) {
            const uint32_t targetId = GetMostConditionedAlly(excludeSelf);
            return targetId ? targetId : (excludeSelf ? 0u : me->agent_id);
        }
        if (skill.hasRole(ROLE_HEX_REMOVE)) {
            const uint32_t targetId = GetMostHexedAlly(excludeSelf);
            return targetId ? targetId : (excludeSelf ? 0u : me->agent_id);
        }
        if (skill.hasRole(ROLE_PRECAST) || skill.hasRole(ROLE_SURVIVAL)) {
            return me->agent_id;
        }
        const uint32_t targetId = GetLowestHealthAlly(2500.0f, excludeSelf);
        return targetId ? targetId : (excludeSelf ? 0u : me->agent_id);
    }

    if (skill.target_type == 5u) {
        if (skill.hasRole(ROLE_HEX)) {
            uint32_t targetId = GetUnhexedBalledEnemy(aggroRange);
            if (targetId) return targetId;
            targetId = GetBestBalledEnemy(aggroRange);
            return targetId ? targetId : defaultFoeId;
        }
        if (skill.hasRole(ROLE_ANY_INTERRUPT)) {
            return GetCastingBalledEnemy(aggroRange);
        }
        if (skill.hasRole(ROLE_ENCHANT_REMOVE)) {
            return GetEnchantedBalledEnemy(aggroRange);
        }
        if (skill.hasRole(ROLE_ATTACK)) {
            const uint32_t targetId = GetMeleeBalledEnemy();
            return targetId ? targetId : 0u;
        }
        return GetBestBalledEnemy(aggroRange);
    }

    if (skill.target_type == 6u) {
        return GetDeadAlly();
    }

    if (skill.target_type == 14u) {
        const uint32_t minionId = GetNearestMinionAlly();
        return minionId ? minionId : 0u;
    }

    return me->agent_id;
}

const char* ExplainCanCastFailure(const CachedSkill& skill) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return "no_player";
    if (me->hp <= 0.0f) return "dead";

    const uint32_t loadingState = MapMgr::GetLoadingState();
    if (loadingState == 2u) return "disconnected";
    if (loadingState != 1u) return "not_loaded";
    if (PartyMgr::GetIsPartyDefeated()) return "party_defeated";
    if (me->model_state == 0x450u) return "knocked";
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (bar && skill.slot < 8u && bar->skills[skill.slot].recharge > 0u) {
        return "recharging";
    }

    const uint32_t myId = me->agent_id;
    const uint8_t type = skill.skill_type;
    if (type == 1u || type == 2u || type == 3u || type == 5u || type == 7u ||
        type == 14u || type == 15u || type == 16u) {
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return "diversion";
        if (EffectMgr::HasEffect(myId, DEBUFF_BACKFIRE)) return "backfire";
        if (EffectMgr::HasEffect(myId, DEBUFF_SOUL_LEECH)) return "soul_leech";
        if (EffectMgr::HasEffect(myId, DEBUFF_MISTRUST)) return "mistrust";
        if (EffectMgr::HasEffect(myId, DEBUFF_VISIONS_OF_REGRET)) return "visions_of_regret";
        if (EffectMgr::HasEffect(myId, DEBUFF_MARK_OF_SUBVERSION)) return "mark_of_subversion";
        if (EffectMgr::HasEffect(myId, DEBUFF_SPITEFUL_SPIRIT)) return "spiteful_spirit";
    }
    if (type == 9u || type == 17u) {
        if (EffectMgr::HasEffect(myId, DEBUFF_INEPTITUDE)) return "ineptitude";
        if (EffectMgr::HasEffect(myId, DEBUFF_CLUMSINESS)) return "clumsiness";
        if (EffectMgr::HasEffect(myId, DEBUFF_WANDERING_EYE)) return "wandering_eye";
        if (EffectMgr::HasEffect(myId, DEBUFF_SPITEFUL_SPIRIT)) return "spiteful_spirit";
    }
    if (type == 4u) {
        if (EffectMgr::HasEffect(myId, DEBUFF_IGNORANCE)) return "ignorance";
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return "diversion";
    }
    if (type == 10u || type == 20u) {
        if (EffectMgr::HasEffect(myId, DEBUFF_WELL_OF_SILENCE)) return "well_of_silence";
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return "diversion";
    }
    return nullptr;
}

bool CanCast(const CachedSkill& skill) {
    return ExplainCanCastFailure(skill) == nullptr;
}

bool CanBasicAttack() {
    CachedSkill basicAttack = {};
    basicAttack.slot = 0xFFu;
    basicAttack.skill_type = 9u;
    return CanCast(basicAttack);
}

const char* ExplainCanUseSkillFailure(const CachedSkill& skill, uint32_t targetId, float aggroRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return "no_player";
    if (AgentMgr::IsCasting(me)) return "casting";

    if (const char* castFailure = ExplainCanCastFailure(skill)) return castFailure;

    const uint32_t resolvedTargetId = ResolveSkillTarget(skill, targetId, aggroRange);
    if (SkillTargetTypeRequiresResolvedTarget(skill.target_type) && resolvedTargetId == 0u) {
        return "no_target";
    }

    if (skill.hasRole(ROLE_ANY_HEAL)) {
        const uint32_t healTarget = resolvedTargetId;
        if (healTarget != 0u) {
            auto* ally = AgentMgr::GetAgentByID(healTarget);
            if (ally && ally->type == 0xDBu &&
                static_cast<AgentLiving*>(ally)->hp > 0.8f) {
                return "heal_target_healthy";
            }
        } else if (me->hp > 0.8f) {
            return "heal_self_healthy";
        }
    }

    if (skill.hasRole(ROLE_SURVIVAL)) {
        if (me->hp > 0.5f) return "survival_hp_high";
        if (EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id) > 5.0f) {
            return "survival_effect_active";
        }
    }

    if (skill.hasRole(ROLE_BINDING)) {
        bool enemyNearby = false;
        if (targetId > 0u) {
            auto* target = AgentMgr::GetAgentByID(targetId);
            if (target && target->type == 0xDBu) {
                auto* living = static_cast<AgentLiving*>(target);
                if (living->allegiance == 3u && living->hp > 0.0f) {
                    const float targetDistSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
                    if (targetDistSq < 5000.0f * 5000.0f) {
                        enemyNearby = true;
                    }
                }
            }
        }
        const uint32_t maxAgents = AgentMgr::GetMaxAgents();
        for (uint32_t i = 1u; i < maxAgents && !enemyNearby; ++i) {
            auto* agent = AgentMgr::GetAgentByID(i);
            if (!agent || agent->type != 0xDBu) continue;
            auto* living = static_cast<AgentLiving*>(agent);
            if (living->allegiance == 3u && living->hp > 0.0f) {
                const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
                if (distSq < 2500.0f * 2500.0f) enemyNearby = true;
            }
        }
        if (!enemyNearby) return "binding_no_enemy";
    }

    if (skill.hasRole(ROLE_PRECAST) && !skill.hasRole(ROLE_OFFENSIVE) &&
        EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id) > 3.0f) {
        return "precast_effect_active";
    }

    if (skill.hasRole(ROLE_SPEED_BOOST) &&
        EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id) > 3.0f) {
        return "speed_boost_active";
    }

    float energyCost = static_cast<float>(skill.energy_cost);
    if (EffectMgr::HasEffect(me->agent_id, EFFECT_QUICKENING_ZEPHYR)) {
        energyCost *= 1.3f;
    }
    const float myEnergy = me->energy * static_cast<float>(me->max_energy);
    if (energyCost > 0.0f && myEnergy < energyCost) {
        return "energy_low";
    }

    const auto* skillData = SkillMgr::GetSkillConstantData(skill.skill_id);
    if (skillData && skillData->adrenaline > 0u) {
        auto* bar = SkillMgr::GetPlayerSkillbar();
        if (bar && bar->skills[skill.slot].adrenaline_a < skillData->adrenaline) {
            return "adrenaline_low";
        }
    }

    if (skill.hasRole(ROLE_PRESSURE) && targetId > 0u && skill.skill_id == SkillIds::FINISH_HIM) {
        auto* target = AgentMgr::GetAgentByID(resolvedTargetId ? resolvedTargetId : targetId);
        if (target && target->type == 0xDBu &&
            static_cast<AgentLiving*>(target)->hp > 0.45f) {
            return "finish_him_hp_high";
        }
    }

    if (skill.hasRole(ROLE_COND_REMOVE)) {
        const uint32_t condTargetId = resolvedTargetId ? resolvedTargetId : me->agent_id;
        if (CountConditionEffects(condTargetId) == 0u) return "no_condition_target";
    }

    if (skill.hasRole(ROLE_HEX_REMOVE)) {
        const uint32_t hexTargetId = resolvedTargetId ? resolvedTargetId : me->agent_id;
        auto* hexTarget = AgentMgr::GetAgentByID(hexTargetId);
        if (!hexTarget || hexTarget->type != 0xDBu || static_cast<AgentLiving*>(hexTarget)->hex == 0u) {
            return "no_hex_target";
        }
    }

    return nullptr;
}

bool CanUseSkill(const CachedSkill& skill, uint32_t targetId, float aggroRange) {
    return ExplainCanUseSkillFailure(skill, targetId, aggroRange) == nullptr;
}

} // namespace GWA3::DungeonSkill
