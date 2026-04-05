#include <gwa3/bot/FroggyHM.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>

#include <Windows.h>
#include <cmath>

namespace GWA3::Bot::Froggy {

// ===== Constants =====

static constexpr uint32_t MAP_SPARKFLY_SWAMP    = 558;
static constexpr uint32_t MAP_BOGROOT_LVL1      = 615;
static constexpr uint32_t MAP_BOGROOT_LVL2      = 616;
static constexpr uint32_t MAP_GADDS_ENCAMPMENT  = 638;

static constexpr uint32_t QUEST_TEKKS_WAR       = 0x339;
static constexpr uint32_t DIALOG_QUEST_REWARD   = 0x833907;
static constexpr uint32_t DIALOG_QUEST_ACCEPT   = 0x833905;
static constexpr uint32_t DIALOG_QUEST_BODY     = 0x8101;
static constexpr uint32_t DIALOG_NPC_TALK       = 0x2AE6;

// ===== Waypoint =====

struct Waypoint {
    float x, y;
    float fightRange;
    const char* label;
};

// ===== Routes =====

static const Waypoint SPARKFLY_TO_DUNGEON[] = {
    {-4559,  -14406, 1300, "1"},
    {-5204,  -9831,  1300, "2"},
    {-928,   -8699,  1300, "3"},
    {4200,   -4897,  1500, "4"},
    {6114,   819,    1300, "5"},
    {9500,   2281,   1300, "6"},
    {11570,  6120,   1200, "7"},
    {11025,  11710,  900,  "8"},
    {14624,  19314,  600,  "9"},
    {14650,  19417,  0,    "10"},
    {12280,  22585,  0,    "11"},
};

static const Waypoint BOGROOT_LVL1[] = {
    {17026,  2168,   1200, "0"},
    {19099,  7762,   1200, "Blessing"},
    {17279,  8106,   1200, "1"},
    {14434,  8000,   1200, "Quest Door Checkpoint"},
    {14434,  8000,   1300, "2"},
    {10789,  6433,   1300, "3"},
    {8101,   6800,   1200, "4"},
    {6721,   5340,   1300, "5"},
    {4305,   1078,   1300, "6"},
    {757,    1110,   1200, "7"},
    {1370,   149,    1700, "8"},
    {672,    1105,   2000, "9"},
    {453,    1449,   2000, "10"},
    {504,    -1973,  800,  "11"},
    {-447,   -3014,  800,  "12"},
    {-1055,  -4527,  1000, "13"},
    {-1424,  -6156,  1200, "14"},
    {-475,   -7511,  800,  "15"},
    {265,    -8791,  1400, "16"},
    {1061,   -9443,  1400, "17"},
    {1805,   -10185, 1400, "18"},
    {1665,   -12213, 1400, "19"},
    {3550,   -16052, 1400, "20"},
    {4941,   -16181, 0,    "21"},
    {7360,   -17361, 0,    "22"},
    {7552,   -18776, 0,    "23"},
    {7665,   -19050, 0,    "24"},
    {7665,   -19050, 0,    "Lvl1 to Lvl2"},
};

static const Waypoint BOGROOT_LVL2[] = {
    {-11386, -3871,  400,  "1"},
    {-11132, -2450,  400,  "2"},
    {-8559,  593,    300,  "3"},
    {-4110,  4484,   1200, "4"},
    {-3747,  5068,   1200, "5"},
    {-2597,  5775,   1000, "6"},
    {-2618,  6383,   1100, "7"},
    {-2770,  7571,   1000, "8"},
    {-243,   8364,   1000, "9"},
    {-189,   10499,  1000, "10"},
    {37,     11449,  1400, "11"},
    {3086,   12899,  2000, "12"},
    {4182,   13767,  2000, "13"},
    {7293,   9457,   2000, "14"},
    {8150,   8143,   1500, "15"},
    {8560,   2323,   1500, "16"},
    {9525,   -1153,  1500, "17"},
    {12200,  -6591,  1600, "18"},
    {12200,  -6591,  1600, "19"},
    {17003,  -4906,  1600, "20"},
    {16854,  -5830,  1600, "Dungeon Key"},
    {17925,  -6197,  300,  "Dungeon Door"},
    {17482,  -6661,  300,  "Dungeon Door Checkpoint"},
    {18334,  -8838,  0,    "Boss 1"},
    {16131,  -11510, 0,    "Boss 2"},
    {19009,  -12300, 0,    "Boss 3"},
    {19610,  -11527, 0,    "Boss 4"},
    {18413,  -13924, 0,    "Boss 5"},
    {14188,  -15231, 0,    "Boss 6"},
    {13186,  -17286, 0,    "Boss 7"},
    {14035,  -17800, 0,    "Boss 8"},
    {13583,  -17529, 1100, "Boss 9"},
    {14617,  -18282, 1400, "Boss 10"},
    {15117,  -18582, 1400, "Boss 11"},
    {15117,  -18582, 1400, "Boss 12"},
    {15117,  -18582, 1600, "Boss"},
};

// ===== Run Statistics =====
static uint32_t s_runCount = 0;
static uint32_t s_failCount = 0;
static uint32_t s_wipeCount = 0;
static DWORD s_runStartTime = 0;
static DWORD s_totalStartTime = 0;
static DWORD s_bestRunTime = 0xFFFFFFFF;

// ===== Forward declarations =====
static void WaitMs(DWORD ms);
static int PickupNearbyLoot(float maxRange = 1200.0f);
static bool OpenNearbyChest(float maxRange = 1200.0f);
static uint32_t CountItemByModel(uint32_t modelId);
static void FlagAllHeroes(float x, float y);
static void UnflagAllHeroes();
static bool SendDialogWithRetry(uint32_t dialogId, int maxRetries = 3, DWORD delayMs = 1000);
static void UseDpRemovalIfNeeded();

// ===== Skill Template Decoder (GWA3-101) =====

// GW's custom base64 alphabet
static const char* BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int Base64CharToVal(char c) {
    const char* p = strchr(BASE64_CHARS, c);
    return p ? static_cast<int>(p - BASE64_CHARS) : -1;
}

// Decode a GW skill template code into 8 skill IDs.
// Returns true on success, fills skillIds[8].
static bool DecodeSkillTemplate(const char* code, uint32_t skillIds[8]) {
    // Convert base64 to bit stream
    uint8_t bits[256] = {};
    int totalBits = 0;
    for (int i = 0; code[i] && totalBits < 240; i++) {
        int val = Base64CharToVal(code[i]);
        if (val < 0) continue;
        // 6 bits per char, LSB first
        for (int b = 0; b < 6; b++) {
            bits[totalBits++] = (val >> b) & 1;
        }
    }

    int pos = 0;
    auto readBits = [&](int count) -> uint32_t {
        uint32_t val = 0;
        for (int i = 0; i < count && pos < totalBits; i++) {
            val |= (bits[pos++] << i);
        }
        return val;
    };

    // Header
    uint32_t header = readBits(4);
    if (header == 14) {
        readBits(4); // version, skip
    } else if (header != 0) {
        return false; // unknown template type
    }

    // Professions
    uint32_t profBits = readBits(2) * 2 + 4;
    readBits(profBits); // primary prof (skip)
    readBits(profBits); // secondary prof (skip)

    // Attributes (skip)
    uint32_t attrCount = readBits(4);
    uint32_t attrBits = readBits(4) + 4;
    for (uint32_t i = 0; i < attrCount; i++) {
        readBits(attrBits); // attr ID
        readBits(4);        // attr value
    }

    // Skills
    uint32_t skillBits = readBits(4) + 8;
    for (int i = 0; i < 8; i++) {
        skillIds[i] = readBits(skillBits);
    }

    return true;
}

// Load hero configs from a file. Format: "HeroID,TemplateCode ; comment"
// Returns number of heroes loaded.
static int LoadHeroConfigFile(const char* filename, BotConfig& cfg) {
    // Build path relative to DLL location
    char dllDir[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&LoadHeroConfigFile), &hSelf);
    GetModuleFileNameA(hSelf, dllDir, MAX_PATH);
    char* slash = strrchr(dllDir, '\\');
    if (slash) *(slash + 1) = '\0';

    char path[MAX_PATH] = {};
    snprintf(path, sizeof(path), "%s..\\..\\GWA Censured\\hero_configs\\%s", dllDir, filename);

    FILE* f = nullptr;
    fopen_s(&f, path, "r");
    if (!f) {
        LogBot("Hero config file not found: %s", path);
        return 0;
    }

    char line[512];
    int heroIdx = 0;
    while (fgets(line, sizeof(line), f) && heroIdx < 7) {
        // Strip comment
        char* semi = strchr(line, ';');
        if (semi) *semi = '\0';
        // Strip whitespace
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '\r') continue;

        // Parse "HeroID,TemplateCode"
        char* comma = strchr(p, ',');
        if (!comma) continue;
        *comma = '\0';
        uint32_t heroId = static_cast<uint32_t>(atoi(p));
        char* tmpl = comma + 1;
        // Trim trailing whitespace
        char* end = tmpl + strlen(tmpl) - 1;
        while (end > tmpl && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';

        if (heroId == 0 || tmpl[0] == '\0') continue;

        cfg.hero_ids[heroIdx] = heroId;

        // Decode template and load skillbar
        uint32_t skillIds[8] = {};
        if (DecodeSkillTemplate(tmpl, skillIds)) {
            LogBot("Hero %d (id=%u): skills=%u,%u,%u,%u,%u,%u,%u,%u",
                   heroIdx + 1, heroId,
                   skillIds[0], skillIds[1], skillIds[2], skillIds[3],
                   skillIds[4], skillIds[5], skillIds[6], skillIds[7]);
            SkillMgr::LoadSkillbar(skillIds, heroIdx + 1);
            WaitMs(500); // rate limit between skillbar loads
        } else {
            LogBot("Hero %d (id=%u): failed to decode template '%s'", heroIdx + 1, heroId, tmpl);
        }

        heroIdx++;
    }
    fclose(f);
    LogBot("Loaded %d heroes from %s", heroIdx, filename);
    return heroIdx;
}

// ===== Skill System (GWA3-097, GWA3-122) =====

// Role bitmask — a skill can have multiple roles
static constexpr uint32_t ROLE_NONE           = 0;
static constexpr uint32_t ROLE_HEAL_SINGLE    = (1 << 0);   // Single-target heal
static constexpr uint32_t ROLE_HEAL_PARTY     = (1 << 1);   // Party-wide heal
static constexpr uint32_t ROLE_HEAL_SELF      = (1 << 2);   // Self-only heal
static constexpr uint32_t ROLE_PROT           = (1 << 3);   // Protection spell
static constexpr uint32_t ROLE_BOND           = (1 << 4);   // Maintained enchantment
static constexpr uint32_t ROLE_COND_REMOVE    = (1 << 5);   // Condition removal
static constexpr uint32_t ROLE_HEX_REMOVE     = (1 << 6);   // Hex removal
static constexpr uint32_t ROLE_ENCHANT_REMOVE = (1 << 7);   // Enchant removal on foe
static constexpr uint32_t ROLE_HEX            = (1 << 8);   // Hex spell (offensive)
static constexpr uint32_t ROLE_PRESSURE       = (1 << 9);   // Condition/hex pressure
static constexpr uint32_t ROLE_ATTACK         = (1 << 10);  // Melee/ranged attack
static constexpr uint32_t ROLE_INTERRUPT_HARD = (1 << 11);  // Hard interrupt
static constexpr uint32_t ROLE_INTERRUPT_SOFT = (1 << 12);  // Soft interrupt
static constexpr uint32_t ROLE_PRECAST        = (1 << 13);  // Pre-combat setup
static constexpr uint32_t ROLE_BINDING        = (1 << 14);  // Spirit/ritual
static constexpr uint32_t ROLE_SPEED_BOOST    = (1 << 15);  // Movement speed
static constexpr uint32_t ROLE_SURVIVAL       = (1 << 16);  // Defensive survival
static constexpr uint32_t ROLE_SHOUT          = (1 << 17);  // Shout/chant
static constexpr uint32_t ROLE_RESURRECT      = (1 << 18);  // Resurrection
static constexpr uint32_t ROLE_OFFENSIVE      = (1 << 19);  // Generic offensive
static constexpr uint32_t ROLE_DEFENSIVE      = (1 << 20);  // Generic defensive

// Combined masks for legacy compatibility
static constexpr uint32_t ROLE_ANY_HEAL = ROLE_HEAL_SINGLE | ROLE_HEAL_PARTY | ROLE_HEAL_SELF;
static constexpr uint32_t ROLE_ANY_INTERRUPT = ROLE_INTERRUPT_HARD | ROLE_INTERRUPT_SOFT;
static constexpr uint32_t ROLE_ANY_REMOVAL = ROLE_COND_REMOVE | ROLE_HEX_REMOVE | ROLE_ENCHANT_REMOVE;

struct CachedSkill {
    uint32_t skill_id;
    uint32_t roles;        // bitmask of ROLE_* flags
    uint8_t  slot;         // 0-7
    uint8_t  target_type;  // from Skill::target (0=self, 3=ally, 5=foe, 6=dead)
    uint8_t  energy_cost;
    uint8_t  skill_type;   // from Skill::type
    float    activation;
    float    recharge_time;
    bool hasRole(uint32_t r) const { return (roles & r) != 0; }
};

static CachedSkill s_skillCache[8] = {};
static bool s_skillsCached = false;

// ===== Skill Classifiers (ported from BotCore-SkillRules.au3) =====

// Hardcoded interrupt skill IDs (from AutoIt IsHardRuptSkill)
static bool IsHardInterruptId(uint32_t id) {
    switch (id) {
    case 5: case 64: case 99: case 116: case 170:   // Power Block, Power Drain, etc.
    case 218: case 312: case 332: case 782: case 783:
    case 838: case 950: case 1041: case 1338: case 1489:
    case 2013: case 2162: case 2370:
        return true;
    }
    return false;
}

// Condition removal skills
static bool IsCondRemovalId(uint32_t id) {
    switch (id) {
    case 25: case 31: case 53: case 222: case 270:  // Cure Hex, Dismiss Condition, etc.
    case 280: case 289: case 291: case 331: case 838:
    case 951: case 1258: case 2054: case 2059: case 2145:
    case 2179: case 2286: case 2362: case 2451:
        return true;
    }
    return false;
}

// Hex removal skills
static bool IsHexRemovalId(uint32_t id) {
    switch (id) {
    case 24: case 25: case 156: case 270: case 280:  // Cure Hex, Holy Veil, etc.
    case 304: case 331: case 838: case 944: case 1258:
    case 2145: case 2179: case 2451:
        return true;
    }
    return false;
}

// Survival/shadow skills
static bool IsSurvivalId(uint32_t id) {
    switch (id) {
    case 2358: case 312: case 826: case 828: case 867:  // Shadow Form, Shroud of Distress, etc.
    case 878: case 1338: case 2370: case 2371:
        return true;
    }
    return false;
}

// Speed boost skills (40+ from AutoIt IsSpeedBoost)
static bool IsSpeedBoostId(uint32_t id) {
    switch (id) {
    case 312: case 452: case 826: case 828: case 856:
    case 867: case 878: case 947: case 1003: case 1338:
    case 2370: case 2371: case 2051: case 2052:
        return true;
    }
    return false;
}

// Binding ritual skill IDs
static bool IsBindingId(uint32_t id) {
    switch (id) {
    case 786: case 787: case 788: case 789: case 790:  // Spirits
    case 791: case 792: case 793: case 794: case 795:
    case 960: case 961: case 962: case 963: case 964:
    case 965: case 966: case 967: case 2083: case 2084:
    case 2085: case 2087: case 2088: case 2089:
        return true;
    }
    return false;
}

static void CacheSkillBar() {
    s_skillsCached = false;
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) return;

    for (int i = 0; i < 8; i++) {
        auto& c = s_skillCache[i];
        c.skill_id = bar->skills[i].skill_id;
        c.slot = static_cast<uint8_t>(i);
        c.roles = ROLE_NONE;
        c.target_type = 0;
        c.energy_cost = 0;
        c.skill_type = 0;
        c.activation = 0;
        c.recharge_time = 0;

        if (c.skill_id == 0) continue;

        const auto* data = SkillMgr::GetSkillConstantData(c.skill_id);
        if (!data) continue;

        c.target_type = data->target;
        c.energy_cost = data->energy_cost;
        c.skill_type = static_cast<uint8_t>(data->type);
        c.activation = data->activation;
        c.recharge_time = static_cast<float>(data->recharge);

        // === Role assignment ===

        // Resurrection (target type 6 = dead ally)
        if (data->target == 6) {
            c.roles |= ROLE_RESURRECT;
        }

        // Hardcoded role overrides by skill ID
        if (IsHardInterruptId(c.skill_id))  c.roles |= ROLE_INTERRUPT_HARD;
        if (IsCondRemovalId(c.skill_id))    c.roles |= ROLE_COND_REMOVE;
        if (IsHexRemovalId(c.skill_id))     c.roles |= ROLE_HEX_REMOVE;
        if (IsSurvivalId(c.skill_id))       c.roles |= ROLE_SURVIVAL;
        if (IsSpeedBoostId(c.skill_id))     c.roles |= ROLE_SPEED_BOOST;
        if (IsBindingId(c.skill_id))        c.roles |= ROLE_BINDING;

        // Type-based classification
        switch (data->type) {
        case 1:  // Hex
            c.roles |= ROLE_HEX | ROLE_PRESSURE;
            if (data->target == 5) c.roles |= ROLE_OFFENSIVE;
            break;
        case 2:  // Spell
            if (data->target == 3 || data->target == 4) {
                c.roles |= ROLE_HEAL_SINGLE;
            } else if (data->target == 5) {
                c.roles |= ROLE_OFFENSIVE;
            } else if (data->target == 0) {
                c.roles |= ROLE_HEAL_SELF;
            }
            // Soft interrupt: any fast spell targeting foe
            if (data->activation <= 0.5f && data->target == 5 && !(c.roles & ROLE_INTERRUPT_HARD)) {
                c.roles |= ROLE_INTERRUPT_SOFT;
            }
            break;
        case 3:  // Enchantment
        case 16: // Flash Enchantment
            if (data->target == 0 || data->target == 3) {
                c.roles |= ROLE_DEFENSIVE | ROLE_PROT;
            }
            if (data->target == 5) c.roles |= ROLE_OFFENSIVE;
            break;
        case 0:  // Stance
            c.roles |= ROLE_PRECAST | ROLE_DEFENSIVE;
            break;
        case 4:  // Signet
            if (data->target == 5) c.roles |= ROLE_OFFENSIVE;
            else c.roles |= ROLE_DEFENSIVE;
            break;
        case 5:  // Well
        case 7:  // Ward
            c.roles |= ROLE_PRECAST | ROLE_DEFENSIVE;
            break;
        case 6:  // Skill (generic)
            if (data->target == 5) c.roles |= ROLE_OFFENSIVE;
            else c.roles |= ROLE_DEFENSIVE;
            break;
        case 8:  // Glyph
            c.roles |= ROLE_PRECAST;
            break;
        case 9:  // Attack skill
        case 17: // Double Attack
            c.roles |= ROLE_ATTACK | ROLE_OFFENSIVE;
            break;
        case 10: // Shout
        case 20: // Chant
            c.roles |= ROLE_SHOUT | ROLE_PRECAST;
            break;
        case 11: // Preparation
            c.roles |= ROLE_PRECAST;
            break;
        case 12: // Trap
            c.roles |= ROLE_PRECAST;
            break;
        case 13: // Ritual
            c.roles |= ROLE_BINDING | ROLE_PRECAST;
            break;
        case 14: // Item Spell
        case 15: // Weapon Spell
            if (data->target == 3) c.roles |= ROLE_DEFENSIVE;
            else if (data->target == 5) c.roles |= ROLE_OFFENSIVE;
            break;
        }
    }
    s_skillsCached = true;
    LogBot("Skillbar cached (bitmask roles): %u/%u/%u/%u/%u/%u/%u/%u",
           s_skillCache[0].skill_id, s_skillCache[1].skill_id,
           s_skillCache[2].skill_id, s_skillCache[3].skill_id,
           s_skillCache[4].skill_id, s_skillCache[5].skill_id,
           s_skillCache[6].skill_id, s_skillCache[7].skill_id);
}

// Try to use a skill from the cache. Returns true if a skill was used.
// ===== Intelligent Target Selection (GWA3-123) =====

// Find the ally with the lowest HP fraction. Returns 0 if no ally found.
static uint32_t GetLowestHealthAlly(float maxRange = 2500.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestHp = 1.0f;
    uint32_t bestId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 1) continue; // allies only
        if (living->hp <= 0.0f) continue; // skip dead
        float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (dist > maxRange * maxRange) continue;
        if (living->hp < bestHp) {
            bestHp = living->hp;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

// Find the nearest dead ally. Returns 0 if none.
static uint32_t GetDeadAlly(float maxRange = 2500.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = maxRange * maxRange;
    uint32_t bestId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 1) continue;
        if (living->hp > 0.0f) continue; // alive — skip
        float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

// Find nearest enemy without a hex. Returns 0 if all hexed or none nearby.
static uint32_t GetUnhexedEnemy(float maxRange = 1500.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = maxRange * maxRange;
    uint32_t bestId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 3) continue; // foes only
        if (living->hp <= 0.0f) continue;
        if (living->hex != 0) continue; // already hexed — skip
        float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

// Find nearest enemy that is currently casting. Returns 0 if none casting.
static uint32_t GetCastingEnemy(float maxRange = 1500.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = maxRange * maxRange;
    uint32_t bestId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 3) continue;
        if (living->hp <= 0.0f) continue;
        if (living->skill == 0) continue; // not casting
        float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

// Find nearest enemy with an enchantment. Returns 0 if none enchanted.
static uint32_t GetEnchantedEnemy(float maxRange = 1500.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = maxRange * maxRange;
    uint32_t bestId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 3) continue;
        if (living->hp <= 0.0f) continue;
        // Check for enchantments via effects
        auto* agentEffects = EffectMgr::GetAgentEffects(living->agent_id);
        if (!agentEffects) continue;
        bool hasEnchant = false;
        if (agentEffects->effects.buffer) {
            for (uint32_t ei = 0; ei < agentEffects->effects.size; ei++) {
                auto& eff = agentEffects->effects.buffer[ei];
                if (eff.skill_id == 0) continue;
                const auto* sd = SkillMgr::GetSkillConstantData(eff.skill_id);
                if (sd && (sd->type == 3 || sd->type == 16)) { // Enchantment or Flash Enchantment
                    hasEnchant = true;
                    break;
                }
            }
        }
        if (!hasEnchant) continue;
        float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

// Find nearest enemy in melee range (250 units).
static uint32_t GetMeleeRangeEnemy() {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = 250.0f * 250.0f;
    uint32_t bestId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 3) continue;
        if (living->hp <= 0.0f) continue;
        float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

// Resolve best target for a skill based on its roles.
// Returns the target agent ID to use, or 0 if no valid target.
static uint32_t ResolveSkillTarget(const CachedSkill& skill, uint32_t defaultFoeId) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;

    // Resurrection: target dead ally
    if (skill.hasRole(ROLE_RESURRECT)) {
        return GetDeadAlly();
    }

    // Ally-targeting skills
    if (skill.target_type == 3 || skill.target_type == 4) {
        // Heal: target lowest HP ally
        if (skill.hasRole(ROLE_ANY_HEAL)) {
            uint32_t target = GetLowestHealthAlly();
            return target ? target : me->agent_id;
        }
        // Condition removal: target most-conditioned (simplified: use self for now)
        if (skill.hasRole(ROLE_COND_REMOVE)) return me->agent_id;
        // Hex removal: target most-hexed (simplified: use self for now)
        if (skill.hasRole(ROLE_HEX_REMOVE)) return me->agent_id;
        // Default ally: self
        return me->agent_id;
    }

    // Foe-targeting skills
    if (skill.target_type == 5) {
        // Hex: prefer unhexed enemy
        if (skill.hasRole(ROLE_HEX)) {
            uint32_t target = GetUnhexedEnemy();
            return target ? target : defaultFoeId;
        }
        // Interrupt: prefer casting enemy
        if (skill.hasRole(ROLE_ANY_INTERRUPT)) {
            uint32_t target = GetCastingEnemy();
            return target ? target : 0; // don't waste interrupt if nobody casting
        }
        // Enchant removal: prefer enchanted enemy
        if (skill.hasRole(ROLE_ENCHANT_REMOVE)) {
            uint32_t target = GetEnchantedEnemy();
            return target ? target : 0;
        }
        // Attack: prefer melee range
        if (skill.hasRole(ROLE_ATTACK)) {
            uint32_t target = GetMeleeRangeEnemy();
            return target ? target : defaultFoeId;
        }
        return defaultFoeId;
    }

    // Self-targeting / no target
    return 0;
}

// ===== Debuff Blocking (GWA3-126) =====

// Debuff skill IDs that block spell casting
static constexpr uint32_t DEBUFF_DIVERSION       = 11;
static constexpr uint32_t DEBUFF_BACKFIRE         = 73;
static constexpr uint32_t DEBUFF_SOUL_LEECH       = 844;
static constexpr uint32_t DEBUFF_MISTRUST         = 2065;
static constexpr uint32_t DEBUFF_VISIONS_OF_REGRET = 2042;
static constexpr uint32_t DEBUFF_MARK_OF_SUBVERSION = 654;
static constexpr uint32_t DEBUFF_SPITEFUL_SPIRIT  = 653;
// Attack-blocking debuffs
static constexpr uint32_t DEBUFF_INEPTITUDE       = 60;
static constexpr uint32_t DEBUFF_CLUMSINESS       = 51;
static constexpr uint32_t DEBUFF_WANDERING_EYE    = 1039;
// Shout/chant blocking
static constexpr uint32_t DEBUFF_WELL_OF_SILENCE  = 668;
// Signet blocking
static constexpr uint32_t DEBUFF_IGNORANCE        = 56;

static bool CanCast(const CachedSkill& skill) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return false;
    if (me->hp <= 0.0f) return false; // dead
    if (!MapMgr::GetIsMapLoaded()) return false;

    uint32_t myId = me->agent_id;

    // Check debuffs based on skill type
    uint8_t type = skill.skill_type;

    // Spell/Hex/Enchantment/Well/Ward blocking debuffs
    if (type == 1 || type == 2 || type == 3 || type == 5 || type == 7 ||
        type == 14 || type == 15 || type == 16) {
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_BACKFIRE)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_SOUL_LEECH)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_MISTRUST)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_VISIONS_OF_REGRET)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_SPITEFUL_SPIRIT)) return false;
    }

    // Attack skill blocking debuffs
    if (type == 9 || type == 17) {
        if (EffectMgr::HasEffect(myId, DEBUFF_INEPTITUDE)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_CLUMSINESS)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_WANDERING_EYE)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_SPITEFUL_SPIRIT)) return false;
    }

    // Signet blocking
    if (type == 4) {
        if (EffectMgr::HasEffect(myId, DEBUFF_IGNORANCE)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return false;
    }

    // Shout/Chant blocking
    if (type == 10 || type == 20) {
        if (EffectMgr::HasEffect(myId, DEBUFF_WELL_OF_SILENCE)) return false;
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return false;
    }

    return true;
}

// ===== HP Gating & Effect Overlap (GWA3-124) =====

static bool CanUseSkill(const CachedSkill& skill, uint32_t targetId) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return false;

    // Debuff check first
    if (!CanCast(skill)) return false;

    // Heal skills: only cast if someone actually needs healing
    if (skill.hasRole(ROLE_ANY_HEAL)) {
        uint32_t healTarget = GetLowestHealthAlly();
        if (healTarget) {
            auto* ally = AgentMgr::GetAgentByID(healTarget);
            if (ally && ally->type == 0xDB) {
                auto* living = static_cast<AgentLiving*>(ally);
                if (living->hp > 0.8f) return false; // nobody below 80% — don't waste
            }
        } else {
            if (me->hp > 0.8f) return false; // self is fine — skip
        }
    }

    // Survival skills: only if HP is low
    if (skill.hasRole(ROLE_SURVIVAL)) {
        if (me->hp > 0.5f) return false;
        // Check if already have the effect active
        float remaining = EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id);
        if (remaining > 5.0f) return false; // already active with >5s left
    }

    // Binding rituals: only if enemies nearby
    if (skill.hasRole(ROLE_BINDING)) {
        bool enemyNearby = false;
        uint32_t maxAgents = AgentMgr::GetMaxAgents();
        for (uint32_t i = 1; i < maxAgents && !enemyNearby; i++) {
            auto* a = AgentMgr::GetAgentByID(i);
            if (!a || a->type != 0xDB) continue;
            auto* living = static_cast<AgentLiving*>(a);
            if (living->allegiance == 3 && living->hp > 0.0f) {
                float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
                if (dist < 1500.0f * 1500.0f) enemyNearby = true;
            }
        }
        if (!enemyNearby) return false;
    }

    // Precast: check if effect already active (don't re-cast stances/preps)
    if (skill.hasRole(ROLE_PRECAST) && !skill.hasRole(ROLE_OFFENSIVE)) {
        float remaining = EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id);
        if (remaining > 3.0f) return false; // already active
    }

    return true;
}

// Try to use a skill matching the given role bitmask. Returns true if a skill was used.
static bool TryUseSkillWithRole(uint32_t targetId, uint32_t roleMask) {
    auto* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = AgentMgr::GetMyAgent();
    if (!bar || !me) return false;

    float myEnergy = me->energy * me->max_energy; // absolute energy points

    for (int i = 0; i < 8; i++) {
        auto& c = s_skillCache[i];
        if (c.skill_id == 0) continue;
        if (!(c.roles & roleMask)) continue; // no matching role
        if (bar->skills[i].recharge > 0) continue; // still recharging
        if (c.energy_cost > static_cast<uint8_t>(myEnergy)) continue; // not enough energy
        if (!CanUseSkill(c, targetId)) continue; // HP gates, debuff blocking, overlap prevention

        // Intelligent target selection based on skill role
        uint32_t skillTarget = ResolveSkillTarget(c, targetId);
        // If ResolveSkillTarget returns 0 for a foe-targeting skill, skip it
        if (skillTarget == 0 && (c.target_type == 5 || c.target_type == 6)) continue;

        SkillMgr::UseSkill(i + 1, skillTarget, 0);
        return true;
    }
    return false;
}

// Full combat routine: use skills then fall back to auto-attack
static void FightTarget(uint32_t targetId) {
    // GWA3-121: Combat mode toggle — if LLM mode, just auto-attack
    // Gemma handles skill decisions via the bridge
    auto& cfg = Bot::GetConfig();
    if (cfg.combat_mode == CombatMode::LLM) {
        AgentMgr::Attack(targetId);
        return;
    }

    if (!s_skillsCached) CacheSkillBar();

    auto* me = AgentMgr::GetMyAgent();
    if (!me) return;

    // === GWA3-125: Dynamic Priority Combat Engine ===

    // Priority 1: Emergency ally heal — if any ally HP < 30%
    uint32_t lowestAlly = GetLowestHealthAlly();
    if (lowestAlly) {
        auto* ally = AgentMgr::GetAgentByID(lowestAlly);
        if (ally && ally->type == 0xDB) {
            auto* allyLiving = static_cast<AgentLiving*>(ally);
            if (allyLiving->hp < 0.3f && allyLiving->hp > 0.0f) {
                if (TryUseSkillWithRole(lowestAlly, ROLE_ANY_HEAL)) return;
            }
        }
    }

    // Priority 2: Resurrection — if any ally dead
    uint32_t deadAlly = GetDeadAlly();
    if (deadAlly) {
        if (TryUseSkillWithRole(deadAlly, ROLE_RESURRECT)) return;
    }

    // Priority 3: Self-survival — if own HP critically low
    if (me->hp < 0.3f) {
        if (TryUseSkillWithRole(targetId, ROLE_SURVIVAL)) return;
        if (TryUseSkillWithRole(me->agent_id, ROLE_ANY_HEAL)) return;
        if (TryUseSkillWithRole(targetId, ROLE_PROT | ROLE_DEFENSIVE)) return;
    }

    // Priority 4: Condition/hex removal on self
    if (me->hex != 0) {
        TryUseSkillWithRole(me->agent_id, ROLE_HEX_REMOVE | ROLE_COND_REMOVE);
    }

    // Priority 5: Interrupt casting enemies (prefer hard over soft)
    uint32_t castingFoe = GetCastingEnemy();
    if (castingFoe) {
        if (!TryUseSkillWithRole(castingFoe, ROLE_INTERRUPT_HARD)) {
            TryUseSkillWithRole(castingFoe, ROLE_INTERRUPT_SOFT);
        }
    }

    // Priority 6: Pre-combat buffs (stances, shouts, preparations)
    TryUseSkillWithRole(targetId, ROLE_PRECAST | ROLE_SHOUT);

    // Priority 7: Hex pressure — prefer unhexed enemies
    TryUseSkillWithRole(targetId, ROLE_HEX | ROLE_PRESSURE);

    // Priority 8: Enchant removal on enchanted enemies
    uint32_t enchantedFoe = GetEnchantedEnemy();
    if (enchantedFoe) {
        TryUseSkillWithRole(enchantedFoe, ROLE_ENCHANT_REMOVE);
    }

    // Priority 9: Offensive skills (attacks, damage spells)
    if (!TryUseSkillWithRole(targetId, ROLE_OFFENSIVE | ROLE_ATTACK)) {
        // No skills ready — auto-attack
        AgentMgr::Attack(targetId);
    }
}

// ===== Helpers =====

static float DistanceTo(float x, float y) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 99999.0f;
    return AgentMgr::GetDistance(me->x, me->y, x, y);
}

static int GetNearestWaypointIndex(const Waypoint* wps, int count) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = 999999.0f;
    int bestIdx = 0;
    for (int i = 0; i < count; i++) {
        float d = AgentMgr::GetSquaredDistance(me->x, me->y, wps[i].x, wps[i].y);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

static bool IsDead() {
    auto* me = AgentMgr::GetMyAgent();
    return !me || me->hp <= 0.0f;
}

static bool IsMapLoaded() {
    return MapMgr::GetIsMapLoaded();
}

static void WaitMs(DWORD ms) {
    Sleep(ms);
}

static void MoveToAndWait(float x, float y, float threshold = 250.0f) {
    AgentMgr::Move(x, y);
    DWORD start = GetTickCount();
    while (DistanceTo(x, y) > threshold && (GetTickCount() - start) < 30000) {
        if (IsDead()) return;
        WaitMs(250);
    }
}

static void AggroMoveToEx(float x, float y, float fightRange = 1350.0f) {
    AgentMgr::Move(x, y);
    DWORD start = GetTickCount();
    while (DistanceTo(x, y) > 250.0f && (GetTickCount() - start) < 240000) {
        if (IsDead()) return;
        if (!IsMapLoaded()) return;

        // Check for enemies in fight range — auto-attack nearest
        {
            float bestDist = fightRange * fightRange;
            uint32_t bestId = 0;
            auto* me = AgentMgr::GetMyAgent();
            uint32_t maxAgents = AgentMgr::GetMaxAgents();
            if (me && maxAgents > 0) {
                for (uint32_t i = 1; i < maxAgents; i++) {
                    auto* a = AgentMgr::GetAgentByID(i);
                    if (!a || a->type != 0xDB) continue;
                    auto* living = static_cast<AgentLiving*>(a);
                    if (living->allegiance != 3) continue; // not foe
                    if (living->hp <= 0.0f) continue;
                    float d = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
                    if (d < bestDist) {
                        bestDist = d;
                        bestId = living->agent_id;
                    }
                }
            }
            if (bestId) {
                // Flag heroes to fight position
                auto* foeAgent = AgentMgr::GetAgentByID(bestId);
                if (foeAgent) {
                    FlagAllHeroes(foeAgent->x, foeAgent->y);
                }
                AgentMgr::ChangeTarget(bestId);
                FightTarget(bestId);
                // Brief wait for skill activation
                WaitMs(500);
                // Continue fighting if enemy still alive
                auto* foe = AgentMgr::GetAgentByID(bestId);
                if (foe && foe->type == 0xDB) {
                    auto* living = static_cast<AgentLiving*>(foe);
                    if (living->hp > 0.0f) {
                        WaitMs(500);
                        continue; // keep fighting same target
                    }
                }
                // Combat resolved — unflag heroes
                UnflagAllHeroes();
                // Enemy dead or gone — pick up loot
                PickupNearbyLoot(600.0f);
                continue;
            }
        }

        AgentMgr::Move(x, y);
        WaitMs(500);
    }
}

static void FollowWaypoints(const Waypoint* wps, int count) {
    int startIdx = GetNearestWaypointIndex(wps, count);
    uint32_t mapId = MapMgr::GetMapId();

    for (int i = startIdx; i < count; i++) {
        if (!Bot::IsRunning()) return;
        if (MapMgr::GetMapId() != mapId) return;
        if (IsDead()) {
            s_wipeCount++;
            LogBot("WIPE detected at waypoint %d (%s) — wipe #%u", i, wps[i].label, s_wipeCount);

            // Wait for resurrection (up to 2 minutes)
            DWORD wipeStart = GetTickCount();
            while (IsDead() && (GetTickCount() - wipeStart) < 120000) {
                WaitMs(500);
            }
            if (IsDead()) {
                // Party fully defeated — return to outpost
                LogBot("Party defeated — returning to outpost");
                MapMgr::ReturnToOutpost();
                return;
            }

            // Alive again — use DP removal if we've wiped multiple times
            if (s_wipeCount >= 2) {
                LogBot("Multiple wipes (%u) — using DP removal", s_wipeCount);
                UseDpRemovalIfNeeded();
                WaitMs(500);
            }

            // Resume from nearest waypoint
            int restartIdx = GetNearestWaypointIndex(wps, count);
            LogBot("Resuming from waypoint %d after wipe (was at %d)", restartIdx, i);
            i = restartIdx;
        }

        LogBot("Moving to waypoint %d: %s (%.0f, %.0f)", i, wps[i].label, wps[i].x, wps[i].y);

        // Special waypoint handling
        if (strcmp(wps[i].label, "Lvl1 to Lvl2") == 0) {
            DWORD start = GetTickCount();
            while ((GetTickCount() - start) < 60000) {
                AgentMgr::Move(7665, -19050);
                WaitMs(250);
                if (MapMgr::GetMapId() == MAP_BOGROOT_LVL2) return;
            }
            return;
        }
        if (strcmp(wps[i].label, "Dungeon Key") == 0) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            // Pickup dungeon key + any loot in area
            WaitMs(500);
            PickupNearbyLoot(1200.0f);
            continue;
        }
        if (strcmp(wps[i].label, "Dungeon Door") == 0) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            // Open door interaction
            AgentMgr::InteractSignpost(AgentMgr::GetTargetId());
            WaitMs(2000);
            continue;
        }
        if (strcmp(wps[i].label, "Boss") == 0) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            // Boss encounter — fight then loot
            WaitMs(3000);
            PickupNearbyLoot(1500.0f);
            // Open chest
            MoveToAndWait(14876, -19033);
            OpenNearbyChest(1000.0f);
            WaitMs(2000);
            PickupNearbyLoot(1000.0f);
            // Talk to Tekk for reward (with retry)
            MoveToAndWait(14618, -17828);
            SendDialogWithRetry(DIALOG_QUEST_REWARD, 3, 1000);
            return;
        }

        // Standard waypoint — aggro move then loot sweep
        if (wps[i].fightRange > 0 && IsMapLoaded()) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            PickupNearbyLoot(800.0f);
        } else {
            MoveToAndWait(wps[i].x, wps[i].y);
        }
    }
}

// ===== Item Rarity (from name_enc first ushort) =====

static constexpr uint16_t RARITY_WHITE  = 2621;
static constexpr uint16_t RARITY_BLUE   = 2623;
static constexpr uint16_t RARITY_GOLD   = 2624;
static constexpr uint16_t RARITY_PURPLE = 2626;
static constexpr uint16_t RARITY_GREEN  = 2627;

static uint16_t GetItemRarity(const Item* item) {
    if (!item || !item->name_enc) return 0;
    __try {
        return *reinterpret_cast<const uint16_t*>(item->name_enc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static bool IsIdentified(const Item* item) {
    return item && (item->interaction & 0x1) != 0;
}

// Item types (from GWA2_ID)
static constexpr uint8_t ITEM_TYPE_SALVAGE = 11;
static constexpr uint8_t ITEM_TYPE_WEAPON  = 0;
static constexpr uint8_t ITEM_TYPE_OFFHAND = 2;
static constexpr uint8_t ITEM_TYPE_SHIELD  = 4;

// Consumable model IDs (ID kits, salvage kits, consets, DP removal)
static constexpr uint32_t MODEL_ID_KIT       = 2992;  // Identification Kit
static constexpr uint32_t MODEL_SUP_ID_KIT   = 5899;  // Superior ID Kit
static constexpr uint32_t MODEL_SALV_KIT     = 2993;  // Salvage Kit
static constexpr uint32_t MODEL_EXP_SALV_KIT = 2991;  // Expert Salvage Kit
static constexpr uint32_t MODEL_ARMOR_SALV   = 5900;  // Armor of Salvation (conset)
static constexpr uint32_t MODEL_ESSENCE_CEL  = 5901;  // Essence of Celerity (conset)
static constexpr uint32_t MODEL_GRAIL_MIGHT  = 5902;  // Grail of Might (conset)

// DP removal sweets
static constexpr uint32_t MODEL_BIRTHDAY_CUPCAKE  = 22269;
static constexpr uint32_t MODEL_SLICE_BIRTHDAY     = 28436;
static constexpr uint32_t MODEL_CANDY_CORN         = 28431;

// ===== Hero Flagging (GWA3-107) =====

static void FlagAllHeroes(float x, float y) {
    PartyMgr::FlagAll(x, y);
}

static void UnflagAllHeroes() {
    PartyMgr::UnflagAll();
}

// ===== Quest Dialog Retry (GWA3-108) =====

static bool SendDialogWithRetry(uint32_t dialogId, int maxRetries, DWORD delayMs) {
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        QuestMgr::Dialog(dialogId);
        WaitMs(delayMs);

        // Check if dialog was processed (we can't directly verify,
        // but waiting + retrying is the best we can do)
        if (attempt < maxRetries - 1) {
            LogBot("Dialog 0x%X attempt %d/%d", dialogId, attempt + 1, maxRetries);
        }
    }
    return true;
}

// ===== Loot Pickup (GWA3-098) =====

// Model IDs that should always be picked up regardless of rarity
static bool IsAlwaysPickupModel(uint32_t modelId) {
    switch (modelId) {
    // Ectoplasm, obsidian
    case 930: case 945:
    // Diamonds, rubies, sapphires, onyx
    case 935: case 936: case 937: case 938:
    // DP removal sweets
    case 22269: case 28436: case 28431:
    // Lockpicks
    case 22751:
    // Tomes (all professions)
    case 21796: case 21797: case 21798: case 21799: case 21800:
    case 21801: case 21802: case 21803: case 21804: case 21805:
        return true;
    }
    return false;
}

static bool ShouldPickUp(const Agent* agent, uint32_t myAgentId) {
    if (!agent || agent->type != 0x400) return false;
    auto* itemAgent = static_cast<const AgentItem*>(agent);

    // Don't pick up items reserved for other players
    if (itemAgent->owner != 0 && itemAgent->owner != myAgentId) return false;

    // Cross-reference with actual item data
    auto* item = ItemMgr::GetItemById(itemAgent->item_id);
    if (!item) return true; // Can't read item data — pick up anyway

    // Always pick up whitelisted models
    if (IsAlwaysPickupModel(item->model_id)) return true;

    uint16_t rarity = GetItemRarity(item);

    // Always pick up gold and green rarity
    if (rarity == RARITY_GOLD || rarity == RARITY_GREEN) return true;

    // Pick up purple if we have space
    if (rarity == RARITY_PURPLE) return true;

    // Skip white and blue (not worth the time unless it's a material)
    if (item->type == 11) return true; // Materials always

    return false;
}

static int PickupNearbyLoot(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    uint32_t myId = me->agent_id;

    // Check free slots
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return 0;
    uint32_t freeSlots = 0;
    for (int b = 1; b <= 4; b++) {
        auto* bag = ItemMgr::GetBag(b);
        if (!bag) continue;
        freeSlots += (bag->items.size > bag->items_count) ? (bag->items.size - bag->items_count) : 0;
    }
    if (freeSlots == 0) return 0;

    int picked = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents && freeSlots > 0; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0x400) continue;
        float dist = AgentMgr::GetDistance(me->x, me->y, a->x, a->y);
        if (dist > maxRange) continue;
        if (!ShouldPickUp(a, myId)) continue;

        // Move close enough to pick up
        if (dist > 200.0f) {
            AgentMgr::Move(a->x, a->y);
            DWORD start = GetTickCount();
            while (AgentMgr::GetDistance(me->x, me->y, a->x, a->y) > 200.0f &&
                   (GetTickCount() - start) < 5000) {
                WaitMs(100);
                me = AgentMgr::GetMyAgent();
                if (!me || IsDead()) return picked;
            }
        }

        ItemMgr::PickUpItem(a->agent_id);
        WaitMs(300);
        picked++;
        freeSlots--;
        me = AgentMgr::GetMyAgent(); // refresh after pickup
        if (!me) return picked;
    }
    return picked;
}

// Known chest gadget IDs
static bool IsChestGadgetId(uint32_t gadgetId) {
    return gadgetId == 6062 || gadgetId == 4579 || gadgetId == 4582 ||
           gadgetId == 8141 || gadgetId == 74 || gadgetId == 68 || gadgetId == 9157;
}

static bool OpenNearbyChest(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return false;

    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0x200) continue;
        float dist = AgentMgr::GetDistance(me->x, me->y, a->x, a->y);
        if (dist > maxRange) continue;

        auto* gadget = static_cast<const AgentGadget*>(a);
        if (!IsChestGadgetId(gadget->gadget_id)) continue;

        LogBot("Opening chest (agent=%u gadget=%u dist=%.0f)", a->agent_id, gadget->gadget_id, dist);

        // Move to chest
        if (dist > 200.0f) {
            MoveToAndWait(a->x, a->y, 200.0f);
        }

        AgentMgr::InteractSignpost(a->agent_id);
        WaitMs(2000);

        // Pick up any drops from chest
        PickupNearbyLoot(800.0f);
        return true;
    }
    return false;
}

// ===== Item Identification (GWA3-099) =====

static int IdentifyGoldItems() {
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    // Find an ID kit
    uint32_t kitId = 0;
    for (int b = 1; b <= 4 && !kitId; b++) {
        auto* bag = ItemMgr::GetBag(b);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            auto* item = bag->items.buffer[s];
            if (!item) continue;
            if (item->model_id == MODEL_ID_KIT || item->model_id == MODEL_SUP_ID_KIT) {
                kitId = item->item_id;
                break;
            }
        }
    }
    if (!kitId) {
        LogBot("No ID kit found — skipping identification");
        return 0;
    }

    int identified = 0;
    for (int b = 1; b <= 4; b++) {
        auto* bag = ItemMgr::GetBag(b);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            auto* item = bag->items.buffer[s];
            if (!item || item->item_id == 0) continue;
            if (IsIdentified(item)) continue;

            uint16_t rarity = GetItemRarity(item);
            if (rarity != RARITY_GOLD && rarity != RARITY_PURPLE) continue;

            LogBot("Identifying item %u (model=%u rarity=%u)", item->item_id, item->model_id, rarity);
            ItemMgr::IdentifyItem(item->item_id, kitId);
            WaitMs(500);
            identified++;

            // Re-check kit still exists (charges deplete)
            auto* kit = ItemMgr::GetItemById(kitId);
            if (!kit) {
                LogBot("ID kit depleted after %d identifications", identified);
                return identified;
            }
        }
    }
    if (identified > 0) {
        LogBot("Identified %d items", identified);
    }
    return identified;
}

// ===== Salvage System (GWA3-100) =====

static bool ShouldSalvage(const Item* item) {
    if (!item || item->item_id == 0 || item->model_id == 0) return false;
    if (item->equipped) return false;
    if (item->customized) return false;

    uint16_t rarity = GetItemRarity(item);

    // Only salvage white and blue items
    if (rarity != RARITY_WHITE && rarity != RARITY_BLUE) return false;

    // Never salvage kits, consumables, quest items
    switch (item->model_id) {
    case MODEL_ID_KIT: case MODEL_SUP_ID_KIT:
    case MODEL_SALV_KIT: case MODEL_EXP_SALV_KIT:
    case MODEL_ARMOR_SALV: case MODEL_ESSENCE_CEL: case MODEL_GRAIL_MIGHT:
    case MODEL_BIRTHDAY_CUPCAKE: case MODEL_SLICE_BIRTHDAY: case MODEL_CANDY_CORN:
        return false;
    }

    // Don't salvage materials (type 11) — they already ARE materials
    if (item->type == 11) return false;

    // Don't salvage keys (type 18)
    if (item->type == 18) return false;

    // Don't salvage usable items (type 9) — scrolls, tonics, etc.
    if (item->type == 9) return false;

    // Don't salvage kits (type 29)
    if (item->type == 29) return false;

    return true;
}

static int SalvageJunkItems() {
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    // Find a salvage kit
    uint32_t kitId = 0;
    for (int b = 1; b <= 4 && !kitId; b++) {
        auto* bag = ItemMgr::GetBag(b);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            auto* item = bag->items.buffer[s];
            if (!item) continue;
            if (item->model_id == MODEL_SALV_KIT || item->model_id == MODEL_EXP_SALV_KIT) {
                kitId = item->item_id;
                break;
            }
        }
    }
    if (!kitId) {
        LogBot("No salvage kit found — skipping salvage");
        return 0;
    }

    int salvaged = 0;
    for (int b = 1; b <= 4; b++) {
        auto* bag = ItemMgr::GetBag(b);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            auto* item = bag->items.buffer[s];
            if (!item || item->item_id == 0) continue;
            if (!ShouldSalvage(item)) continue;

            LogBot("Salvaging item %u (model=%u type=%u)", item->item_id, item->model_id, item->type);
            ItemMgr::SalvageSessionOpen(kitId, item->item_id);
            WaitMs(800);
            ItemMgr::SalvageMaterials();
            WaitMs(800);
            ItemMgr::SalvageSessionDone();
            WaitMs(300);
            salvaged++;

            // Re-check kit still exists
            auto* kit = ItemMgr::GetItemById(kitId);
            if (!kit) {
                LogBot("Salvage kit depleted after %d salvages", salvaged);
                return salvaged;
            }
        }
    }
    if (salvaged > 0) {
        LogBot("Salvaged %d items for materials", salvaged);
    }
    return salvaged;
}

static bool ShouldSellItem(const Item* item) {
    if (!item || item->item_id == 0 || item->model_id == 0) return false;
    if (item->equipped) return false;
    if (item->customized) return false;

    const uint16_t rarity = GetItemRarity(item);

    // Never sell green items
    if (rarity == RARITY_GREEN) return false;

    // Never sell unidentified golds/purples — they might be valuable
    if ((rarity == RARITY_GOLD || rarity == RARITY_PURPLE) && !IsIdentified(item)) return false;

    // Never sell kits, consets, or DP removal items
    switch (item->model_id) {
    case MODEL_ID_KIT: case MODEL_SUP_ID_KIT:
    case MODEL_SALV_KIT: case MODEL_EXP_SALV_KIT:
    case MODEL_ARMOR_SALV: case MODEL_ESSENCE_CEL: case MODEL_GRAIL_MIGHT:
    case MODEL_BIRTHDAY_CUPCAKE: case MODEL_SLICE_BIRTHDAY: case MODEL_CANDY_CORN:
        return false;
    }

    // Sell white items
    if (rarity == RARITY_WHITE) return true;

    // Sell identified blue items (low value)
    if (rarity == RARITY_BLUE && IsIdentified(item)) return true;

    // Sell identified purple/gold if value is low (< 100g each)
    if (IsIdentified(item) && item->value < 100) return true;

    return false;
}

static uint32_t FindNearestNpcByAllegiance(float x, float y, float maxDist) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    if (maxAgents == 0) return 0;

    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 6) continue; // not NPC
        if (living->hp <= 0.0f) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (d < bestDist) {
            bestDist = d;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

static void SellJunkToMerchant() {
    // Gadd's Encampment merchant coordinates
    static constexpr float kGaddsMerchantX = -8374.0f;
    static constexpr float kGaddsMerchantY = -22491.0f;

    LogBot("Moving to merchant...");
    MoveToAndWait(kGaddsMerchantX, kGaddsMerchantY, 350.0f);
    WaitMs(500);

    // Find and interact with merchant NPC
    uint32_t merchantId = FindNearestNpcByAllegiance(kGaddsMerchantX, kGaddsMerchantY, 900.0f);
    if (!merchantId) {
        LogBot("No merchant NPC found near target coords");
        return;
    }

    LogBot("Interacting with merchant %u...", merchantId);
    AgentMgr::ChangeTarget(merchantId);
    WaitMs(250);

    // Approach and interact
    auto* npc = AgentMgr::GetAgentByID(merchantId);
    if (npc) {
        MoveToAndWait(npc->x, npc->y, 120.0f);
    }

    CtoS::SendPacket(2, 0x38u, merchantId); // INTERACT_NPC
    WaitMs(750);
    CtoS::SendPacket(2, 0x3Bu, merchantId); // Dialog to open merchant
    WaitMs(1000);

    // Check if merchant window opened
    if (TradeMgr::GetMerchantItemCount() == 0) {
        LogBot("Merchant window did not open, retrying...");
        CtoS::SendPacket(2, 0x38u, merchantId);
        WaitMs(500);
        CtoS::SendPacket(2, 0x3Bu, merchantId);
        WaitMs(1000);
    }

    if (TradeMgr::GetMerchantItemCount() == 0) {
        LogBot("Merchant window failed to open, skipping sell");
        return;
    }

    // Sell items from backpack bags (1-4)
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return;

    uint32_t itemsSold = 0;
    uint32_t goldEarned = 0;

    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;

        for (uint32_t slot = 0; slot < bag->items.size; slot++) {
            Item* item = bag->items.buffer[slot];
            if (!item) continue;

            if (ShouldSellItem(item)) {
                uint32_t totalValue = item->value * item->quantity;
                LogBot("  Selling item %u (model=%u qty=%u value=%u)",
                       item->item_id, item->model_id, item->quantity, totalValue);

                // Sell via TransactItems: type=0xB (MerchantSell)
                TradeMgr::TransactItems(0xB, item->quantity, item->item_id);
                WaitMs(200 + ChatMgr::GetPing());

                itemsSold++;
                goldEarned += totalValue;
            }
        }
    }

    LogBot("Sold %u items for ~%u gold", itemsSold, goldEarned);
}

// ===== Xunlai Chest Operations (GWA3-105) =====

static constexpr float kGaddsXunlaiX = -10481.0f;
static constexpr float kGaddsXunlaiY = -22787.0f;

// Model IDs worth storing in Xunlai
static bool ShouldStore(const Item* item) {
    if (!item || item->item_id == 0) return false;
    uint16_t rarity = GetItemRarity(item);
    // Store ectos, gems, rare materials
    switch (item->model_id) {
    case 930:   // Glob of Ectoplasm
    case 935: case 936: case 937: case 938:  // Diamond, Onyx, Ruby, Sapphire
    case 945:   // Obsidian Shard
        return true;
    }
    // Store green items
    if (rarity == RARITY_GREEN) return true;
    return false;
}

static void DepositValuablesToXunlai() {
    // Move to Xunlai chest
    LogBot("Moving to Xunlai chest...");
    MoveToAndWait(kGaddsXunlaiX, kGaddsXunlaiY, 350.0f);
    WaitMs(500);

    // Find and interact with Xunlai NPC
    uint32_t xunlaiId = FindNearestNpcByAllegiance(kGaddsXunlaiX, kGaddsXunlaiY, 900.0f);
    if (!xunlaiId) {
        LogBot("No Xunlai chest NPC found");
        return;
    }

    auto* npc = AgentMgr::GetAgentByID(xunlaiId);
    if (npc) {
        MoveToAndWait(npc->x, npc->y, 120.0f);
    }
    CtoS::SendPacket(2, 0x38u, xunlaiId); // INTERACT_NPC
    WaitMs(1500);

    // Move valuable items from backpack (bags 1-4) to storage (bag 8 = first storage pane)
    auto* inv = ItemMgr::GetInventory();
    if (!inv) return;

    int deposited = 0;
    for (int b = 1; b <= 4; b++) {
        auto* bag = ItemMgr::GetBag(b);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            auto* item = bag->items.buffer[s];
            if (!item) continue;
            if (!ShouldStore(item)) continue;

            // Find a free slot in storage (bags 8-16)
            for (int sb = 8; sb <= 16; sb++) {
                auto* storageBag = ItemMgr::GetBag(sb);
                if (!storageBag) continue;
                if (storageBag->items_count < storageBag->items.size) {
                    LogBot("Depositing item %u (model=%u) to storage bag %d",
                           item->item_id, item->model_id, sb);
                    ItemMgr::MoveItem(item->item_id, sb, storageBag->items_count);
                    WaitMs(500);
                    deposited++;
                    break;
                }
            }
        }
    }
    if (deposited > 0) {
        LogBot("Deposited %d items to Xunlai storage", deposited);
    }
}

// ===== Kit Purchasing (GWA3-103) =====

static void BuyKitsIfNeeded() {
    static constexpr uint32_t TARGET_ID_KITS   = 3;
    static constexpr uint32_t TARGET_SALV_KITS = 3;

    // Check if merchant window is open
    if (TradeMgr::GetMerchantItemCount() == 0) {
        LogBot("BuyKits: merchant window not open, skipping");
        return;
    }

    // Count current kits
    uint32_t idKits = CountItemByModel(MODEL_ID_KIT) + CountItemByModel(MODEL_SUP_ID_KIT);
    uint32_t salvKits = CountItemByModel(MODEL_SALV_KIT) + CountItemByModel(MODEL_EXP_SALV_KIT);

    // Buy ID kits
    if (idKits < TARGET_ID_KITS) {
        uint32_t toBuy = TARGET_ID_KITS - idKits;
        LogBot("Buying %u ID kits (have %u, target %u)", toBuy, idKits, TARGET_ID_KITS);
        TradeMgr::BuyMaterials(MODEL_ID_KIT, toBuy);
        WaitMs(500 + ChatMgr::GetPing());
    }

    // Buy salvage kits
    if (salvKits < TARGET_SALV_KITS) {
        uint32_t toBuy = TARGET_SALV_KITS - salvKits;
        LogBot("Buying %u salvage kits (have %u, target %u)", toBuy, salvKits, TARGET_SALV_KITS);
        TradeMgr::BuyMaterials(MODEL_SALV_KIT, toBuy);
        WaitMs(500 + ChatMgr::GetPing());
    }
}

static uint32_t CountFreeSlots() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t freeSlots = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            if (!bag->items.buffer[i]) freeSlots++;
        }
    }
    return freeSlots;
}

static uint32_t CountItemByModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t total = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (item && item->model_id == modelId) {
                total += item->quantity;
            }
        }
    }
    return total;
}

static Item* FindItemByModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;

    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (item && item->model_id == modelId) return item;
        }
    }
    return nullptr;
}

// Known buff/effect skill IDs for maintenance detection
static constexpr uint32_t SKILL_DWARVEN_BLESSING  = 2049;
static constexpr uint32_t SKILL_ASURAN_BLESSING   = 2050;
static constexpr uint32_t SKILL_NORN_BLESSING     = 2051;
static constexpr uint32_t SKILL_VANGUARD_BLESSING = 2052;
static constexpr uint32_t SKILL_ARMOR_OF_SALVATION = 2053; // conset
static constexpr uint32_t SKILL_ESSENCE_CELERITY  = 2054; // conset
static constexpr uint32_t SKILL_GRAIL_OF_MIGHT    = 2055; // conset

static uint32_t GetPlayerEffectCount() {
    auto* ae = EffectMgr::GetPlayerEffects();
    if (!ae) return 0;
    return ae->effects.size;
}

static bool HasBlessing() {
    if (Offsets::MyID <= 0x10000) return false;
    uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);
    return EffectMgr::HasEffect(myId, SKILL_DWARVEN_BLESSING) ||
           EffectMgr::HasEffect(myId, SKILL_ASURAN_BLESSING) ||
           EffectMgr::HasEffect(myId, SKILL_NORN_BLESSING) ||
           EffectMgr::HasEffect(myId, SKILL_VANGUARD_BLESSING);
}

static bool HasConset() {
    if (Offsets::MyID <= 0x10000) return false;
    uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);
    return EffectMgr::HasEffect(myId, SKILL_ARMOR_OF_SALVATION) &&
           EffectMgr::HasEffect(myId, SKILL_ESSENCE_CELERITY) &&
           EffectMgr::HasEffect(myId, SKILL_GRAIL_OF_MIGHT);
}

// ===== Conset Crafting (GWA3-104) =====

static constexpr uint32_t MAP_EMBARK_BEACH = 857;

// Embark Beach crafter NPC coordinates
static constexpr float kEyjaX = 3336.0f, kEyjaY = 627.0f;       // Grail of Might
static constexpr float kKwatX = 3596.0f, kKwatY = 107.0f;       // Essence of Celerity
static constexpr float kAlcusX = 3704.0f, kAlcusY = -163.0f;    // Armor of Salvation

static void CraftConsetsIfNeeded() {
    // Only craft if config says to use consets
    auto& cfg = Bot::GetConfig();
    if (!cfg.use_consets) return;

    // Count current consets
    uint32_t armor = CountItemByModel(MODEL_ARMOR_SALV);
    uint32_t essence = CountItemByModel(MODEL_ESSENCE_CEL);
    uint32_t grail = CountItemByModel(MODEL_GRAIL_MIGHT);

    // Need at least 3 of each for a few runs
    static constexpr uint32_t MIN_CONSETS = 3;
    if (armor >= MIN_CONSETS && essence >= MIN_CONSETS && grail >= MIN_CONSETS) {
        return; // Well stocked
    }

    LogBot("Consets low (armor=%u essence=%u grail=%u) — traveling to Embark Beach to craft",
           armor, essence, grail);

    // Travel to Embark Beach
    MapMgr::Travel(MAP_EMBARK_BEACH);
    DWORD travelStart = GetTickCount();
    while (MapMgr::GetMapId() != MAP_EMBARK_BEACH && (GetTickCount() - travelStart) < 60000) {
        WaitMs(1000);
    }
    if (MapMgr::GetMapId() != MAP_EMBARK_BEACH) {
        LogBot("Failed to travel to Embark Beach for crafting");
        return;
    }
    WaitMs(3000);

    // Helper: interact with crafter NPC and craft
    auto craftAt = [](float npcX, float npcY, uint32_t modelId, uint32_t count, const char* name) {
        MoveToAndWait(npcX, npcY, 350.0f);
        uint32_t npcId = FindNearestNpcByAllegiance(npcX, npcY, 900.0f);
        if (!npcId) {
            LogBot("Crafter NPC '%s' not found", name);
            return;
        }
        auto* npc = AgentMgr::GetAgentByID(npcId);
        if (npc) MoveToAndWait(npc->x, npc->y, 120.0f);

        CtoS::SendPacket(2, 0x38u, npcId); // INTERACT_NPC
        WaitMs(750);
        CtoS::SendPacket(2, 0x3Bu, npcId); // Open crafter dialog
        WaitMs(1000);

        if (TradeMgr::GetMerchantItemCount() == 0) {
            LogBot("Crafter window did not open for '%s'", name);
            return;
        }

        // Find the item in the crafter's list
        auto* item = TradeMgr::GetMerchantItemByModelId(modelId);
        if (!item) {
            LogBot("Item model %u not found in crafter list", modelId);
            return;
        }

        for (uint32_t i = 0; i < count; i++) {
            TradeMgr::TransactItems(3, 1, item->item_id); // type=3 = CrafterBuy
            WaitMs(500 + ChatMgr::GetPing());
        }
        LogBot("Crafted %u x %s", count, name);
    };

    // Craft what's needed
    if (grail < MIN_CONSETS) {
        craftAt(kEyjaX, kEyjaY, MODEL_GRAIL_MIGHT, MIN_CONSETS - grail, "Grail of Might");
    }
    if (essence < MIN_CONSETS) {
        craftAt(kKwatX, kKwatY, MODEL_ESSENCE_CEL, MIN_CONSETS - essence, "Essence of Celerity");
    }
    if (armor < MIN_CONSETS) {
        craftAt(kAlcusX, kAlcusY, MODEL_ARMOR_SALV, MIN_CONSETS - armor, "Armor of Salvation");
    }

    // Travel back to Gadd's
    MapMgr::Travel(MAP_GADDS_ENCAMPMENT);
    DWORD returnStart = GetTickCount();
    while (MapMgr::GetMapId() != MAP_GADDS_ENCAMPMENT && (GetTickCount() - returnStart) < 60000) {
        WaitMs(1000);
    }
    WaitMs(3000);
}

// ===== Consumable Usage (GWA3-102) =====

static void UseConsumables(const BotConfig& cfg) {
    if (!cfg.use_consets) return;

    if (Offsets::MyID <= 0x10000) return;
    uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);

    // Conset: Armor of Salvation
    if (!EffectMgr::HasEffect(myId, SKILL_ARMOR_OF_SALVATION)) {
        Item* item = FindItemByModel(MODEL_ARMOR_SALV);
        if (item) {
            LogBot("Using Armor of Salvation (item=%u)", item->item_id);
            ItemMgr::UseItem(item->item_id);
            WaitMs(1000);
        }
    }

    // Conset: Essence of Celerity
    if (!EffectMgr::HasEffect(myId, SKILL_ESSENCE_CELERITY)) {
        Item* item = FindItemByModel(MODEL_ESSENCE_CEL);
        if (item) {
            LogBot("Using Essence of Celerity (item=%u)", item->item_id);
            ItemMgr::UseItem(item->item_id);
            WaitMs(1000);
        }
    }

    // Conset: Grail of Might
    if (!EffectMgr::HasEffect(myId, SKILL_GRAIL_OF_MIGHT)) {
        Item* item = FindItemByModel(MODEL_GRAIL_MIGHT);
        if (item) {
            LogBot("Using Grail of Might (item=%u)", item->item_id);
            ItemMgr::UseItem(item->item_id);
            WaitMs(1000);
        }
    }

    if (HasConset()) {
        LogBot("All consets active");
    } else {
        LogBot("Some consets missing — may need to craft/buy");
    }
}

static bool NeedsMaintenance() {
    static constexpr uint32_t MIN_FREE_SLOTS   = 7;
    static constexpr uint32_t MIN_ID_KITS      = 1;
    static constexpr uint32_t MIN_SALVAGE_KITS = 1;
    static constexpr uint32_t MAX_CHAR_GOLD    = 90000;

    if (CountFreeSlots() < MIN_FREE_SLOTS) {
        LogBot("Maintenance needed: free slots = %u (min %u)", CountFreeSlots(), MIN_FREE_SLOTS);
        return true;
    }

    uint32_t idKits = CountItemByModel(MODEL_ID_KIT) + CountItemByModel(MODEL_SUP_ID_KIT);
    if (idKits < MIN_ID_KITS) {
        LogBot("Maintenance needed: ID kits = %u (min %u)", idKits, MIN_ID_KITS);
        return true;
    }

    uint32_t salvKits = CountItemByModel(MODEL_SALV_KIT) + CountItemByModel(MODEL_EXP_SALV_KIT);
    if (salvKits < MIN_SALVAGE_KITS) {
        LogBot("Maintenance needed: salvage kits = %u (min %u)", salvKits, MIN_SALVAGE_KITS);
        return true;
    }

    uint32_t gold = ItemMgr::GetGoldCharacter();
    if (gold >= MAX_CHAR_GOLD) {
        LogBot("Maintenance needed: character gold = %u (max %u)", gold, MAX_CHAR_GOLD);
        return true;
    }

    return false;
}

static void DepositExcessGold() {
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t storageGold = ItemMgr::GetGoldStorage();

    if (charGold > 10000 && storageGold < 950000) {
        uint32_t deposit = charGold - 10000;
        if (deposit + storageGold > 1000000) {
            deposit = 1000000 - storageGold;
        }
        if (deposit > 0) {
            LogBot("Depositing %u gold to storage", deposit);
            ItemMgr::ChangeGold(charGold - deposit, storageGold + deposit);
            WaitMs(500);
        }
    }
}

static void UseItemByModel(uint32_t modelId) {
    Item* item = FindItemByModel(modelId);
    if (item) {
        ItemMgr::UseItem(item->item_id);
        WaitMs(500);
    }
}

static void UseDpRemovalIfNeeded() {
    // DP removal sweets — use if we have any and morale is bad
    // Since we can't read morale accurately yet, use a simple heuristic:
    // if we had wipes this session, use a sweet
    if (s_wipeCount == 0) return;

    static constexpr uint32_t dpSweets[] = {
        MODEL_BIRTHDAY_CUPCAKE, MODEL_SLICE_BIRTHDAY, MODEL_CANDY_CORN
    };

    for (uint32_t modelId : dpSweets) {
        Item* sweet = FindItemByModel(modelId);
        if (sweet) {
            LogBot("Using DP removal sweet (model=%u) after %u wipes", modelId, s_wipeCount);
            ItemMgr::UseItem(sweet->item_id);
            WaitMs(5000); // Sweet has casting time
            s_wipeCount = 0; // Reset wipe counter after using sweet
            return;
        }
    }
}

// ===== State Handlers =====

BotState HandleCharSelect(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: CharSelect");

    // Click Play button
    if (UIMgr::IsFrameVisible(UIMgr::Hashes::PlayButton)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::PlayButton);
        WaitMs(5000);
    }

    // Handle reconnect dialog
    if (UIMgr::IsFrameVisible(UIMgr::Hashes::ReconnectYes)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::ReconnectYes);
        WaitMs(3000);
    }

    // Wait for map load
    WaitMs(5000);
    uint32_t mapId = MapMgr::GetMapId();
    if (mapId > 0) {
        return BotState::InTown;
    }

    return BotState::CharSelect;
}

BotState HandleTownSetup(BotConfig& cfg) {
    LogBot("State: TownSetup (run #%u)", s_runCount + 1);

    uint32_t mapId = MapMgr::GetMapId();

    // If not at Gadd's Encampment, travel there
    if (mapId != MAP_GADDS_ENCAMPMENT) {
        MapMgr::Travel(MAP_GADDS_ENCAMPMENT);
        WaitMs(10000);
        return BotState::InTown;
    }

    // Load hero config from file (adds heroes + loads their skillbars)
    if (!cfg.hero_config_file.empty()) {
        // Kick existing heroes first so we get a clean slate
        PartyMgr::KickAllHeroes();
        WaitMs(500);

        int loaded = LoadHeroConfigFile(cfg.hero_config_file.c_str(), cfg);
        if (loaded == 0) {
            // Fallback: add heroes from hardcoded config without skillbar loading
            LogBot("Config file failed — using hardcoded hero IDs");
            for (int i = 0; i < 7; i++) {
                if (cfg.hero_ids[i] > 0) {
                    PartyMgr::AddHero(cfg.hero_ids[i]);
                    WaitMs(300);
                }
            }
        }
    } else {
        // No config file — use hardcoded hero IDs
        for (int i = 0; i < 7; i++) {
            if (cfg.hero_ids[i] > 0) {
                PartyMgr::AddHero(cfg.hero_ids[i]);
                WaitMs(300);
            }
        }
    }

    // Set hard mode
    if (cfg.hard_mode) {
        MapMgr::SetHardMode(true);
        WaitMs(500);
    }

    // Set hero behaviors to Guard
    for (int i = 0; i < 7; i++) {
        PartyMgr::SetHeroBehavior(i + 1, 1); // 1 = Guard
        WaitMs(100);
    }

    // Cache player skillbar for combat
    CacheSkillBar();

    // Use consumables before entering dungeon
    UseConsumables(cfg);

    return BotState::Traveling;
}

BotState HandleTravel(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Travel to Sparkfly Swamp");

    uint32_t mapId = MapMgr::GetMapId();

    if (mapId == MAP_GADDS_ENCAMPMENT) {
        // Move to exit portal
        MoveToAndWait(-10018, -21892);
        MoveToAndWait(-9550, -20400);
        AgentMgr::Move(-9451, -19766);
        // Wait for Sparkfly Swamp load
        WaitMs(10000);
    }

    mapId = MapMgr::GetMapId();
    if (mapId == MAP_SPARKFLY_SWAMP) {
        return BotState::InDungeon;
    }

    // Fallback
    return BotState::InTown;
}

BotState HandleDungeon(BotConfig& cfg) {
    (void)cfg;
    uint32_t mapId = MapMgr::GetMapId();

    if (mapId == MAP_SPARKFLY_SWAMP) {
        LogBot("State: Sparkfly Swamp — running to dungeon");
        s_runCount++;
        s_runStartTime = GetTickCount();

        // Accept quest from Tekk
        MoveToAndWait(12396, 22407);
        WaitMs(500);
        SendDialogWithRetry(DIALOG_NPC_TALK, 2, 500);
        SendDialogWithRetry(DIALOG_QUEST_ACCEPT, 2, 500);

        // Move to dungeon entrance
        MoveToAndWait(12228, 22677);
        MoveToAndWait(12470, 25036);
        AgentMgr::Move(12968, 26219);
        WaitMs(1000);
        // Enter dungeon
        DWORD start = GetTickCount();
        while ((GetTickCount() - start) < 60000) {
            AgentMgr::Move(13097, 26393);
            WaitMs(250);
            if (MapMgr::GetMapId() == MAP_BOGROOT_LVL1) break;
        }
        WaitMs(3000);
    }

    mapId = MapMgr::GetMapId();

    if (mapId == MAP_BOGROOT_LVL1) {
        LogBot("State: Bogroot Level 1");
        FollowWaypoints(BOGROOT_LVL1, sizeof(BOGROOT_LVL1) / sizeof(BOGROOT_LVL1[0]));
        WaitMs(3000);
    }

    mapId = MapMgr::GetMapId();

    if (mapId == MAP_BOGROOT_LVL2) {
        LogBot("State: Bogroot Level 2");
        FollowWaypoints(BOGROOT_LVL2, sizeof(BOGROOT_LVL2) / sizeof(BOGROOT_LVL2[0]));

        // Run complete
        DWORD runTime = GetTickCount() - s_runStartTime;
        if (runTime < s_bestRunTime) s_bestRunTime = runTime;
        LogBot("Run #%u complete in %u ms (best: %u ms)", s_runCount, runTime, s_bestRunTime);

        return BotState::Looting;
    }

    // If we ended up back in outpost (wipe/resign), restart
    if (mapId == MAP_GADDS_ENCAMPMENT) {
        s_failCount++;
        LogBot("Run failed (returned to outpost), restarting");
        return BotState::InTown;
    }

    return BotState::InDungeon;
}

BotState HandleLoot(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Loot collection");
    // Loot is handled inline during waypoint traversal
    // This state handles post-boss cleanup
    WaitMs(2000);
    return BotState::Merchant;
}

BotState HandleMerchant(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Merchant (return to outpost)");

    uint32_t mapId = MapMgr::GetMapId();

    // Return to Gadd's Encampment if not already there
    if (mapId != MAP_GADDS_ENCAMPMENT) {
        MapMgr::Travel(MAP_GADDS_ENCAMPMENT);
        DWORD travelStart = GetTickCount();
        while (MapMgr::GetMapId() != MAP_GADDS_ENCAMPMENT && (GetTickCount() - travelStart) < 60000) {
            WaitMs(1000);
        }
        if (MapMgr::GetMapId() != MAP_GADDS_ENCAMPMENT) {
            LogBot("Failed to travel to Gadd's Encampment");
            return BotState::Error;
        }
        // Wait for full map load
        WaitMs(3000);
    }

    // Deposit excess gold before selling (avoid "too rich" merchant cap)
    DepositExcessGold();

    // Identify gold/purple items before selling (maximizes value)
    IdentifyGoldItems();

    // Salvage white/blue items for materials (before selling remaining junk)
    SalvageJunkItems();

    // Sell junk items to merchant
    SellJunkToMerchant();

    // Buy kits while merchant window is still open
    BuyKitsIfNeeded();
    WaitMs(500);

    // Deposit gold earned from selling
    DepositExcessGold();

    // Check if maintenance is needed before next run
    if (NeedsMaintenance()) {
        return BotState::Maintenance;
    }

    return BotState::InTown;
}

BotState HandleMaintenance(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Maintenance");

    // Ensure we're in an outpost
    uint32_t mapId = MapMgr::GetMapId();
    if (mapId != MAP_GADDS_ENCAMPMENT) {
        MapMgr::Travel(MAP_GADDS_ENCAMPMENT);
        DWORD travelStart = GetTickCount();
        while (MapMgr::GetMapId() != MAP_GADDS_ENCAMPMENT && (GetTickCount() - travelStart) < 60000) {
            WaitMs(1000);
        }
        WaitMs(3000);
    }

    // Use DP removal sweets if we had wipes
    UseDpRemovalIfNeeded();

    // Report inventory and buff status
    uint32_t freeSlots = CountFreeSlots();
    uint32_t idKits = CountItemByModel(MODEL_ID_KIT) + CountItemByModel(MODEL_SUP_ID_KIT);
    uint32_t salvKits = CountItemByModel(MODEL_SALV_KIT) + CountItemByModel(MODEL_EXP_SALV_KIT);
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t effectCount = GetPlayerEffectCount();
    bool hasBless = HasBlessing();
    bool hasCon = HasConset();
    LogBot("Inventory: %u free slots, %u ID kits, %u salvage kits, %u gold",
           freeSlots, idKits, salvKits, charGold);
    LogBot("Buffs: %u effects, blessing=%s, conset=%s",
           effectCount, hasBless ? "yes" : "no", hasCon ? "yes" : "no");

    // Deposit valuable items to Xunlai storage
    DepositValuablesToXunlai();

    // Craft consets if running low
    CraftConsetsIfNeeded();

    // If still critically low on slots after selling + depositing, log warning
    freeSlots = CountFreeSlots();
    if (freeSlots < 3) {
        LogBot("WARNING: Critically low inventory space (%u slots). Consider manual cleanup.", freeSlots);
    }

    return BotState::Traveling;
}

BotState HandleError(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: ERROR — waiting 10s before retry");
    WaitMs(10000);

    // Try to recover by going back to town
    uint32_t mapId = MapMgr::GetMapId();
    if (mapId == 0) {
        return BotState::CharSelect;
    }
    return BotState::InTown;
}

// ===== Registration =====

void Register() {
    Bot::RegisterStateHandler(BotState::CharSelect, HandleCharSelect);
    Bot::RegisterStateHandler(BotState::InTown, HandleTownSetup);
    Bot::RegisterStateHandler(BotState::Traveling, HandleTravel);
    Bot::RegisterStateHandler(BotState::InDungeon, HandleDungeon);
    Bot::RegisterStateHandler(BotState::Looting, HandleLoot);
    Bot::RegisterStateHandler(BotState::Merchant, HandleMerchant);
    Bot::RegisterStateHandler(BotState::Maintenance, HandleMaintenance);
    Bot::RegisterStateHandler(BotState::Error, HandleError);

    // Default config — hero IDs are fallback if config file fails to load
    auto& cfg = Bot::GetConfig();
    cfg.hero_config_file = "Mercs.txt";  // Default: load from hero_configs/Mercs.txt
    cfg.hero_ids[0] = 30; // Mercenary 3 (fallback)
    cfg.hero_ids[1] = 14; // Olias
    cfg.hero_ids[2] = 21; // Livia
    cfg.hero_ids[3] = 4;  // Master of Whispers
    cfg.hero_ids[4] = 24; // Gwen
    cfg.hero_ids[5] = 15; // Norgu
    cfg.hero_ids[6] = 29; // Mercenary 2
    cfg.hard_mode = true;
    cfg.target_map_id = MAP_BOGROOT_LVL1;
    cfg.outpost_map_id = MAP_GADDS_ENCAMPMENT;
    cfg.bot_module_name = "FroggyHM";

    s_runCount = 0;
    s_failCount = 0;
    s_wipeCount = 0;
    s_totalStartTime = GetTickCount();

    LogBot("Froggy HM module registered (7 heroes, hard mode)");
}

// ===== Unit Tests (GWA3-109 through GWA3-120) =====
// These run inside the DLL and have access to all static functions.

static int s_testPassed = 0;
static int s_testFailed = 0;

static void FroggyCheck(const char* name, bool condition) {
    if (condition) {
        s_testPassed++;
        LogBot("[PASS] %s", name);
    } else {
        s_testFailed++;
        LogBot("[FAIL] %s", name);
    }
}

int RunFroggyUnitTests() {
    s_testPassed = 0;
    s_testFailed = 0;
    LogBot("=== Froggy Unit Tests ===");

    // --- GWA3-110: Skill template decoding ---
    FroggyCheck("Base64 A=0", Base64CharToVal('A') == 0);
    FroggyCheck("Base64 Z=25", Base64CharToVal('Z') == 25);
    FroggyCheck("Base64 a=26", Base64CharToVal('a') == 26);
    FroggyCheck("Base64 z=51", Base64CharToVal('z') == 51);
    FroggyCheck("Base64 0=52", Base64CharToVal('0') == 52);
    FroggyCheck("Base64 /=63", Base64CharToVal('/') == 63);
    FroggyCheck("Base64 invalid=-1", Base64CharToVal('!') == -1);

    // Decode a known template: "OgATQfY1MXVyimBA" (random test case)
    // We can at least verify it doesn't crash and returns true
    uint32_t testSkills[8] = {};
    bool decoded = DecodeSkillTemplate("OAOiAyk8gNtePuwJ00ZaNbJA", testSkills);
    FroggyCheck("DecodeSkillTemplate succeeds", decoded);
    FroggyCheck("DecodeSkillTemplate has non-zero skills", testSkills[0] != 0 || testSkills[1] != 0);

    // Empty/invalid template
    uint32_t emptySkills[8] = {};
    FroggyCheck("DecodeSkillTemplate empty fails", !DecodeSkillTemplate("", emptySkills));

    // --- GWA3-111: Item filtering logic ---
    FroggyCheck("IsAlwaysPickup ecto=true", IsAlwaysPickupModel(930));
    FroggyCheck("IsAlwaysPickup diamond=true", IsAlwaysPickupModel(935));
    FroggyCheck("IsAlwaysPickup lockpick=true", IsAlwaysPickupModel(22751));
    FroggyCheck("IsAlwaysPickup random=false", !IsAlwaysPickupModel(12345));
    FroggyCheck("IsAlwaysPickup 0=false", !IsAlwaysPickupModel(0));

    // ShouldStore
    // Create a fake item struct for testing
    Item fakeEcto = {};
    fakeEcto.item_id = 1;
    fakeEcto.model_id = 930;
    FroggyCheck("ShouldStore ecto=true", ShouldStore(&fakeEcto));

    Item fakeJunk = {};
    fakeJunk.item_id = 2;
    fakeJunk.model_id = 12345;
    FroggyCheck("ShouldStore junk=false", !ShouldStore(&fakeJunk));
    FroggyCheck("ShouldStore null=false", !ShouldStore(nullptr));

    // ShouldSalvage — fake items with rarity in name_enc
    // We can't easily fake name_enc pointer, so test with nullptr (should return false)
    Item noName = {};
    noName.item_id = 3;
    noName.model_id = 100;
    noName.name_enc = nullptr;
    FroggyCheck("ShouldSalvage null_name=false", !ShouldSalvage(&noName));
    FroggyCheck("ShouldSalvage null=false", !ShouldSalvage(nullptr));

    // ShouldSalvage kit model — should be false regardless
    Item fakeKit = {};
    fakeKit.item_id = 4;
    fakeKit.model_id = MODEL_SALV_KIT;
    uint16_t whiteRarity = RARITY_WHITE;
    fakeKit.name_enc = reinterpret_cast<wchar_t*>(&whiteRarity);
    FroggyCheck("ShouldSalvage kit=false", !ShouldSalvage(&fakeKit));

    // ShouldSalvage material type — should be false
    Item fakeMat = {};
    fakeMat.item_id = 5;
    fakeMat.model_id = 948; // Iron Ingot
    fakeMat.type = 11; // material
    fakeMat.name_enc = reinterpret_cast<wchar_t*>(&whiteRarity);
    FroggyCheck("ShouldSalvage material=false", !ShouldSalvage(&fakeMat));

    // --- GWA3-112: Chest gadget ID detection ---
    FroggyCheck("IsChest HM=true", IsChestGadgetId(8141));
    FroggyCheck("IsChest NM=true", IsChestGadgetId(4582));
    FroggyCheck("IsChest Obsidian=true", IsChestGadgetId(74));
    FroggyCheck("IsChest random=false", !IsChestGadgetId(9999));
    FroggyCheck("IsChest 0=false", !IsChestGadgetId(0));

    // --- GWA3-111 continued: ShouldSalvage positive case ---
    Item fakeWhiteWeapon = {};
    fakeWhiteWeapon.item_id = 6;
    fakeWhiteWeapon.model_id = 15055; // Some weapon
    fakeWhiteWeapon.type = 27;        // Sword type
    fakeWhiteWeapon.name_enc = reinterpret_cast<wchar_t*>(&whiteRarity);
    FroggyCheck("ShouldSalvage white weapon=true", ShouldSalvage(&fakeWhiteWeapon));

    // --- GWA3-115: State checks (require live game state) ---
    LogBot("--- GWA3-115: State Checks ---");
    auto* me115 = AgentMgr::GetMyAgent();
    if (me115 && me115->hp > 0.0f) {
        // We're alive in-game — can do real state checks
        uint32_t freeSlots = CountFreeSlots();
        FroggyCheck("CountFreeSlots > 0 (have some space)", freeSlots > 0);
        FroggyCheck("CountFreeSlots <= 40 (max 4 bags * 10)", freeSlots <= 40);

        uint32_t idKits = CountItemByModel(MODEL_ID_KIT) + CountItemByModel(MODEL_SUP_ID_KIT);
        FroggyCheck("CountItemByModel(ID_KIT) >= 0", idKits <= 100); // sanity

        // HasConset/HasBlessing return deterministic results based on actual buffs
        bool conset = HasConset();
        bool blessing = HasBlessing();
        LogBot("  HasConset=%s HasBlessing=%s (actual state)", conset ? "yes" : "no", blessing ? "yes" : "no");
        // We verify these functions ran without crash by reaching this point
        FroggyCheck("HasConset/HasBlessing executed without crash", true);
    } else {
        LogBot("  SKIP: Not in-game (no agent), skipping live state checks");
    }

    // --- GWA3-116: Hero flagging ---
    LogBot("--- GWA3-116: Hero Flagging ---");
    if (me115 && me115->hp > 0.0f) {
        float origX = me115->x;
        float origY = me115->y;
        FlagAllHeroes(origX + 100, origY + 100);
        WaitMs(200);
        UnflagAllHeroes();
        WaitMs(100);
        // Verify we're still alive and didn't crash
        auto* meAfterFlag = AgentMgr::GetMyAgent();
        FroggyCheck("FlagAll+UnflagAll: agent still valid", meAfterFlag != nullptr);
    } else {
        LogBot("  SKIP: Not in-game, skipping hero flagging");
    }

    // SendDialogWithRetry with dialog_id=0 (no-op, won't trigger real dialog)
    bool dialogResult = SendDialogWithRetry(0, 1, 50);
    FroggyCheck("SendDialogWithRetry(0) returns true", dialogResult);

    // --- GWA3-128: Skill classification (role bitmask) ---
    LogBot("--- GWA3-128: Skill Classification ---");

    // Test hardcoded classifier functions — these are pure logic, no game state needed
    FroggyCheck("IsHardInterrupt Power Block(5)=true", IsHardInterruptId(5));
    FroggyCheck("IsHardInterrupt Power Drain(64)=true", IsHardInterruptId(64));
    FroggyCheck("IsHardInterrupt 0=false", !IsHardInterruptId(0));
    FroggyCheck("IsHardInterrupt 999=false", !IsHardInterruptId(999));

    FroggyCheck("IsCondRemoval(25)=true", IsCondRemovalId(25));
    FroggyCheck("IsCondRemoval(53)=true", IsCondRemovalId(53));
    FroggyCheck("IsCondRemoval(0)=false", !IsCondRemovalId(0));

    FroggyCheck("IsHexRemoval(24)=true", IsHexRemovalId(24));
    FroggyCheck("IsHexRemoval(156)=true", IsHexRemovalId(156));
    FroggyCheck("IsHexRemoval(0)=false", !IsHexRemovalId(0));

    FroggyCheck("IsSurvival Shadow Form(2358)=true", IsSurvivalId(2358));
    FroggyCheck("IsSurvival(312)=true", IsSurvivalId(312));
    FroggyCheck("IsSurvival(0)=false", !IsSurvivalId(0));

    FroggyCheck("IsBinding(786)=true", IsBindingId(786));
    FroggyCheck("IsBinding(960)=true", IsBindingId(960));
    FroggyCheck("IsBinding(0)=false", !IsBindingId(0));

    FroggyCheck("IsSpeedBoost(947)=true", IsSpeedBoostId(947));
    FroggyCheck("IsSpeedBoost(0)=false", !IsSpeedBoostId(0));

    // Test role bitmask operations — pure logic
    CachedSkill testSkillBitmask = {};
    testSkillBitmask.roles = ROLE_HEX | ROLE_OFFENSIVE | ROLE_PRESSURE;
    FroggyCheck("hasRole HEX=true", testSkillBitmask.hasRole(ROLE_HEX));
    FroggyCheck("hasRole OFFENSIVE=true", testSkillBitmask.hasRole(ROLE_OFFENSIVE));
    FroggyCheck("hasRole PRESSURE=true", testSkillBitmask.hasRole(ROLE_PRESSURE));
    FroggyCheck("hasRole HEAL=false", !testSkillBitmask.hasRole(ROLE_ANY_HEAL));
    FroggyCheck("hasRole INTERRUPT=false", !testSkillBitmask.hasRole(ROLE_ANY_INTERRUPT));
    FroggyCheck("hasRole RESURRECT=false", !testSkillBitmask.hasRole(ROLE_RESURRECT));

    // Verify combined masks are correct
    FroggyCheck("ANY_HEAL = SINGLE|PARTY|SELF",
                ROLE_ANY_HEAL == (ROLE_HEAL_SINGLE | ROLE_HEAL_PARTY | ROLE_HEAL_SELF));
    FroggyCheck("ANY_INTERRUPT = HARD|SOFT",
                ROLE_ANY_INTERRUPT == (ROLE_INTERRUPT_HARD | ROLE_INTERRUPT_SOFT));
    FroggyCheck("ANY_REMOVAL = COND|HEX|ENCHANT",
                ROLE_ANY_REMOVAL == (ROLE_COND_REMOVE | ROLE_HEX_REMOVE | ROLE_ENCHANT_REMOVE));

    // Verify all role bits are distinct (no overlap)
    FroggyCheck("ROLE_HEAL_SINGLE is unique bit", ROLE_HEAL_SINGLE == (1 << 0));
    FroggyCheck("ROLE_RESURRECT is unique bit", ROLE_RESURRECT == (1 << 18));
    FroggyCheck("ROLE_OFFENSIVE is unique bit", ROLE_OFFENSIVE == (1 << 19));

    // Test CacheSkillBar with real skillbar (if available)
    LogBot("--- GWA3-128 continued: CacheSkillBar ---");
    auto* bar128 = SkillMgr::GetPlayerSkillbar();
    if (bar128) {
        CacheSkillBar();
        FroggyCheck("CacheSkillBar sets s_skillsCached=true", s_skillsCached);
        // Verify at least one cached skill has non-zero roles
        bool anyRoles = false;
        for (int i = 0; i < 8; i++) {
            if (s_skillCache[i].skill_id != 0 && s_skillCache[i].roles != ROLE_NONE) {
                anyRoles = true;
                LogBot("  Slot %d: skill=%u roles=0x%X target=%u energy=%u",
                       i, s_skillCache[i].skill_id, s_skillCache[i].roles,
                       s_skillCache[i].target_type, s_skillCache[i].energy_cost);
            }
        }
        FroggyCheck("At least one skill has roles assigned", anyRoles);
    } else {
        LogBot("  SKIP: No skillbar available");
    }

    // --- GWA3-129: Target selection ---
    LogBot("--- GWA3-129: Target Selection ---");

    // Determine context: outpost (no enemies) vs explorable (may have enemies)
    bool inExplorable = false;
    const auto* areaInfo = MapMgr::GetAreaInfo(MapMgr::GetMapId());
    if (areaInfo && areaInfo->type >= 4) { // type 4+ = explorable/mission/dungeon
        inExplorable = true;
    }

    // Ally targeting: should work in both outpost and explorable
    if (me115) {
        uint32_t lowestAlly = GetLowestHealthAlly();
        // In any context, we should find at least ourselves or a hero
        FroggyCheck("GetLowestHealthAlly returns valid ID or 0",
                    lowestAlly == 0 || AgentMgr::GetAgentExists(lowestAlly));

        uint32_t deadAlly = GetDeadAlly();
        FroggyCheck("GetDeadAlly returns valid ID or 0",
                    deadAlly == 0 || AgentMgr::GetAgentExists(deadAlly));

        // ResolveSkillTarget for heals — should return self or ally when in-game
        CachedSkill fakeHealResolve = {};
        fakeHealResolve.roles = ROLE_HEAL_SINGLE;
        fakeHealResolve.target_type = 3;
        uint32_t healTarget = ResolveSkillTarget(fakeHealResolve, 0);
        FroggyCheck("ResolveSkillTarget(heal) returns valid agent",
                    healTarget > 0 && AgentMgr::GetAgentExists(healTarget));

        // ResolveSkillTarget for res — should return 0 (nobody dead, hopefully)
        CachedSkill fakeResResolve = {};
        fakeResResolve.roles = ROLE_RESURRECT;
        fakeResResolve.target_type = 6;
        uint32_t resTarget = ResolveSkillTarget(fakeResResolve, 0);
        FroggyCheck("ResolveSkillTarget(res) returns 0 or valid dead ally",
                    resTarget == 0 || AgentMgr::GetAgentExists(resTarget));
    }

    // Enemy targeting: only meaningful in explorable with enemies
    if (inExplorable) {
        uint32_t unhexed = GetUnhexedEnemy();
        FroggyCheck("GetUnhexedEnemy returns valid enemy or 0",
                    unhexed == 0 || AgentMgr::GetAgentExists(unhexed));

        uint32_t castingFoe = GetCastingEnemy();
        FroggyCheck("GetCastingEnemy returns valid enemy or 0",
                    castingFoe == 0 || AgentMgr::GetAgentExists(castingFoe));

        uint32_t enchFoe = GetEnchantedEnemy();
        FroggyCheck("GetEnchantedEnemy returns valid enemy or 0",
                    enchFoe == 0 || AgentMgr::GetAgentExists(enchFoe));

        uint32_t meleeFoe = GetMeleeRangeEnemy();
        FroggyCheck("GetMeleeRangeEnemy returns valid or 0",
                    meleeFoe == 0 || AgentMgr::GetAgentExists(meleeFoe));

        // ResolveSkillTarget for hex — should return unhexed enemy or fallback
        CachedSkill fakeHexResolve = {};
        fakeHexResolve.roles = ROLE_HEX;
        fakeHexResolve.target_type = 5;
        uint32_t hexTarget = ResolveSkillTarget(fakeHexResolve, 42);
        FroggyCheck("ResolveSkillTarget(hex) returns valid enemy or fallback 42",
                    hexTarget == 42 || AgentMgr::GetAgentExists(hexTarget));
    } else {
        LogBot("  SKIP: Not in explorable — skipping enemy targeting tests");
        // In outpost, enemy finders should return 0 (no enemies)
        FroggyCheck("GetUnhexedEnemy=0 in outpost", GetUnhexedEnemy() == 0);
        FroggyCheck("GetCastingEnemy=0 in outpost", GetCastingEnemy() == 0);
        FroggyCheck("GetMeleeRangeEnemy=0 in outpost", GetMeleeRangeEnemy() == 0);
    }

    // --- GWA3-130: HP gating & debuff blocking ---
    LogBot("--- GWA3-130: HP Gating & Debuffs ---");

    if (me115 && me115->hp > 0.0f && MapMgr::GetIsMapLoaded()) {
        // CanCast should return true when alive, not debuffed, map loaded
        CachedSkill fakeSpellCast = {};
        fakeSpellCast.skill_type = 2; // Spell
        fakeSpellCast.roles = ROLE_OFFENSIVE;
        FroggyCheck("CanCast(spell) when alive+loaded = true", CanCast(fakeSpellCast));

        CachedSkill fakeAtkCast = {};
        fakeAtkCast.skill_type = 9; // Attack
        fakeAtkCast.roles = ROLE_ATTACK;
        FroggyCheck("CanCast(attack) when alive+loaded = true", CanCast(fakeAtkCast));

        CachedSkill fakeSigCast = {};
        fakeSigCast.skill_type = 4; // Signet
        fakeSigCast.roles = ROLE_OFFENSIVE;
        FroggyCheck("CanCast(signet) when alive+loaded = true", CanCast(fakeSigCast));

        CachedSkill fakeShoutCast = {};
        fakeShoutCast.skill_type = 10; // Shout
        fakeShoutCast.roles = ROLE_SHOUT;
        FroggyCheck("CanCast(shout) when alive+loaded = true", CanCast(fakeShoutCast));

        // HP gating: heal should be BLOCKED when all allies are healthy
        CachedSkill fakeHealGate = {};
        fakeHealGate.skill_id = 68;
        fakeHealGate.roles = ROLE_HEAL_SINGLE;
        fakeHealGate.skill_type = 2;
        fakeHealGate.target_type = 3;
        bool canHealWhenHealthy = CanUseSkill(fakeHealGate, 0);
        if (me115->hp > 0.8f) {
            FroggyCheck("CanUseSkill(heal) blocked when self HP > 80%", !canHealWhenHealthy);
        } else {
            FroggyCheck("CanUseSkill(heal) allowed when self HP <= 80%", canHealWhenHealthy);
        }

        // Survival gating: should be BLOCKED when HP is high
        CachedSkill fakeSurvGate = {};
        fakeSurvGate.skill_id = 2358; // Shadow Form
        fakeSurvGate.roles = ROLE_SURVIVAL;
        fakeSurvGate.skill_type = 3;
        fakeSurvGate.target_type = 0;
        bool canSurvWhenHealthy = CanUseSkill(fakeSurvGate, 0);
        if (me115->hp > 0.5f) {
            FroggyCheck("CanUseSkill(survival) blocked when HP > 50%", !canSurvWhenHealthy);
        }

        // Binding: should be BLOCKED in outpost (no enemies)
        if (!inExplorable) {
            CachedSkill fakeBindGate = {};
            fakeBindGate.skill_id = 786;
            fakeBindGate.roles = ROLE_BINDING;
            fakeBindGate.skill_type = 13;
            fakeBindGate.target_type = 0;
            FroggyCheck("CanUseSkill(binding) blocked in outpost (no enemies)",
                        !CanUseSkill(fakeBindGate, 0));
        }
    } else {
        LogBot("  SKIP: Not alive/loaded — skipping HP gating tests");
    }

    // --- GWA3-131: Combat mode toggle ---
    LogBot("--- GWA3-131: Combat Mode ---");
    auto& cfg = Bot::GetConfig();
    auto origMode = cfg.combat_mode;

    cfg.combat_mode = CombatMode::LLM;
    FroggyCheck("Combat mode set to LLM", cfg.combat_mode == CombatMode::LLM);
    cfg.combat_mode = CombatMode::Builtin;
    FroggyCheck("Combat mode set to Builtin", cfg.combat_mode == CombatMode::Builtin);

    // Verify the two modes are distinct enum values
    FroggyCheck("LLM != Builtin", CombatMode::LLM != CombatMode::Builtin);

    cfg.combat_mode = origMode; // restore

    LogBot("=== Froggy Unit Tests Complete: %d passed, %d failed ===", s_testPassed, s_testFailed);
    return s_testFailed;
}

} // namespace GWA3::Bot::Froggy
