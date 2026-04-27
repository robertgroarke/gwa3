#pragma once

#include <cstdint>

namespace GWA3::DungeonSkill {

inline constexpr uint32_t ROLE_NONE           = 0u;
inline constexpr uint32_t ROLE_HEAL_SINGLE    = (1u << 0);
inline constexpr uint32_t ROLE_HEAL_PARTY     = (1u << 1);
inline constexpr uint32_t ROLE_HEAL_SELF      = (1u << 2);
inline constexpr uint32_t ROLE_PROT           = (1u << 3);
inline constexpr uint32_t ROLE_BOND           = (1u << 4);
inline constexpr uint32_t ROLE_COND_REMOVE    = (1u << 5);
inline constexpr uint32_t ROLE_HEX_REMOVE     = (1u << 6);
inline constexpr uint32_t ROLE_ENCHANT_REMOVE = (1u << 7);
inline constexpr uint32_t ROLE_HEX            = (1u << 8);
inline constexpr uint32_t ROLE_PRESSURE       = (1u << 9);
inline constexpr uint32_t ROLE_ATTACK         = (1u << 10);
inline constexpr uint32_t ROLE_INTERRUPT_HARD = (1u << 11);
inline constexpr uint32_t ROLE_INTERRUPT_SOFT = (1u << 12);
inline constexpr uint32_t ROLE_PRECAST        = (1u << 13);
inline constexpr uint32_t ROLE_BINDING        = (1u << 14);
inline constexpr uint32_t ROLE_SPEED_BOOST    = (1u << 15);
inline constexpr uint32_t ROLE_SURVIVAL       = (1u << 16);
inline constexpr uint32_t ROLE_SHOUT          = (1u << 17);
inline constexpr uint32_t ROLE_RESURRECT      = (1u << 18);
inline constexpr uint32_t ROLE_OFFENSIVE      = (1u << 19);
inline constexpr uint32_t ROLE_DEFENSIVE      = (1u << 20);

inline constexpr uint32_t ROLE_ANY_HEAL =
    ROLE_HEAL_SINGLE | ROLE_HEAL_PARTY | ROLE_HEAL_SELF;
inline constexpr uint32_t ROLE_ANY_INTERRUPT =
    ROLE_INTERRUPT_HARD | ROLE_INTERRUPT_SOFT;
inline constexpr uint32_t ROLE_ANY_REMOVAL =
    ROLE_COND_REMOVE | ROLE_HEX_REMOVE | ROLE_ENCHANT_REMOVE;

struct CachedSkill {
    uint32_t skill_id = 0u;
    uint32_t roles = ROLE_NONE;
    uint8_t slot = 0u;
    uint8_t target_type = 0u;
    uint8_t energy_cost = 0u;
    uint8_t skill_type = 0u;
    float activation = 0.0f;
    float recharge_time = 0.0f;

    bool hasRole(uint32_t roleMask) const {
        return (roles & roleMask) != 0u;
    }
};

using SkillCacheSummaryLogFn = void(*)(const CachedSkill cache[8]);
using SkillCacheSlotLogFn = void(*)(int slotIndex, const CachedSkill& skill);

struct SkillCacheLogCallbacks {
    SkillCacheSummaryLogFn summary = nullptr;
    SkillCacheSlotLogFn slot = nullptr;
};

bool BuildSkillCache(CachedSkill outCache[8]);
bool CacheSkillBar(CachedSkill outCache[8],
                   bool& outCached,
                   const SkillCacheLogCallbacks* logCallbacks = nullptr);
bool SkillTargetTypeRequiresResolvedTarget(uint8_t targetType);

uint32_t GetLowestHealthAlly(float maxRange = 2500.0f, bool excludeSelf = false);
uint32_t CountHexEffects(uint32_t agentId);
uint32_t CountConditionEffects(uint32_t agentId);
uint32_t GetMostHexedAlly(bool excludeSelf = false, float maxRange = 2500.0f);
uint32_t GetMostConditionedAlly(bool excludeSelf = false, float maxRange = 2500.0f);
uint32_t GetDeadAlly(float maxRange = 2500.0f);
uint32_t GetNearestLivingAgentByAllegiance(uint8_t allegiance, float maxRange = 2500.0f);
uint32_t GetNearestSpiritAlly(float maxRange = 2500.0f);
uint32_t GetNearestMinionAlly(float maxRange = 2500.0f);
uint32_t SelectMostBalledEnemy(float maxRange = 1350.0f,
                               bool castingOnly = false,
                               bool noHexOnly = false,
                               bool enchantedOnly = false);
uint32_t GetBestBalledEnemy(float maxRange = 1350.0f);
uint32_t GetUnhexedBalledEnemy(float maxRange = 1320.0f);
uint32_t GetCastingBalledEnemy(float maxRange = 1320.0f);
uint32_t GetEnchantedBalledEnemy(float maxRange = 1320.0f);
uint32_t GetMeleeBalledEnemy(float maxRange = 1320.0f);
uint32_t GetUnhexedEnemy(float maxRange = 1500.0f);
uint32_t GetCastingEnemy(float maxRange = 1500.0f);
uint32_t GetEnchantedEnemy(float maxRange = 1500.0f);
uint32_t GetMeleeRangeEnemy(float maxRange = 250.0f);

uint32_t ResolveSkillTarget(const CachedSkill& skill, uint32_t defaultFoeId, float aggroRange = 1320.0f);
const char* ExplainCanCastFailure(const CachedSkill& skill);
bool CanCast(const CachedSkill& skill);
bool CanBasicAttack();
const char* ExplainCanUseSkillFailure(const CachedSkill& skill, uint32_t targetId, float aggroRange = 1320.0f);
bool CanUseSkill(const CachedSkill& skill, uint32_t targetId, float aggroRange = 1320.0f);

} // namespace GWA3::DungeonSkill
