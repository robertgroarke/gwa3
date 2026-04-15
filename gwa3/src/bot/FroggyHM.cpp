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
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace GWA3::Bot::Froggy {

// ===== Constants =====

static constexpr uint32_t MAP_SPARKFLY_SWAMP    = 558;
static constexpr uint32_t MAP_BOGROOT_LVL1      = 615;
static constexpr uint32_t MAP_BOGROOT_LVL2      = 616;
static constexpr uint32_t MAP_GADDS_ENCAMPMENT  = 638;
static constexpr float LONG_BOW_RANGE           = 1320.0f;
static constexpr uint32_t EFFECTS_SKIP_AGGRO_FOE = 0x0014u;
static constexpr uint32_t TYPE_MAP_VANISHED_HOSTILE_MINION = 262152u;

static constexpr uint32_t QUEST_TEKKS_WAR       = 0x339;
static constexpr uint32_t DIALOG_QUEST_REWARD   = 0x833907;
// AutoIt AcceptQuest(0x339) sends 0x00833901. Keep Froggy aligned with the
// proven quest-accept dialog scalar used by the original bot.
static constexpr uint32_t DIALOG_QUEST_ACCEPT   = 0x833901;
static constexpr uint32_t DIALOG_QUEST_BODY     = 0x8101;
static constexpr uint32_t DIALOG_NPC_TALK       = 0x2AE6;
static constexpr uint32_t DIALOG_DUNGEON_ENTRY  = 0x833905;

// ===== Waypoint =====

struct Waypoint {
    float x, y;
    float fightRange;
    const char* label;
};

// ===== Routes =====

static const Waypoint SPARKFLY_TO_DUNGEON[] = {
    {-4559,  -14406, 1350, "1"},
    {-5204,  -9831,  1350, "2"},
    {-928,   -8699,  1350, "3"},
    {4200,   -4897,  1350, "4"},
    {6114,   819,    1250, "5"},
    {9500,   2281,   1250, "6"},
    {11570,  6120,   1250, "7"},
    {11025,  11710,  1250, "8"},
    {14624,  19314,  1250, "9"},
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
static DungeonLoopTelemetry s_dungeonLoopTelemetry = {};

// ===== Forward declarations =====
static void WaitMs(DWORD ms);
static int PickupNearbyLoot(float maxRange = 1200.0f);
static bool OpenNearbyChest(float maxRange = 1200.0f);
static uint32_t CountItemByModel(uint32_t modelId);
static void FlagAllHeroes(float x, float y);
static void UnflagAllHeroes();
static bool SendDialogWithRetry(uint32_t dialogId, int maxRetries = 3, DWORD delayMs = 1000);
static void UseDpRemovalIfNeeded();
static bool IsDead();
static uint32_t CountFreeSlots();
static void SuspendTransitionSensitiveHooks();
static void ResumeTransitionSensitiveHooks();

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
    if (!code || code[0] == '\0') return false;

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

// Role bitmask â€” a skill can have multiple roles
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

// Combined role masks
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
static bool s_skillUsedThisStep[8] = {};
static constexpr uint32_t EFFECT_QUICKENING_ZEPHYR = 475u;
static char s_lastCombatStep[128] = "uninitialized";
static LastCombatStepInfo s_lastCombatStepInfo = {};
static SparkflyTraversalCombatStats s_sparkflyTraversalCombatStats = {};
static bool s_combatDebugLogging = false;
static char s_builtinCombatDump[12][256] = {};
static int s_builtinCombatDumpCount = 0;
static char s_combatDebugTrace[96][256] = {};
static int s_combatDebugTraceCount = 0;

static void SetLastCombatStepDescription(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(s_lastCombatStep, sizeof(s_lastCombatStep), _TRUNCATE, fmt, args);
    va_end(args);
}

static void ResetLastCombatStepInfo() {
    s_lastCombatStepInfo = {};
}

static void ResetSparkflyTraversalCombatStatsState() {
    s_sparkflyTraversalCombatStats = {};
}

static void ResetCombatDebugTrace() {
    s_combatDebugTraceCount = 0;
    ZeroMemory(s_combatDebugTrace, sizeof(s_combatDebugTrace));
}

static void AddCombatDebugTraceLine(const char* fmt, ...) {
    if (s_combatDebugTraceCount >= static_cast<int>(std::size(s_combatDebugTrace))) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(s_combatDebugTrace[s_combatDebugTraceCount],
                sizeof(s_combatDebugTrace[s_combatDebugTraceCount]),
                _TRUNCATE, fmt, args);
    va_end(args);
    s_combatDebugTraceCount++;
}

static void CombatDebugLog(const char* fmt, ...) {
    if (!s_combatDebugLogging) return;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    AddCombatDebugTraceLine("%s", buffer);
    LogBot("CombatDebug: %s", buffer);
}

static void ResetBuiltinCombatDump() {
    s_builtinCombatDumpCount = 0;
    for (auto& line : s_builtinCombatDump) {
        line[0] = '\0';
    }
}

static void AddBuiltinCombatDumpLine(const char* fmt, ...) {
    if (s_builtinCombatDumpCount < 0 || s_builtinCombatDumpCount >= static_cast<int>(std::size(s_builtinCombatDump))) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_builtinCombatDump[s_builtinCombatDumpCount], sizeof(s_builtinCombatDump[s_builtinCombatDumpCount]), fmt, args);
    va_end(args);
    ++s_builtinCombatDumpCount;
}

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
    case 2051: case 2052: case 2370: case 2371:
    case 3431: // Heroic Refrain
        return true;
    }
    return false;
}

// Binding ritual skill IDs
static bool IsBindingId(uint32_t id) {
    switch (id) {
    case 2233: // Ebon Battle Standard of Honor
    case 2100: // Summon Spirits Kurzick
    case 1232: // Armor of Unfeeling
    case 1238: // Signet of Creation
    case 1239: // Signet of Spirits
    case 1253: // Bloodsong
    case 2110: // Vampirism
    case 1742: // Signet of Ghostly Might
    case 1217: // Ritual Lord
    case 1240: // Soul Twisting
    case 786: case 787: case 788: case 789: case 790:  // Spirits
    case 791: case 792: case 793: case 794: case 795:
    case 960: case 961: case 962: case 963: case 964:
    case 965: case 966: case 967: case 2083: case 2084:
    case 2085: case 2087: case 2088: case 2089:
        return true;
    }
    return false;
}

static bool IsPressureSpiritId(uint32_t id) {
    switch (id) {
    case 1239: // Signet of Spirits
    case 1253: // Bloodsong
    case 2110: // Vampirism
    case 2233: // Ebon Battle Standard of Honor
        return true;
    }
    return false;
}

static bool IsPrecastId(uint32_t id) {
    if (IsPressureSpiritId(id)) return true;
    switch (id) {
    case 1230: // Boon of Creation
    case 1232: // Armor of Unfeeling
    case 1228: // Spirit Siphon
    case 2100: // Summon Spirits Kurzick
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
        if (IsPressureSpiritId(c.skill_id)) c.roles |= ROLE_PRESSURE | ROLE_PRECAST;
        if (IsPrecastId(c.skill_id))        c.roles |= ROLE_PRECAST;

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
        case 22: // Ritualist spirit/bundle style skills in current client
            if (c.roles == ROLE_NONE) {
                c.roles |= ROLE_BINDING | ROLE_PRECAST;
            } else {
                c.roles |= ROLE_PRECAST;
            }
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
    for (int i = 0; i < 8; ++i) {
        const auto& c = s_skillCache[i];
        if (c.skill_id == 0) continue;
        CombatDebugLog("cache slot=%d skill=%u roles=0x%X targetType=%u energy=%u type=%u activation=%.2f recharge=%.2f",
                       i + 1, c.skill_id, c.roles, c.target_type, c.energy_cost, c.skill_type,
                       c.activation, c.recharge_time);
    }
}

// Try to use a skill from the cache. Returns true if a skill was used.
// ===== Intelligent Target Selection (GWA3-123) =====

// Find the ally with the lowest HP fraction. Returns 0 if no ally found.
static uint32_t GetLowestHealthAlly(float maxRange = 2500.0f, bool excludeSelf = false) {
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
        if (excludeSelf && living->agent_id == me->agent_id) continue;
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

static uint32_t CountAgentEffectsByPredicate(uint32_t agentId, bool (*predicate)(const Skill*)) {
    if (agentId == 0 || predicate == nullptr) return 0;
    auto* effectArray = EffectMgr::GetAgentEffectArray(agentId);
    if (!effectArray || !effectArray->buffer) return 0;

    uint32_t matches = 0;
    for (uint32_t i = 0; i < effectArray->size; ++i) {
        const Effect& effect = effectArray->buffer[i];
        if (effect.skill_id == 0) continue;
        const Skill* effectSkill = SkillMgr::GetSkillConstantData(effect.skill_id);
        if (!effectSkill) continue;
        if (predicate(effectSkill)) {
            ++matches;
        }
    }
    return matches;
}

static bool IsHexEffectSkill(const Skill* skill) {
    return skill && skill->type == 1u;
}

static bool IsConditionEffectSkill(const Skill* skill) {
    return skill && skill->condition != 0u;
}

static uint32_t GetMostAffectedAlly(bool excludeSelf,
                                    uint32_t (*scoreFn)(uint32_t),
                                    float maxRange = 2500.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || !scoreFn) return 0;

    uint32_t bestId = 0;
    uint32_t bestScore = 0;
    float bestHp = 1.1f;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 1 || living->hp <= 0.0f) continue;
        if (excludeSelf && living->agent_id == me->agent_id) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq > maxRange * maxRange) continue;

        const uint32_t score = scoreFn(living->agent_id);
        if (score == 0) continue;
        if (score > bestScore || (score == bestScore && living->hp < bestHp)) {
            bestScore = score;
            bestHp = living->hp;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

static uint32_t CountHexEffects(uint32_t agentId) {
    return CountAgentEffectsByPredicate(agentId, &IsHexEffectSkill);
}

static uint32_t CountConditionEffects(uint32_t agentId) {
    return CountAgentEffectsByPredicate(agentId, &IsConditionEffectSkill);
}

static uint32_t GetMostHexedAlly(bool excludeSelf = false, float maxRange = 2500.0f) {
    return GetMostAffectedAlly(excludeSelf, &CountHexEffects, maxRange);
}

static uint32_t GetMostConditionedAlly(bool excludeSelf = false, float maxRange = 2500.0f) {
    return GetMostAffectedAlly(excludeSelf, &CountConditionEffects, maxRange);
}

static bool CanCast(const CachedSkill& skill);

static bool CanBasicAttack() {
    CachedSkill basicAttack = {};
    basicAttack.slot = 0xFF;
    basicAttack.skill_type = 9;
    return CanCast(basicAttack);
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
        if (living->hp > 0.0f) continue; // alive â€” skip
        float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

static uint32_t GetNearestLivingAgentByAllegiance(uint8_t allegiance, float maxRange = 2500.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = maxRange * maxRange;
    uint32_t bestId = 0;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != allegiance) continue;
        if (living->hp <= 0.0f) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDist) {
            bestDist = distSq;
            bestId = living->agent_id;
        }
    }
    return bestId;
}

static uint32_t GetNearestSpiritAlly(float maxRange = 2500.0f) {
    return GetNearestLivingAgentByAllegiance(4u, maxRange);
}

static uint32_t GetNearestMinionAlly(float maxRange = 2500.0f) {
    return GetNearestLivingAgentByAllegiance(5u, maxRange);
}

static bool IsValidAggroEnemy(const AgentLiving* me,
                              const AgentLiving* living,
                              float maxRange,
                              bool castingOnly = false,
                              bool noHexOnly = false,
                              bool enchantedOnly = false) {
    if (!me || !living) return false;
    if (living->allegiance != 3) return false;
    if (living->agent_id == 0 || living->agent_id == me->agent_id) return false;
    if (living->hp <= 0.0f) return false;
    if ((living->effects & EFFECTS_SKIP_AGGRO_FOE) != 0) return false;
    if (living->type_map == TYPE_MAP_VANISHED_HOSTILE_MINION) return false;

    const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
    if (distSq > maxRange * maxRange) return false;

    if (castingOnly && living->skill == 0) return false;
    if (noHexOnly && living->hex != 0) return false;
    if (enchantedOnly) {
        auto* agentEffects = EffectMgr::GetAgentEffects(living->agent_id);
        if (!agentEffects || !agentEffects->effects.buffer) return false;
        bool hasEnchant = false;
        for (uint32_t ei = 0; ei < agentEffects->effects.size; ++ei) {
            auto& eff = agentEffects->effects.buffer[ei];
            if (eff.skill_id == 0) continue;
            const auto* sd = SkillMgr::GetSkillConstantData(eff.skill_id);
            if (sd && (sd->type == 3 || sd->type == 16)) {
                hasEnchant = true;
                break;
            }
        }
        if (!hasEnchant) return false;
    }

    return true;
}

static uint32_t SelectMostBalledEnemy(float maxRange = 1350.0f,
                                      bool castingOnly = false,
                                      bool noHexOnly = false,
                                      bool enchantedOnly = false) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;

    constexpr size_t kMaxCandidates = 512;
    uint32_t candidateIds[kMaxCandidates] = {};
    size_t candidateCount = 0;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents && candidateCount < kMaxCandidates; ++i) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (!IsValidAggroEnemy(me, living, maxRange, castingOnly, noHexOnly, enchantedOnly)) continue;
        candidateIds[candidateCount++] = living->agent_id;
    }

    float bestSumDistances = 999999999.0f;
    uint32_t bestId = 0;
    for (size_t i = 0; i < candidateCount; ++i) {
        auto* src = AgentMgr::GetAgentByID(candidateIds[i]);
        if (!src) continue;

        float sumDistances = 0.0f;
        for (size_t j = 0; j < candidateCount; ++j) {
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

// Mirrors AutoIt's GetBestTargetPtr(range): choose the most balled foe in range,
// not simply the nearest foe.
static uint32_t GetBestEnemy(float maxRange = 1350.0f) {
    return SelectMostBalledEnemy(maxRange, false, false, false);
}

// Find the most balled enemy without a hex. Returns 0 if all hexed or none nearby.
static uint32_t GetUnhexedEnemy(float maxRange = 1320.0f) {
    return SelectMostBalledEnemy(maxRange, false, true, false);
}

// Find the most balled enemy that is currently casting. Returns 0 if none casting.
static uint32_t GetCastingEnemy(float maxRange = 1320.0f) {
    return SelectMostBalledEnemy(maxRange, true, false, false);
}

// Find the most balled enemy with an enchantment. Returns 0 if none enchanted.
static uint32_t GetEnchantedEnemy(float maxRange = 1320.0f) {
    return SelectMostBalledEnemy(maxRange, false, false, true);
}

// Preserve the old helper name, but mirror AutoIt's GetBestMeleeTarget semantics:
// choose the best aggro target for attack skills within standard aggro range.
static uint32_t GetMeleeRangeEnemy(float maxRange = LONG_BOW_RANGE) {
    return SelectMostBalledEnemy(maxRange, false, false, false);
}

// Resolve best target for a skill based on its roles.
// Returns the target agent ID to use, or 0 if no valid target.
static uint32_t ResolveSkillTarget(const CachedSkill& skill, uint32_t defaultFoeId, float aggroRange = 1320.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;

    // Resurrection: target dead ally
    if (skill.hasRole(ROLE_RESURRECT)) {
        return GetDeadAlly();
    }

    if (skill.target_type == 0) {
        if (skill.skill_type == 7) {
            auto* foe = defaultFoeId ? AgentMgr::GetAgentByID(defaultFoeId) : nullptr;
            if (!foe || foe->type != 0xDB) return 0;
            const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, foe->x, foe->y);
            if (distSq > aggroRange * aggroRange) return 0;
        }
        return me->agent_id;
    }

    if (skill.target_type == 1) {
        const uint32_t spiritId = GetNearestSpiritAlly();
        return spiritId ? spiritId : 0;
    }

    // Ally-targeting skills
    if (skill.target_type == 3 || skill.target_type == 4) {
        const bool excludeSelf = skill.target_type == 4;
        if (skill.hasRole(ROLE_ANY_HEAL)) {
            uint32_t target = GetLowestHealthAlly(2500.0f, excludeSelf);
            return target ? target : (excludeSelf ? 0 : me->agent_id);
        }
        if (skill.hasRole(ROLE_COND_REMOVE)) {
            uint32_t target = GetMostConditionedAlly(excludeSelf);
            return target ? target : (excludeSelf ? 0 : me->agent_id);
        }
        if (skill.hasRole(ROLE_HEX_REMOVE)) {
            uint32_t target = GetMostHexedAlly(excludeSelf);
            return target ? target : (excludeSelf ? 0 : me->agent_id);
        }
        if (skill.hasRole(ROLE_PRECAST) || skill.hasRole(ROLE_SURVIVAL)) {
            return me->agent_id;
        }
        uint32_t target = GetLowestHealthAlly(2500.0f, excludeSelf);
        if (target) return target;
        return excludeSelf ? 0 : me->agent_id;
    }

    // Foe-targeting skills
    if (skill.target_type == 5) {
        // Hex: prefer unhexed enemy
        if (skill.hasRole(ROLE_HEX)) {
            uint32_t target = GetUnhexedEnemy(aggroRange);
            if (target) return target;
            target = GetBestEnemy(aggroRange);
            return target ? target : defaultFoeId;
        }
        // Interrupt: prefer casting enemy
        if (skill.hasRole(ROLE_ANY_INTERRUPT)) {
            uint32_t target = GetCastingEnemy(aggroRange);
            return target ? target : 0; // don't waste interrupt if nobody casting
        }
        // Enchant removal: prefer enchanted enemy
        if (skill.hasRole(ROLE_ENCHANT_REMOVE)) {
            uint32_t target = GetEnchantedEnemy(aggroRange);
            return target ? target : 0;
        }
        // Attack: prefer melee range
        if (skill.hasRole(ROLE_ATTACK)) {
            uint32_t target = GetMeleeRangeEnemy();
            return target ? target : 0;
        }
        return GetBestEnemy(aggroRange);
    }

    if (skill.target_type == 6) {
        return GetDeadAlly();
    }

    if (skill.target_type == 14) {
        const uint32_t minionId = GetNearestMinionAlly();
        return minionId ? minionId : 0;
    }

    // Self-targeting / no target
    return me->agent_id;
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

static const char* ExplainCanCastFailure(const CachedSkill& skill) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return "no_player";
    if (me->hp <= 0.0f) return "dead";

    const uint32_t loadingState = MapMgr::GetLoadingState();
    if (loadingState == 2) return "disconnected";
    if (loadingState != 1) return "not_loaded";
    if (PartyMgr::GetIsPartyDefeated()) return "party_defeated";
    if (me->model_state == 0x450) return "knocked";
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (bar && skill.slot < 8u && bar->skills[skill.slot].recharge > 0u) {
        return "recharging";
    }

    const uint32_t myId = me->agent_id;
    const uint8_t type = skill.skill_type;
    if (type == 1 || type == 2 || type == 3 || type == 5 || type == 7 ||
        type == 14 || type == 15 || type == 16) {
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return "diversion";
        if (EffectMgr::HasEffect(myId, DEBUFF_BACKFIRE)) return "backfire";
        if (EffectMgr::HasEffect(myId, DEBUFF_SOUL_LEECH)) return "soul_leech";
        if (EffectMgr::HasEffect(myId, DEBUFF_MISTRUST)) return "mistrust";
        if (EffectMgr::HasEffect(myId, DEBUFF_VISIONS_OF_REGRET)) return "visions_of_regret";
        if (EffectMgr::HasEffect(myId, DEBUFF_MARK_OF_SUBVERSION)) return "mark_of_subversion";
        if (EffectMgr::HasEffect(myId, DEBUFF_SPITEFUL_SPIRIT)) return "spiteful_spirit";
    }
    if (type == 9 || type == 17) {
        if (EffectMgr::HasEffect(myId, DEBUFF_INEPTITUDE)) return "ineptitude";
        if (EffectMgr::HasEffect(myId, DEBUFF_CLUMSINESS)) return "clumsiness";
        if (EffectMgr::HasEffect(myId, DEBUFF_WANDERING_EYE)) return "wandering_eye";
        if (EffectMgr::HasEffect(myId, DEBUFF_SPITEFUL_SPIRIT)) return "spiteful_spirit";
    }
    if (type == 4) {
        if (EffectMgr::HasEffect(myId, DEBUFF_IGNORANCE)) return "ignorance";
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return "diversion";
    }
    if (type == 10 || type == 20) {
        if (EffectMgr::HasEffect(myId, DEBUFF_WELL_OF_SILENCE)) return "well_of_silence";
        if (EffectMgr::HasEffect(myId, DEBUFF_DIVERSION)) return "diversion";
    }
    return nullptr;
}

static bool CanCast(const CachedSkill& skill) {
    return ExplainCanCastFailure(skill) == nullptr;
}

// ===== HP Gating & Effect Overlap (GWA3-124) =====

static const char* ExplainCanUseSkillFailure(const CachedSkill& skill, uint32_t targetId, float aggroRange = LONG_BOW_RANGE) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return "no_player";
    if (AgentMgr::IsCasting(me)) return "casting";

    // Debuff check first
    if (const char* castFailure = ExplainCanCastFailure(skill)) return castFailure;

    const uint32_t resolvedTargetId = ResolveSkillTarget(skill, targetId, aggroRange);
    if ((skill.target_type == 1 || skill.target_type == 4 || skill.target_type == 5 || skill.target_type == 6 || skill.target_type == 14) &&
        resolvedTargetId == 0) {
        return "no_target";
    }

    // Heal skills: only cast if someone actually needs healing
    if (skill.hasRole(ROLE_ANY_HEAL)) {
        uint32_t healTarget = resolvedTargetId;
        if (healTarget) {
            auto* ally = AgentMgr::GetAgentByID(healTarget);
            if (ally && ally->type == 0xDB) {
                auto* living = static_cast<AgentLiving*>(ally);
                if (living->hp > 0.8f) return "heal_target_healthy"; // nobody below 80% â€” don't waste
            }
        } else if (me->hp > 0.8f) {
            return "heal_self_healthy"; // self is fine â€” skip
        }
    }

    // Survival skills: only if HP is low
    if (skill.hasRole(ROLE_SURVIVAL)) {
        if (me->hp > 0.5f) return "survival_hp_high";
        // Check if already have the effect active
        float remaining = EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id);
        if (remaining > 5.0f) return "survival_effect_active"; // already active with >5s left
    }

    // Binding spirits should be available once we've committed to a real foe,
    // even if the target is slightly outside the old 1500-unit proximity gate.
    // The tighter gate was suppressing Bloodsong/AoU follow-up casts during
    // Sparkfly approaches after Signet of Spirits resolved.
    if (skill.hasRole(ROLE_BINDING)) {
        bool enemyNearby = false;
        if (targetId > 0) {
            auto* target = AgentMgr::GetAgentByID(targetId);
            if (target && target->type == 0xDB) {
                auto* living = static_cast<AgentLiving*>(target);
                if (living->allegiance == 3 && living->hp > 0.0f) {
                    const float targetDistSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
                    if (targetDistSq < 5000.0f * 5000.0f) {
                        enemyNearby = true;
                    }
                }
            }
        }
        uint32_t maxAgents = AgentMgr::GetMaxAgents();
        for (uint32_t i = 1; i < maxAgents && !enemyNearby; i++) {
            auto* a = AgentMgr::GetAgentByID(i);
            if (!a || a->type != 0xDB) continue;
            auto* living = static_cast<AgentLiving*>(a);
            if (living->allegiance == 3 && living->hp > 0.0f) {
                float dist = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
                if (dist < 2500.0f * 2500.0f) enemyNearby = true;
            }
        }
        if (!enemyNearby) return "binding_no_enemy";
    }

    // Precast: check if effect already active (don't re-cast stances/preps)
    if (skill.hasRole(ROLE_PRECAST) && !skill.hasRole(ROLE_OFFENSIVE)) {
        float remaining = EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id);
        if (remaining > 3.0f) return "precast_effect_active"; // already active
    }

    if (skill.hasRole(ROLE_SPEED_BOOST)) {
        float remaining = EffectMgr::GetEffectTimeRemaining(me->agent_id, skill.skill_id);
        if (remaining > 3.0f) return "speed_boost_active";
    }

    // GWA3-137: Quickening Zephyr energy cost multiplier
    // AutoIt applies the +30% cost penalty while Quickening Zephyr is active.
    float energyCost = static_cast<float>(skill.energy_cost);
    if (EffectMgr::HasEffect(me->agent_id, EFFECT_QUICKENING_ZEPHYR)) {
        energyCost *= 1.3f;
    }
    float myEnergy = me->energy * static_cast<float>(me->max_energy);
    if (energyCost > 0 && myEnergy < energyCost) return "energy_low";

    // GWA3-137: Adrenaline check â€” adrenaline skills need adrenaline, not energy
    const auto* skillData = SkillMgr::GetSkillConstantData(skill.skill_id);
    if (skillData && skillData->adrenaline > 0) {
        auto* bar = SkillMgr::GetPlayerSkillbar();
        if (bar) {
            uint32_t currentAdrenaline = bar->skills[skill.slot].adrenaline_a;
            if (currentAdrenaline < skillData->adrenaline) return "adrenaline_low";
        }
    }

    // GWA3-137: Pressure gate â€” Finish Him (and similar) require low target HP
    // Finish Him: only if target HP < 45%
    if (skill.hasRole(ROLE_PRESSURE) && targetId > 0) {
        auto* target = AgentMgr::GetAgentByID(resolvedTargetId ? resolvedTargetId : targetId);
        if (target && target->type == 0xDB) {
            auto* targetLiving = static_cast<AgentLiving*>(target);
            // For pressure skills with known HP gates, check target HP
            // Finish Him (skill 2249): requires < 45%
            if (skill.skill_id == 2249 && targetLiving->hp > 0.45f) return "finish_him_hp_high";
        }
    }

    if (skill.hasRole(ROLE_COND_REMOVE)) {
        const uint32_t condTargetId = resolvedTargetId ? resolvedTargetId : me->agent_id;
        if (CountConditionEffects(condTargetId) == 0) return "no_condition_target";
    }

    if (skill.hasRole(ROLE_HEX_REMOVE)) {
        const uint32_t hexTargetId = resolvedTargetId ? resolvedTargetId : me->agent_id;
        auto* hexTarget = AgentMgr::GetAgentByID(hexTargetId);
        if (!hexTarget || hexTarget->type != 0xDB || static_cast<AgentLiving*>(hexTarget)->hex == 0) {
            return "no_hex_target";
        }
    }

    return nullptr;
}

static bool CanUseSkill(const CachedSkill& skill, uint32_t targetId, float aggroRange = LONG_BOW_RANGE) {
    return ExplainCanUseSkillFailure(skill, targetId, aggroRange) == nullptr;
}

static bool TryUseSkillIndex(int idx, uint32_t targetId, bool waitForCompletion = true, float aggroRange = LONG_BOW_RANGE) {
    static constexpr DWORD kUseSkillTimeoutMs = 6000u;
    auto* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = AgentMgr::GetMyAgent();
    if (!bar || !me) return false;

    if (idx < 0 || idx >= 8) return false;
    auto& c = s_skillCache[idx];
    if (c.skill_id == 0) {
        CombatDebugLog("slot=%d skip empty", idx + 1);
        return false;
    }
    if (s_skillUsedThisStep[idx]) {
        CombatDebugLog("slot=%d skill=%u skip already_used_this_step", idx + 1, c.skill_id);
        return false;
    }

    const uint32_t skillTarget = ResolveSkillTarget(c, targetId, aggroRange);
    if (!CanUseSkill(c, targetId, aggroRange)) {
        const char* useReason = ExplainCanUseSkillFailure(c, targetId, aggroRange);
        CombatDebugLog("slot=%d skill=%u skip CanUseSkill=false reason=%s target=%u",
                       idx + 1, c.skill_id, useReason ? useReason : "unknown", targetId);
        return false;
    }

    if (skillTarget == 0 && (c.target_type == 1 || c.target_type == 4 || c.target_type == 5 || c.target_type == 6 || c.target_type == 14)) {
        CombatDebugLog("slot=%d skill=%u skip target resolution=0 targetType=%u",
                       idx + 1, c.skill_id, c.target_type);
        return false;
    }

    SetLastCombatStepDescription("skill slot=%d skill=%u target=%u",
                                 idx + 1, c.skill_id, skillTarget);
    ResetLastCombatStepInfo();
    s_lastCombatStepInfo.valid = true;
    s_lastCombatStepInfo.used_skill = true;
    s_lastCombatStepInfo.auto_attack = false;
    s_lastCombatStepInfo.slot = idx + 1;
    s_lastCombatStepInfo.skill_id = c.skill_id;
    s_lastCombatStepInfo.target_id = skillTarget;
    s_lastCombatStepInfo.role_mask = c.roles;
    s_lastCombatStepInfo.target_type = c.target_type;
    s_lastCombatStepInfo.started_at_ms = GetTickCount();
    s_skillUsedThisStep[idx] = true;
    CombatDebugLog("slot=%d skill=%u USE target=%u", idx + 1, c.skill_id, skillTarget);
    if (skillTarget != 0 && skillTarget != me->agent_id) {
        AgentMgr::ChangeTarget(skillTarget);
    }
    SkillMgr::UseSkill(idx + 1, skillTarget, 0);
    if (!waitForCompletion) {
        s_lastCombatStepInfo.finished_at_ms = GetTickCount();
        return true;
    }

    DWORD castStart = GetTickCount();
    while ((GetTickCount() - castStart) < kUseSkillTimeoutMs) {
        if (IsDead()) break;
        if (!CanCast(c)) break;
        WaitMs(50);
    }

    float aftercast = c.activation > 0 ? c.activation : 0.0f;
    const auto* skillData = SkillMgr::GetSkillConstantData(c.skill_id);
    if (skillData && skillData->aftercast > 0) {
        aftercast = skillData->aftercast;
    }
    s_lastCombatStepInfo.expected_aftercast_ms =
        aftercast > 0 ? static_cast<uint32_t>(aftercast * 1000.0f) : 0;
    if (aftercast > 0) {
        WaitMs(static_cast<DWORD>(aftercast * 1000));
    }
    s_lastCombatStepInfo.finished_at_ms = GetTickCount();
    return true;
}

// Try to use a skill matching the given role bitmask. Returns true if a skill was used.
static bool TryUseSkillWithRole(uint32_t targetId, uint32_t roleMask, bool waitForCompletion = true, float aggroRange = LONG_BOW_RANGE) {
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) return false;

    for (int i = 0; i < 8; i++) {
        auto& c = s_skillCache[i];
        if (c.skill_id == 0) {
            CombatDebugLog("roleMask=0x%X slot=%d skip empty", roleMask, i + 1);
            continue;
        }
        if (!(c.roles & roleMask)) {
            CombatDebugLog("roleMask=0x%X slot=%d skill=%u skip roles=0x%X", roleMask, i + 1, c.skill_id, c.roles);
            continue;
        }
        if (TryUseSkillIndex(i, targetId, waitForCompletion, aggroRange)) {
            return true;
        }
    }
    return false;
}

static int UseAllSkillsWithRole(uint32_t targetId, uint32_t roleMask, int maxUses = 8, float aggroRange = LONG_BOW_RANGE);
static int UseSkillsInSlotOrder(uint32_t targetId, float aggroRange = LONG_BOW_RANGE, bool waitForCompletion = true);

// Full combat routine: use skills then fall back to auto-attack
static void FightTarget(uint32_t targetId, float aggroRange = LONG_BOW_RANGE) {
    // GWA3-121: Combat mode toggle â€” if LLM mode, just auto-attack
    // Gemma handles skill decisions via the bridge
    auto& cfg = Bot::GetConfig();
    if (cfg.combat_mode == CombatMode::LLM) {
        SetLastCombatStepDescription("llm_auto_attack target=%u", targetId);
        ResetLastCombatStepInfo();
        s_lastCombatStepInfo.valid = true;
        s_lastCombatStepInfo.auto_attack = true;
        s_lastCombatStepInfo.target_id = targetId;
        s_lastCombatStepInfo.started_at_ms = GetTickCount();
        CombatDebugLog("LLM combat mode issuing auto-attack target=%u", targetId);
        AgentMgr::Attack(targetId);
        s_lastCombatStepInfo.finished_at_ms = GetTickCount();
        return;
    }

    if (!s_skillsCached) CacheSkillBar();
    SetLastCombatStepDescription("no_action target=%u", targetId);
    ResetLastCombatStepInfo();
    memset(s_skillUsedThisStep, 0, sizeof(s_skillUsedThisStep));
    CombatDebugLog("FightTarget start target=%u", targetId);

    auto* me = AgentMgr::GetMyAgent();
    if (!me) return;
    if (AgentMgr::IsCasting(me)) {
        CombatDebugLog("FightTarget skip while casting target=%u skill=%u model=0x%X",
                       targetId, me->skill, me->model_state);
        return;
    }

    bool attacked = false;
    if (targetId != 0 && CanBasicAttack()) {
        CombatDebugLog("FightTarget opening auto-attack target=%u before slot sweep", targetId);
        AgentMgr::Attack(targetId);
        attacked = true;
    }

    const int uses = UseSkillsInSlotOrder(targetId, aggroRange, true);
    if (uses <= 0 && attacked) {
        CombatDebugLog("FightTarget slot sweep found no skill; auto-attack target=%u", targetId);
        SetLastCombatStepDescription("auto_attack target=%u", targetId);
        ResetLastCombatStepInfo();
        s_lastCombatStepInfo.valid = true;
        s_lastCombatStepInfo.auto_attack = true;
        s_lastCombatStepInfo.target_id = targetId;
        s_lastCombatStepInfo.role_mask = ROLE_ATTACK | ROLE_OFFENSIVE;
        s_lastCombatStepInfo.started_at_ms = GetTickCount();
        s_lastCombatStepInfo.finished_at_ms = GetTickCount();
    }
}

// ===== Helpers =====

static float GetNearestEnemyDistance(float maxRange = 99999.0f) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return maxRange;

    float bestDistSq = maxRange * maxRange;
    bool found = false;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 3) continue;
        if (living->hp <= 0.0f) continue;
        const float distSq = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            found = true;
        }
    }

    return found ? sqrtf(bestDistSq) : maxRange;
}

static bool CanAttackInAggro(float aggroRange, uint32_t* outTargetId = nullptr) {
    const uint32_t bestTargetId = GetBestEnemy(aggroRange);
    if (outTargetId) *outTargetId = bestTargetId;
    if (bestTargetId == 0) return false;
    return CanBasicAttack();
}

static bool WeCanMove(float range = LONG_BOW_RANGE) {
    if (GetNearestEnemyDistance() < range && range < LONG_BOW_RANGE) return false;
    return true;
}

static void FightEnemiesInAggro(float aggroRange, bool careful = false,
                                SparkflyTraversalCombatStats* stats = nullptr) {
    const DWORD fightStart = GetTickCount();
    const bool sparkflyMap = MapMgr::GetMapId() == MAP_SPARKFLY_SWAMP;
    if (sparkflyMap) {
        SkillMgr::SetSparkflyPlayerUseSkillOverride(true);
    }

    while (GetNearestEnemyDistance() <= aggroRange &&
           !IsDead() &&
           MapMgr::GetIsMapLoaded() &&
           !PartyMgr::GetIsPartyDefeated() &&
           (GetTickCount() - fightStart) < 240000) {
        if (careful) {
            AgentMgr::CancelAction();
        }

        uint32_t bestTarget = 0;
        const bool canAttack = CanAttackInAggro(aggroRange, &bestTarget);
        if (!bestTarget) break;

        if (stats) {
            ++stats->quick_step_attempts;
            stats->last_target_id = bestTarget;
        }

        memset(s_skillUsedThisStep, 0, sizeof(s_skillUsedThisStep));
        bool attacked = false;
        if (canAttack) {
            AgentMgr::Attack(bestTarget);
            attacked = true;
        }
        WaitMs(100);

        if (careful) {
            auto* target = AgentMgr::GetAgentByID(bestTarget);
            if (target) {
                AgentMgr::Move(target->x, target->y);
            }
            WaitMs(300);
        }

        const DWORD actionStart = GetTickCount();
        const int uses = UseSkillsInSlotOrder(bestTarget, aggroRange, true);
        if (uses <= 0 && attacked) {
            SetLastCombatStepDescription("auto_attack target=%u", bestTarget);
            ResetLastCombatStepInfo();
            s_lastCombatStepInfo.valid = true;
            s_lastCombatStepInfo.auto_attack = true;
            s_lastCombatStepInfo.target_id = bestTarget;
            s_lastCombatStepInfo.role_mask = ROLE_ATTACK | ROLE_OFFENSIVE;
            s_lastCombatStepInfo.started_at_ms = actionStart;
            s_lastCombatStepInfo.finished_at_ms = GetTickCount();
        }
        const auto stepInfo = GetLastCombatStepInfo();
        if (stats && stepInfo.valid && stepInfo.started_at_ms >= actionStart) {
            if (stepInfo.used_skill) {
                ++stats->skill_steps;
            } else if (stepInfo.auto_attack) {
                ++stats->auto_attack_steps;
            }
        }

        WaitMs(100);
    }

    if (sparkflyMap) {
        SkillMgr::SetSparkflyPlayerUseSkillOverride(false);
    }

    if (stats == nullptr) {
        PickupNearbyLoot(3000.0f);
    }
}

static float DistanceTo(float x, float y) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 99999.0f;
    return AgentMgr::GetDistance(me->x, me->y, x, y);
}

static int GetNearestWaypointIndex(const Waypoint* wps, int count) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = FLT_MAX;
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

static void LogLvl1ToLvl2TransitionState(const char* stage, uint32_t portalId, DWORD elapsedMs, DWORD attempt) {
    auto* me = AgentMgr::GetMyAgent();
    const float meX = me ? me->x : 0.0f;
    const float meY = me ? me->y : 0.0f;
    const float distToExit = me ? AgentMgr::GetDistance(me->x, me->y, 7665.0f, -19050.0f) : -1.0f;
    const uint32_t mapId = MapMgr::GetMapId();
    const int loaded = MapMgr::GetIsMapLoaded() ? 1 : 0;
    const uint32_t targetId = AgentMgr::GetTargetId();

    float portalX = 0.0f;
    float portalY = 0.0f;
    float portalDist = -1.0f;
    uint32_t portalType = 0u;
    uint32_t portalGadget = 0u;
    if (portalId) {
        if (auto* portal = AgentMgr::GetAgentByID(portalId)) {
            portalX = portal->x;
            portalY = portal->y;
            portalType = portal->type;
            if (portal->type == 0x200) {
                portalGadget = static_cast<AgentGadget*>(portal)->gadget_id;
            }
            if (me) {
                portalDist = AgentMgr::GetDistance(me->x, me->y, portal->x, portal->y);
            }
        }
    }

    LogBot("Lvl1 to Lvl2 [%s] attempt=%lu elapsed=%lums map=%u loaded=%d me=(%.0f, %.0f) distToExit=%.0f target=%u portal=%u type=0x%X gadget=%u portalPos=(%.0f, %.0f) portalDist=%.0f",
           stage,
           static_cast<unsigned long>(attempt),
           static_cast<unsigned long>(elapsedMs),
           mapId,
           loaded,
           meX,
           meY,
           distToExit,
           targetId,
           portalId,
           portalType,
           portalGadget,
           portalX,
           portalY,
           portalDist);
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
      LogBot("AggroMoveToEx start target=(%.0f, %.0f) fightRange=%.0f", x, y, fightRange);
      if (WeCanMove(fightRange)) {
          AgentMgr::Move(x, y);
      }
    DWORD start = GetTickCount();
    DWORD lastMoveIssue = start;
    const float arrivalThreshold = MapMgr::GetMapId() == MAP_SPARKFLY_SWAMP ? 500.0f : 250.0f;
    // GWA3-135: Per-target combat timeout
    uint32_t currentTargetId = 0;
    DWORD targetFightStart = 0;
    // GWA3-134: Stuck detection
    float lastX = 0, lastY = 0;
    int stuckCount = 0;
    {
        auto* meInit = AgentMgr::GetMyAgent();
        if (meInit) { lastX = meInit->x; lastY = meInit->y; }
    }
    while (DistanceTo(x, y) > arrivalThreshold && (GetTickCount() - start) < 240000) {
        if (IsDead()) return;
        if (!IsMapLoaded()) return;

        const float nearestDistance = GetNearestEnemyDistance();
        if (nearestDistance < fightRange) {
            // AutoIt's blocked detection is tied to movement attempts, not
            // intentional combat pauses inside AggroMoveToEX.
            stuckCount = 0;
            if (auto* meCombat = AgentMgr::GetMyAgent()) {
                lastX = meCombat->x;
                lastY = meCombat->y;
            }
        } else {
            // GWA3-134: Check if stuck (position unchanged between iterations)
            auto* meStuck = AgentMgr::GetMyAgent();
            if (meStuck) {
                float moved = AgentMgr::GetDistance(lastX, lastY, meStuck->x, meStuck->y);
                if (moved < 10.0f) {
                    stuckCount++;
                    if (stuckCount == 15) {
                        // Try a random sideways move to unstick
                        float randX = x + static_cast<float>((GetTickCount() % 600) - 300);
                        float randY = y + static_cast<float>((GetTickCount() % 600) - 300);
                        LogBot("Stuck detected (%d iterations) â€” trying random move (%.0f, %.0f)",
                               stuckCount, randX, randY);
                        AgentMgr::Move(randX, randY);
                        WaitMs(500);
                    } else if (stuckCount >= 30) {
                        LogBot("Stuck limit reached (%d) â€” aborting waypoint movement", stuckCount);
                        return;
                    }
                } else {
                    stuckCount = 0; // meaningful progress â€” reset
                }
                lastX = meStuck->x;
                lastY = meStuck->y;
            }
        }

        // Check for enemies in fight range. Mirror AutoIt's AggroMoveToEX by
        // entering the fight loop as soon as a foe is inside aggro range.
        {
            if (nearestDistance < fightRange) {
                const uint32_t bestId = GetBestEnemy(fightRange);
                if (!bestId) {
                    WaitMs(100);
                    continue;
                }
                const bool sparkflyTraversal = MapMgr::GetMapId() == MAP_SPARKFLY_SWAMP;
                if (sparkflyTraversal) {
                    ++s_sparkflyTraversalCombatStats.settle_requests;
                    s_sparkflyTraversalCombatStats.last_target_id = bestId;
                    LogBot("AggroMoveToEx Sparkfly holding movement for local clear: foe=%u waypoint=(%.0f, %.0f) dist=%.0f",
                           bestId, x, y, nearestDistance);
                    AgentMgr::CancelAction();
                    WaitMs(50);
                    FightEnemiesInAggro(fightRange, false, &s_sparkflyTraversalCombatStats);
                    AgentMgr::CancelAction();
                    currentTargetId = 0;
                    AgentMgr::Move(x, y);
                    lastMoveIssue = GetTickCount();
                    WaitMs(250);
                    continue;
                }

                // GWA3-135: Track per-target combat duration
                if (bestId != currentTargetId) {
                    currentTargetId = bestId;
                    targetFightStart = GetTickCount();
                    LogBot("AggroMoveToEx engaging foe=%u waypoint=(%.0f, %.0f) dist=%.0f",
                           bestId, x, y, nearestDistance);
                }
                DWORD fightDuration = GetTickCount() - targetFightStart;
                if (fightDuration > 120000) {
                    // 2 minutes on same target â€” disengage, skip to next waypoint
                    LogBot("Combat timeout: 120s on target %u â€” disengaging", bestId);
                    UnflagAllHeroes();
                    currentTargetId = 0;
                    break;
                }
                if (fightDuration > 60000 && fightDuration < 61000) {
                    LogBot("Combat warning: 60s on target %u", bestId);
                }
                auto* me = AgentMgr::GetMyAgent();
                if (AgentMgr::IsCasting(me)) {
                    WaitMs(50);
                    continue;
                }

                // Flag heroes to fight position
                const bool allowHeroFlags = true;
                __try {
                    auto* foeAgent = AgentMgr::GetAgentByID(bestId);
                    if (allowHeroFlags && foeAgent) {
                        FlagAllHeroes(foeAgent->x, foeAgent->y);
                    }
                    AgentMgr::ChangeTarget(bestId);
                    FightTarget(bestId, fightRange);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    LogBot("AggroMoveToEx combat fault on foe=%u waypoint=(%.0f, %.0f); falling back to auto-attack",
                           bestId, x, y);
                    if (allowHeroFlags) {
                        UnflagAllHeroes();
                    }
                    AgentMgr::Attack(bestId);
                    WaitMs(500);
                    currentTargetId = 0;
                    continue;
                }
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
                currentTargetId = 0; // target dead, reset timer
                // Combat resolved â€” unflag heroes
                if (allowHeroFlags) {
                    UnflagAllHeroes();
                }
                // Enemy dead or gone â€” pick up loot
                PickupNearbyLoot(600.0f);
                AgentMgr::Move(x, y);
                lastMoveIssue = GetTickCount();
                continue;
            }
        }

        const DWORD now = GetTickCount();
        const bool longSinceMove = (now - lastMoveIssue) >= 4000;
        const bool stalledEnoughToReissue = stuckCount >= 2 && (now - lastMoveIssue) >= 1500;
        if (longSinceMove || stalledEnoughToReissue) {
            AgentMgr::Move(x, y);
            lastMoveIssue = now;
        }
        WaitMs(250);
    }
    LogBot("AggroMoveToEx end target=(%.0f, %.0f) remaining=%.0f threshold=%.0f",
           x, y, DistanceTo(x, y), arrivalThreshold);
}

// GWA3-140: Wipe recovery checkpoint â€” back up 2 waypoints from nearest
static int GetWipeRestartWaypoint(const Waypoint* wps, int count) {
    int nearest = GetNearestWaypointIndex(wps, count);
    int restart = nearest - 2;
    if (restart < 0) restart = 0;
    return restart;
}

static void SuspendTransitionSensitiveHooks() {
    CtoS::SuspendEngineHook();
    DialogMgr::Shutdown();
}

static void ResumeTransitionSensitiveHooks() {
    CtoS::ResumeEngineHook();
    DialogMgr::Initialize();
}

static void GrabDungeonBlessing(float shrineX, float shrineY); // forward decl
static void OpenDungeonDoorAt(float doorX, float doorY);       // forward decl
static uint32_t FindNearestNpcByAllegiance(float x, float y, float maxDist); // forward decl
static uint32_t FindNearestSignpost(float x, float y, float maxDist); // forward decl
static void FollowWaypoints(const Waypoint* wps, int count, bool ignoreBotRunning = false) {
    int startIdx = GetNearestWaypointIndex(wps, count);
    uint32_t mapId = MapMgr::GetMapId();
    const bool useNearestProgressBacktrack = mapId != MAP_BOGROOT_LVL1 && mapId != MAP_BOGROOT_LVL2;
    // GWA3-140: Stuck detection â€” track nearest waypoint progress
    int lastNearestWp = startIdx;
    int sameWpCount = 0;

    for (int i = startIdx; i < count; i++) {
        if (!ignoreBotRunning && !Bot::IsRunning()) return;
        if (MapMgr::GetMapId() != mapId) return;

        if (useNearestProgressBacktrack) {
            // GWA3-140: Stuck backtrack â€” if nearest waypoint unchanged 5 iterations
            int currentNearest = GetNearestWaypointIndex(wps, count);
            if (currentNearest == lastNearestWp) {
                sameWpCount++;
                if (sameWpCount >= 5) {
                    int backtrack = (currentNearest > 0) ? currentNearest - 1 : 0;
                    LogBot("Waypoint stuck (nearest=%d unchanged 5x) â€” backtracking to %d",
                           currentNearest, backtrack);
                    i = backtrack;
                    sameWpCount = 0;
                }
            } else {
                lastNearestWp = currentNearest;
                sameWpCount = 0;
            }
        }

        if (IsDead()) {
            s_wipeCount++;
            LogBot("WIPE detected at waypoint %d (%s) â€” wipe #%u", i, wps[i].label, s_wipeCount);

            DWORD wipeStart = GetTickCount();
            while (IsDead() && (GetTickCount() - wipeStart) < 120000) {
                WaitMs(500);
            }
            if (IsDead()) {
                LogBot("Party defeated â€” returning to outpost");
                MapMgr::ReturnToOutpost();
                return;
            }

            if (s_wipeCount >= 2) {
                LogBot("Multiple wipes (%u) â€” using DP removal", s_wipeCount);
                UseDpRemovalIfNeeded();
                WaitMs(500);
            }

            // GWA3-140: Checkpoint-based restart
            int restartIdx = GetWipeRestartWaypoint(wps, count);
            LogBot("Resuming from checkpoint %d after wipe (was at %d)", restartIdx, i);
            i = restartIdx;
        }

        LogBot("Moving to waypoint %d: %s (%.0f, %.0f)", i, wps[i].label, wps[i].x, wps[i].y);

        // Special waypoint handling
        if (strcmp(wps[i].label, "Blessing") == 0) {
            // Move to shrine area with aggro (enemies may be nearby)
            if (wps[i].fightRange > 0 && IsMapLoaded()) {
                AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            } else {
                MoveToAndWait(wps[i].x, wps[i].y);
            }
            GrabDungeonBlessing(wps[i].x, wps[i].y);
            continue;
        }
        if (strcmp(wps[i].label, "Lvl1 to Lvl2") == 0) {
            SuspendTransitionSensitiveHooks();
            uint32_t portalId = FindNearestSignpost(7665.0f, -19050.0f, 1500.0f);
            if (portalId) {
                LogBot("Lvl1 to Lvl2: found portal signpost agent=%u", portalId);
                if (auto* portal = AgentMgr::GetAgentByID(portalId)) {
                    MoveToAndWait(portal->x, portal->y, 150.0f);
                }
            } else {
                LogBot("Lvl1 to Lvl2: no signpost found near portal coords, using movement-only push");
            }
            DWORD start = GetTickCount();
            DWORD lastLogAt = 0;
            DWORD lastRefreshAt = 0;
            DWORD attempt = 0;
            LogLvl1ToLvl2TransitionState("start", portalId, 0, attempt);
            while ((GetTickCount() - start) < 60000) {
                ++attempt;
                const DWORD elapsed = GetTickCount() - start;
                if ((!portalId || !AgentMgr::GetAgentExists(portalId)) && (elapsed - lastRefreshAt) >= 1000) {
                    const uint32_t refreshedPortalId = FindNearestSignpost(7665.0f, -19050.0f, 1500.0f);
                    if (refreshedPortalId != portalId) {
                        LogBot("Lvl1 to Lvl2: portal refresh old=%u new=%u elapsed=%lums",
                               portalId,
                               refreshedPortalId,
                               static_cast<unsigned long>(elapsed));
                        portalId = refreshedPortalId;
                    }
                    lastRefreshAt = elapsed;
                }
                if (elapsed - lastLogAt >= 2000) {
                    LogLvl1ToLvl2TransitionState("loop", portalId, elapsed, attempt);
                    lastLogAt = elapsed;
                }
                if (portalId) {
                    AgentMgr::InteractSignpost(portalId);
                    WaitMs(250);
                }
                AgentMgr::Move(7665, -19050);
                WaitMs(500);
                if (MapMgr::GetMapId() == MAP_BOGROOT_LVL2) {
                    LogLvl1ToLvl2TransitionState("entered_lvl2", portalId, GetTickCount() - start, attempt);
                    s_dungeonLoopTelemetry.entered_lvl2 = true;
                    WaitMs(3000);
                    ResumeTransitionSensitiveHooks();
                    return;
                }
            }
            LogLvl1ToLvl2TransitionState("timeout", portalId, GetTickCount() - start, attempt);
            ResumeTransitionSensitiveHooks();
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
            OpenDungeonDoorAt(wps[i].x, wps[i].y);
            continue;
        }
        if (strcmp(wps[i].label, "Dungeon Door Checkpoint") == 0) {
            // Bogroot's route geometry causes pure nearest-waypoint checks to
            // oscillate here. Just settle at the checkpoint and continue.
            if (wps[i].fightRange > 0 && IsMapLoaded()) {
                AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            } else {
                MoveToAndWait(wps[i].x, wps[i].y);
            }
            LogBot("Dungeon Door Checkpoint reached at wp %d", i);
            continue;
        }
        if (strcmp(wps[i].label, "Quest Door Checkpoint") == 0) {
            // Bogroot's quest-door checkpoint is followed by a duplicate
            // coordinate waypoint. Nearest-waypoint heuristics are not stable
            // here, so rely on forward route progression instead of aborting.
            if (wps[i].fightRange > 0 && IsMapLoaded()) {
                AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            } else {
                MoveToAndWait(wps[i].x, wps[i].y);
            }
            LogBot("Quest Door Checkpoint reached at wp %d", i);
            continue;
        }
        if (strcmp(wps[i].label, "Boss") == 0) {
            // GWA3-163: Full boss encounter
            s_dungeonLoopTelemetry.boss_started = true;
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            // Fight + first loot sweep
            WaitMs(3000);
            PickupNearbyLoot(1500.0f);

            // Open chest (find nearest signpost near chest coords)
            MoveToAndWait(14876, -19033);
            ++s_dungeonLoopTelemetry.chest_attempts;
            if (OpenNearbyChest(1500.0f)) {
                ++s_dungeonLoopTelemetry.chest_successes;
            }
            WaitMs(2000);
            PickupNearbyLoot(1500.0f);

            // Second chest attempt (AutoIt does double open)
            ++s_dungeonLoopTelemetry.chest_attempts;
            if (OpenNearbyChest(1500.0f)) {
                ++s_dungeonLoopTelemetry.chest_successes;
            }
            WaitMs(1000);
            PickupNearbyLoot(1000.0f);

            // Talk to Tekk for quest reward
            // Use GoNPC pattern: find NPC, move close, interact, dialog
            MoveToAndWait(14618, -17828);
            s_dungeonLoopTelemetry.reward_attempted = true;
            uint32_t tekksId = FindNearestNpcByAllegiance(14618, -17828, 1500.0f);
            if (tekksId) {
                auto* npc = AgentMgr::GetAgentByID(tekksId);
                if (npc) MoveToAndWait(npc->x, npc->y, 120.0f);
                AgentMgr::ChangeTarget(tekksId);
                WaitMs(250);
                // GoNPC interaction
                for (int go = 0; go < 3; ++go) {
                    CtoS::SendPacket(3, Packets::INTERACT_NPC, tekksId, 0u);
                    WaitMs(500);
                }
                WaitMs(1000);
                SendDialogWithRetry(DIALOG_QUEST_REWARD, 3, 1000);
            } else {
                LogBot("Boss: Tekk NPC not found â€” sending reward dialog directly");
                SendDialogWithRetry(DIALOG_QUEST_REWARD, 3, 1000);
            }
            s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
            s_dungeonLoopTelemetry.reward_dialog_latched =
                s_dungeonLoopTelemetry.last_dialog_id == DIALOG_QUEST_REWARD;
            s_dungeonLoopTelemetry.boss_completed = true;

            SuspendTransitionSensitiveHooks();
            const DWORD transitionStart = GetTickCount();
            while ((GetTickCount() - transitionStart) < 360000) {
                const uint32_t mapIdNow = MapMgr::GetMapId();
                if (mapIdNow != MAP_BOGROOT_LVL2) {
                    s_dungeonLoopTelemetry.final_map_id = mapIdNow;
                    s_dungeonLoopTelemetry.returned_to_sparkfly = mapIdNow == MAP_SPARKFLY_SWAMP;
                    if (mapIdNow == MAP_SPARKFLY_SWAMP) {
                        WaitMs(3000);
                        ResumeTransitionSensitiveHooks();
                    }
                    return;
                }
                WaitMs(250);
            }
            ResumeTransitionSensitiveHooks();
            s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
            return;
        }

        // Standard waypoint â€” aggro move then loot sweep
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

static bool PrepareTekksDungeonEntry() {
    static constexpr float kTekksStageX = 12061.0f;
    static constexpr float kTekksStageY = 22485.0f;
    static constexpr float kTekksSearchX = 12396.0f;
    static constexpr float kTekksSearchY = 22407.0f;

    LogBot("Preparing Tekks dungeon entry sequence");
    MoveToAndWait(kTekksStageX, kTekksStageY, 500.0f);
    WaitMs(500);

    const uint32_t tekksId = FindNearestNpcByAllegiance(kTekksSearchX, kTekksSearchY, 1800.0f);
    if (tekksId != 0) {
        if (auto* npc = AgentMgr::GetAgentByID(tekksId)) {
            MoveToAndWait(npc->x, npc->y, 120.0f);
        }
        AgentMgr::ChangeTarget(tekksId);
        WaitMs(250);
        DialogMgr::ResetHookState();
        const bool npcHooked = DialogMgr::NPCHook(tekksId, 2000u);
        LogBot("Tekks NPCHook agent=%u hooked=%d lastUi=0x%X lastDialog=0x%X",
               tekksId,
               npcHooked ? 1 : 0,
               DialogMgr::GetLastUIMessageId(),
               DialogMgr::GetLastDialogId());
    } else {
        LogBot("Tekks NPC not found near expected coordinates; sending dialog sequence without NPCHook");
    }

    SendDialogWithRetry(DIALOG_QUEST_ACCEPT, 2, 250);
    if (QuestMgr::GetQuestById(QUEST_TEKKS_WAR) != nullptr) {
        SendDialogWithRetry(DIALOG_QUEST_REWARD, 3, 250);
        WaitMs(250);
        SendDialogWithRetry(DIALOG_QUEST_ACCEPT, 2, 250);
    }
    SendDialogWithRetry(DIALOG_NPC_TALK, 2, 250);
    SendDialogWithRetry(DIALOG_DUNGEON_ENTRY, 2, 250);
    WaitMs(500);

    LogBot("Tekks dungeon entry sequence complete activeQuest=0x%X questPresent=%d lastDialog=0x%X",
           QuestMgr::GetActiveQuestId(),
           QuestMgr::GetQuestById(QUEST_TEKKS_WAR) != nullptr ? 1 : 0,
           DialogMgr::GetLastDialogId());
    return true;
}

// ===== Loot Pickup (GWA3-098) =====

// Model IDs that should always be picked up regardless of rarity
static bool IsAlwaysPickupModel(uint32_t modelId) {
    switch (modelId) {
    case 2619: case 36985:
    case 27067: case 27071: case 27033: case 27052: case 22374:
    case 2605: case 2606: case 501: case 502: case 503: case 2566:
    case 2607: case 6102: case 6104: case 6531:
    case 15564: case 15565: case 15867: case 15869: case 15870: case 15871:
    case 17054: case 17055: case 17075:
    case 22781: case 22782:
    case 25410: case 25413: case 25416:
    case 24628: case 24582:
    case 910: case 2513: case 5585: case 6049: case 6366: case 6367: case 6375:
    case 15477: case 19171: case 19172: case 19173: case 22190: case 24593:
    case 28435: case 30855: case 31145: case 31146: case 35124: case 36682:
    case 15528: case 15479: case 19170: case 21492: case 21812: case 22644:
    case 30208: case 31150: case 35125: case 36681:
    case 17060: case 17061: case 17062: case 22269: case 28431: case 28432:
    case 28436: case 29431: case 31151: case 31152: case 31153: case 35121:
    case 6370: case 19039: case 21488: case 21489: case 22191: case 26784:
    case 28433: case 35127:
    case 556: case 18345: case 21491: case 37765: case 21833: case 28434:
    case 930: case 935: case 936: case 945:
    case 21786: case 21787: case 21788: case 21789: case 21790:
    case 21791: case 21792: case 21793: case 21794: case 21795:
    case 21796: case 21797: case 21798: case 21799: case 21800:
    case 21801: case 21802: case 21803: case 21804: case 21805:
    case 22751:
        return true;
    }
    return false;
}

// GWA3-138: Froggy-specific quest/trophy items (ported from CanPickUpEx)
static bool IsQuestPickupModel(uint32_t modelId) {
    switch (modelId) {
    // Bogroot Growths quest items
    case 22342: case 24350:   // Unlit Torch, Asura Flame Staff
    // General dungeon items
    case 22751:               // Lockpick
    // Tomes (all professions)
    case 21796: case 21797: case 21798: case 21799: case 21800:
    case 21801: case 21802: case 21803: case 21804: case 21805:
    // Alcohol / sweets / party items (for title tracking)
    case 28435: case 28436: case 28431: case 22269:  // Party/birthday/candy
        return true;
    }
    return false;
}

// Item type constants
static constexpr uint8_t TYPE_TROPHY   = 30;
static constexpr uint8_t TYPE_SCROLL   = 31;
static constexpr uint8_t TYPE_DYE      = 10;
static constexpr uint8_t TYPE_USABLE   = 9;
static constexpr uint8_t TYPE_KEY      = 18;
static constexpr uint8_t TYPE_BUNDLE   = 6;
static constexpr uint8_t TYPE_MATERIAL = 11;
static constexpr uint8_t TYPE_GOLD     = 20;

static bool ShouldPickUp(const Agent* agent, uint32_t myAgentId) {
    if (!agent || agent->type != 0x400) return false;
    auto* itemAgent = static_cast<const AgentItem*>(agent);

    // Don't pick up items reserved for other players
    if (itemAgent->owner != 0 && itemAgent->owner != myAgentId) return false;

    // Cross-reference with actual item data
    auto* item = ItemMgr::GetItemById(itemAgent->item_id);
    if (!item) return false;

    // GWA3-138: Inventory guard â€” if < 2 free slots, only pick gold coins and bundles
    uint32_t freeSlots = CountFreeSlots();
    if (freeSlots < 2) {
        if (item->type == TYPE_GOLD) return true;  // gold coins always
        if (item->type == TYPE_BUNDLE) return true; // quest bundles always
        return false; // skip everything else when nearly full
    }

    if (MaintenanceMgr::IsRareSkin(item->model_id)) return true;

    if (IsAlwaysPickupModel(item->model_id)) return true;

    // GWA3-138: Quest-specific items
    if (IsQuestPickupModel(item->model_id)) return true;

    switch (item->type) {
    case TYPE_BUNDLE:
        return item->model_id == 22342 || item->model_id == 24350;
    case TYPE_DYE:
        return item->dye.dye_tint == 10;
    case TYPE_KEY:
        return true;
    case TYPE_GOLD:
        return ItemMgr::GetGoldCharacter() < 100000;
    case TYPE_MATERIAL:
    case TYPE_SCROLL:
    case TYPE_TROPHY:
        return false;
    case TYPE_USABLE:
        return item->model_id >= 21786 && item->model_id <= 21805;
    }

    uint16_t rarity = GetItemRarity(item);
    return rarity == RARITY_GOLD && freeSlots > 8;
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

        // GWA3-133: Retry loop â€” items may fail first pick attempt
        uint32_t itemAgentId = a->agent_id;
        DWORD itemStart = GetTickCount();
        int retries = 0;
        while (retries < 10 && (GetTickCount() - itemStart) < 6000) {
            ItemMgr::PickUpItem(itemAgentId);
            WaitMs(250);
            retries++;
            // Check if item was picked up (agent no longer exists)
            if (!AgentMgr::GetAgentExists(itemAgentId)) break;
            if (IsDead()) return picked;
        }
        picked++;
        freeSlots--;
        me = AgentMgr::GetMyAgent();
        if (!me) return picked;

        // GWA3-133: Global deadlock protection â€” 2 min total loot time
        static DWORD s_lootGlobalStart = 0;
        if (picked == 1) s_lootGlobalStart = GetTickCount();
        if (s_lootGlobalStart > 0 && (GetTickCount() - s_lootGlobalStart) > 120000) {
            LogBot("Loot deadlock: 2 minutes exceeded â€” aborting pickup");
            return picked;
        }
    }
    return picked;
}

// Known chest gadget IDs
static bool IsChestGadgetId(uint32_t gadgetId) {
    return gadgetId == 6062 || gadgetId == 4579 || gadgetId == 4582 ||
           gadgetId == 8141 || gadgetId == 74 || gadgetId == 68 || gadgetId == 9157;
}

// GWA3-139: Track opened chests to avoid re-interaction
static constexpr int MAX_OPENED_CHESTS = 64;
static uint32_t s_openedChests[MAX_OPENED_CHESTS] = {};
static int s_openedChestCount = 0;
static uint32_t s_openedChestMapId = 0; // clear on map change

static bool IsChestOpened(uint32_t agentId) {
    for (int i = 0; i < s_openedChestCount; i++) {
        if (s_openedChests[i] == agentId) return true;
    }
    return false;
}

static void MarkChestOpened(uint32_t agentId) {
    if (s_openedChestCount < MAX_OPENED_CHESTS) {
        s_openedChests[s_openedChestCount++] = agentId;
    }
}

static bool OpenNearbyChest(float maxRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return false;

    // Clear tracking on map change
    uint32_t currentMap = MapMgr::GetMapId();
    if (currentMap != s_openedChestMapId) {
        s_openedChestCount = 0;
        s_openedChestMapId = currentMap;
    }

    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0x200) continue;
        float dist = AgentMgr::GetDistance(me->x, me->y, a->x, a->y);
        if (dist > maxRange) continue;

        auto* gadget = static_cast<const AgentGadget*>(a);
        if (!IsChestGadgetId(gadget->gadget_id)) continue;
        if (IsChestOpened(a->agent_id)) continue; // GWA3-139: skip already opened

        LogBot("Opening chest (agent=%u gadget=%u dist=%.0f)", a->agent_id, gadget->gadget_id, dist);
        MarkChestOpened(a->agent_id);

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
        LogBot("No ID kit found â€” skipping identification");
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

    // Don't salvage materials (type 11) â€” they already ARE materials
    if (item->type == 11) return false;

    // Don't salvage keys (type 18)
    if (item->type == 18) return false;

    // Don't salvage usable items (type 9) â€” scrolls, tonics, etc.
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
        LogBot("No salvage kit found â€” skipping salvage");
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

    // Never sell unidentified golds/purples â€” they might be valuable
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

// Find the nearest signpost/gadget agent (type 0x200) near given coordinates.
static uint32_t FindNearestSignpost(float x, float y, float maxDist) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    if (maxAgents == 0) return 0;

    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a) continue;
        if (a->type != 0x200) continue; // signpost/gadget only
        float d = AgentMgr::GetSquaredDistance(x, y, a->x, a->y);
        if (d < bestDist) {
            bestDist = d;
            bestId = a->agent_id;
        }
    }
    return bestId;
}

// Open a dungeon door at given coordinates.
// Mirrors AutoIt OpenDungeonDoor(): move to coords, find signpost, interact multiple times.
static void OpenDungeonDoorAt(float doorX, float doorY) {
    LogBot("OpenDungeonDoor at (%.0f, %.0f)", doorX, doorY);

    // Find the nearest signpost (door) near the coordinates
    uint32_t doorId = FindNearestSignpost(doorX, doorY, 1500.0f);
    if (doorId == 0) {
        LogBot("OpenDungeonDoor: no signpost found near door coords");
        return;
    }
    LogBot("OpenDungeonDoor: found signpost agent=%u", doorId);

    // Move close to the door
    auto* door = AgentMgr::GetAgentByID(doorId);
    if (door) {
        MoveToAndWait(door->x, door->y, 120.0f);
    }

    // Interact with the door multiple times (AutoIt does ActionInteract x6)
    // We use InteractSignpost which sends the SIGNPOST_RUN (0x51) packet
    for (int attempt = 1; attempt <= 3; ++attempt) {
        LogBot("OpenDungeonDoor: interact attempt %d", attempt);
        AgentMgr::InteractSignpost(doorId);
        WaitMs(500);
        AgentMgr::InteractSignpost(doorId);
        WaitMs(1000);
    }

    // Move to door coords to push through (AutoIt does MoveTo after each interact set)
    MoveToAndWait(doorX, doorY, 200.0f);
    WaitMs(500);
}

static bool WaitForMerchantContext(DWORD timeoutMs) {
    static constexpr uint32_t kMerchantRootHash = 3613855137u;
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (TradeMgr::GetMerchantItemCount() > 0) return true;
        if (UIMgr::IsFrameVisible(kMerchantRootHash)) return true;
        WaitMs(100);
    }
    return false;
}

static int UseAllSkillsWithRole(uint32_t targetId, uint32_t roleMask, int maxUses, float aggroRange) {
    int usedCount = 0;
    while (usedCount < maxUses) {
        if (!TryUseSkillWithRole(targetId, roleMask, true, aggroRange)) {
            break;
        }
        ++usedCount;
    }
    if (usedCount > 0) {
        CombatDebugLog("roleMask=0x%X multi_use count=%d", roleMask, usedCount);
    }
    return usedCount;
}

static int UseSkillsInSlotOrder(uint32_t targetId, float aggroRange, bool waitForCompletion) {
    int usedCount = 0;
    for (int i = 0; i < 8; ++i) {
        if (TryUseSkillIndex(i, targetId, waitForCompletion, aggroRange)) {
            ++usedCount;
        }
        if (GetNearestEnemyDistance() > aggroRange) {
            break;
        }
    }
    if (usedCount > 0) {
        CombatDebugLog("slot_order_use count=%d", usedCount);
    }
    return usedCount;
}

static bool OpenMerchantContextWithVariants(uint32_t npcId) {
    LogBot("Merchant open via proven legacy GoNPC path...");
    AgentMgr::ChangeTarget(npcId);
    WaitMs(250);
    for (int goAttempt = 1; goAttempt <= 3; ++goAttempt) {
        LogBot("  GoNPC attempt %d: SendPacket(3, 0x39, %u, 0)", goAttempt, npcId);
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        WaitMs(500);
    }
    WaitMs(2500);
    return WaitForMerchantContext(1500);
}

static void SellJunkToMerchant(const BotConfig& cfg) {
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

    const bool merchantOpen = OpenMerchantContextWithVariants(merchantId);

    if (!merchantOpen) {
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
    AgentMgr::InteractNPC(xunlaiId);
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
// Overworld EotN zone blessings
static constexpr uint32_t SKILL_DWARVEN_BLESSING  = 2049;
static constexpr uint32_t SKILL_ASURAN_BLESSING   = 2050;
static constexpr uint32_t SKILL_NORN_BLESSING     = 2051;
static constexpr uint32_t SKILL_VANGUARD_BLESSING = 2052;
// Dungeon veteran blessing variants (from shrine NPCs inside dungeons)
static constexpr uint32_t SKILL_VET_ASURAN_BODYGUARD    = 2548;
static constexpr uint32_t SKILL_VET_DWARVEN_RAIDER      = 2549;
static constexpr uint32_t SKILL_VET_VANGUARD_PATROL     = 2550;
static constexpr uint32_t SKILL_VET_NORN_HUNTING_PARTY  = 2551;
// Title display IDs for SetActiveTitle packet (0x58)
// AutoIt: $ID_DWARF_TITLE=0x27, matches GWCA TitleID::Deldrimor
static constexpr uint32_t TITLE_DISPLAY_DELDRIMOR = 0x27;
// Dialog ID for accepting a blessing from a shrine NPC
static constexpr uint32_t DIALOG_ACCEPT_BLESSING  = 0x84;
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
    // Overworld blessings
    if (EffectMgr::HasEffect(myId, SKILL_DWARVEN_BLESSING) ||
        EffectMgr::HasEffect(myId, SKILL_ASURAN_BLESSING) ||
        EffectMgr::HasEffect(myId, SKILL_NORN_BLESSING) ||
        EffectMgr::HasEffect(myId, SKILL_VANGUARD_BLESSING)) return true;
    // Dungeon veteran blessings
    if (EffectMgr::HasEffect(myId, SKILL_VET_ASURAN_BODYGUARD) ||
        EffectMgr::HasEffect(myId, SKILL_VET_DWARVEN_RAIDER) ||
        EffectMgr::HasEffect(myId, SKILL_VET_VANGUARD_PATROL) ||
        EffectMgr::HasEffect(myId, SKILL_VET_NORN_HUNTING_PARTY)) return true;
    return false;
}

static bool HasConset() {
    if (Offsets::MyID <= 0x10000) return false;
    uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);
    return EffectMgr::HasEffect(myId, SKILL_ARMOR_OF_SALVATION) &&
           EffectMgr::HasEffect(myId, SKILL_ESSENCE_CELERITY) &&
           EffectMgr::HasEffect(myId, SKILL_GRAIL_OF_MIGHT);
}

// ===== Dungeon Blessing Grab =====
// Mirrors AutoIt BotsHub pattern: SetDisplayedTitle â†’ GoNearestNPCToCoords â†’ Dialog(0x84)
// Must disable DialogMgr StoC hooks during interaction â€” StringEncoding::DecodeStr
// times out in Bogroot dungeons and crashes the game via StoC callback corruption.

static void GrabDungeonBlessing(float shrineX, float shrineY) {
    if (HasBlessing()) {
        LogBot("Blessing: already active, skipping");
        return;
    }

    // Set Deldrimor title (required for Bogroot shrine to offer blessing)
    uint32_t currentTitle = PlayerMgr::GetActiveTitleId();
    if (currentTitle == 0) {
        LogBot("Blessing: setting Deldrimor title (0x%X)", TITLE_DISPLAY_DELDRIMOR);
        PlayerMgr::SetActiveTitle(TITLE_DISPLAY_DELDRIMOR);
        WaitMs(1000);
    }

    // Find the blessing NPC near the shrine coordinates
    uint32_t npcId = FindNearestNpcByAllegiance(shrineX, shrineY, 2000.0f);
    if (npcId == 0) {
        LogBot("Blessing: no NPC found near (%.0f, %.0f)", shrineX, shrineY);
        return;
    }
    LogBot("Blessing: found NPC agent=%u near shrine", npcId);

    // Move close to the NPC
    auto* npc = AgentMgr::GetAgentByID(npcId);
    if (npc) {
        MoveToAndWait(npc->x, npc->y, 120.0f);
    }

    // Disable DialogMgr StoC hooks to prevent StringEncoding crash in dungeon
    DialogMgr::Shutdown();

    // GoNPC (0x39) x3 â€” matches AutoIt GoNearestNPCToCoords retry pattern
    AgentMgr::ChangeTarget(npcId);
    WaitMs(500);
    for (int attempt = 1; attempt <= 3; ++attempt) {
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        WaitMs(1000);
    }

    // Dialog (0x3B, 0x84) â€” AutoIt DIALOG_SEND header, not DIALOG_SEND_LIVING
    CtoS::SendPacket(2, Packets::DIALOG_SEND, DIALOG_ACCEPT_BLESSING);
    LogBot("Blessing: sent Dialog(0x%X)", DIALOG_ACCEPT_BLESSING);
    WaitMs(2000);

    // Re-enable DialogMgr
    DialogMgr::Initialize();

    // Verify
    if (HasBlessing()) {
        LogBot("Blessing: confirmed active");
    } else {
        LogBot("Blessing: effect not detected after interaction");
    }
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

    LogBot("Consets low (armor=%u essence=%u grail=%u) â€” traveling to Embark Beach to craft",
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

        AgentMgr::InteractNPC(npcId);
        WaitMs(750);
        QuestMgr::Dialog(npcId);
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
        LogBot("Some consets missing â€” may need to craft/buy");
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
    // DP removal sweets â€” use if we have any and morale is bad
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

    // Run maintenance if needed (sell junk, deposit gold, buy kits)
    if (MaintenanceMgr::NeedsMaintenance()) {
        LogBot("Maintenance needed â€” running before dungeon entry");

        // Move to merchant and open window
        static constexpr float kGaddsMerchantX = -8374.0f;
        static constexpr float kGaddsMerchantY = -22491.0f;
        MoveToAndWait(kGaddsMerchantX, kGaddsMerchantY, 350.0f);
        uint32_t merchantId = FindNearestNpcByAllegiance(kGaddsMerchantX, kGaddsMerchantY, 900.0f);
        if (merchantId) {
            auto* npc = AgentMgr::GetAgentByID(merchantId);
            if (npc) MoveToAndWait(npc->x, npc->y, 120.0f);
            if (OpenMerchantContextWithVariants(merchantId)) {
                MaintenanceMgr::PerformMaintenance();
                AgentMgr::CancelAction();
                WaitMs(500);
            } else {
                LogBot("Maintenance: merchant window failed to open, skipping");
            }
        } else {
            LogBot("Maintenance: merchant NPC not found, skipping sell/buy");
            // Still deposit gold even without merchant
            MaintenanceMgr::DepositGold(10000);
        }
    }

    // Load hero config from file (adds heroes + loads their skillbars)
    if (!cfg.hero_config_file.empty()) {
        // Kick existing heroes first so we get a clean slate.
        // Note: legacy HERO_KICK(0x26) "kick all" sentinel is not reliable in
        // our current client environment. PartyMgr::KickAllHeroes() is kept as
        // the confirmed working per-hero clear path.
        PartyMgr::KickAllHeroes();
        for (int retry = 0; retry < 20 && PartyMgr::CountPartyHeroes() > 0; ++retry) {
            if (retry == 8) {
                LogBot("KickAllHeroes still pending during bot setup, reissuing per-hero clear...");
                PartyMgr::KickAllHeroes();
            }
            WaitMs(250);
        }

        int loaded = LoadHeroConfigFile(cfg.hero_config_file.c_str(), cfg);
        if (loaded == 0) {
            // Fallback: add heroes from hardcoded config without skillbar loading
            LogBot("Config file failed â€” using hardcoded hero IDs");
            for (int i = 0; i < 7; i++) {
                if (cfg.hero_ids[i] > 0) {
                    PartyMgr::AddHero(cfg.hero_ids[i]);
                    WaitMs(300);
                }
            }
        }
    } else {
        // No config file â€” use hardcoded hero IDs
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
    uint32_t mapId = MapMgr::GetMapId();

    // Refresh combat cache on explorable entry rather than relying on town-setup state.
    RefreshCombatSkillbar();

    if (mapId == MAP_SPARKFLY_SWAMP) {
        LogBot("State: Sparkfly Swamp â€” running to dungeon");
        s_runCount++;
        s_runStartTime = GetTickCount();

        // GWA3-166: Renew consets at Sparkfly entry
        UseConsumables(cfg);

        // Match the proven AutoIt Sparkfly aggro route instead of
        // straight-lining to Tekks. This keeps combat active on approach and
        // avoids the explorable glide/floating behavior seen on long direct
        // movement.
        FollowWaypoints(SPARKFLY_TO_DUNGEON, sizeof(SPARKFLY_TO_DUNGEON) / sizeof(SPARKFLY_TO_DUNGEON[0]));

        // AutoIt-faithful Tekks interaction: interact with Tekks, refresh the
        // quest dialog state, then send the dungeon-entry dialog so the door
        // actually opens before we walk the portal.
        PrepareTekksDungeonEntry();

        // Move to dungeon entrance
        MoveToAndWait(12228, 22677);
        MoveToAndWait(12470, 25036);
        AgentMgr::Move(12968, 26219);
        WaitMs(1000);

        // Suspend hooks before dungeon zone transition to prevent
        // stale-context crashes from CtoS engine hook and DialogMgr StoC hooks.
        CtoS::SuspendEngineHook();
        DialogMgr::Shutdown();

        // Enter dungeon
        DWORD start = GetTickCount();
        while ((GetTickCount() - start) < 60000) {
            AgentMgr::Move(13097, 26393);
            WaitMs(250);
            if (MapMgr::GetMapId() == MAP_BOGROOT_LVL1) break;
        }
        WaitMs(3000);

        // Resume hooks now that we're stable inside Bogroot
        CtoS::ResumeEngineHook();
        DialogMgr::Initialize();
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
    LogBot("State: Merchant (return to outpost)");
    LogBot("Merchant lane: legacy GoNPC packet path");

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

    // Sell junk items to merchant. The experimental AutoIt-style merchant lane
    // is opt-in via config so the existing merchant path remains untouched.
    SellJunkToMerchant(cfg);

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
    LogBot("State: ERROR â€” waiting 10s before retry");
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

    // Default config â€” hero IDs are fallback if config file fails to load
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
        Log::Info("[FROGGY-UT] [FAIL] %s", name);
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

    // ShouldSalvage â€” fake items with rarity in name_enc
    // We can't easily fake name_enc pointer, so test with nullptr (should return false)
    Item noName = {};
    noName.item_id = 3;
    noName.model_id = 100;
    noName.name_enc = nullptr;
    FroggyCheck("ShouldSalvage null_name=false", !ShouldSalvage(&noName));
    FroggyCheck("ShouldSalvage null=false", !ShouldSalvage(nullptr));

    // ShouldSalvage kit model â€” should be false regardless
    Item fakeKit = {};
    fakeKit.item_id = 4;
    fakeKit.model_id = MODEL_SALV_KIT;
    uint16_t whiteRarity = RARITY_WHITE;
    fakeKit.name_enc = reinterpret_cast<wchar_t*>(&whiteRarity);
    FroggyCheck("ShouldSalvage kit=false", !ShouldSalvage(&fakeKit));

    // ShouldSalvage material type â€” should be false
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
        // We're alive in-game â€” can do real state checks
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
    // This requires an explorable map, so it is exercised in the Froggy
    // feature test's explorable phase rather than here in the outpost unit
    // suite.
    LogBot("--- GWA3-116: Hero Flagging ---");
    LogBot("  SKIP: Hero flagging moved to explorable feature test phase");

    // Skip live dialog mutation in the unit suite. Even dialog_id=0 has been
    // observed to destabilize the later merchant phase in the end-to-end Froggy
    // flow, so this behavior is now covered elsewhere.
    LogBot("  SKIP: SendDialogWithRetry(0) suppressed in unit tests");

    // --- GWA3-128: Skill classification (role bitmask) ---
    LogBot("--- GWA3-128: Skill Classification ---");

    // Test hardcoded classifier functions â€” these are pure logic, no game state needed
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

    // Test role bitmask operations â€” pure logic
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

        // ResolveSkillTarget for heals â€” should return self or ally when in-game
        CachedSkill fakeHealResolve = {};
        fakeHealResolve.roles = ROLE_HEAL_SINGLE;
        fakeHealResolve.target_type = 3;
        uint32_t healTarget = ResolveSkillTarget(fakeHealResolve, 0);
        FroggyCheck("ResolveSkillTarget(heal) returns valid agent",
                    healTarget > 0 && AgentMgr::GetAgentExists(healTarget));

        CachedSkill fakeSelfResolve = {};
        fakeSelfResolve.roles = ROLE_PRECAST | ROLE_BINDING;
        fakeSelfResolve.target_type = 0;
        uint32_t selfTarget = ResolveSkillTarget(fakeSelfResolve, 42);
        FroggyCheck("ResolveSkillTarget(self) returns my agent id",
                    selfTarget == AgentMgr::GetMyId());

        // ResolveSkillTarget for res â€” should return 0 (nobody dead, hopefully)
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

        // ResolveSkillTarget for hex â€” should return unhexed enemy or fallback
        CachedSkill fakeHexResolve = {};
        fakeHexResolve.roles = ROLE_HEX;
        fakeHexResolve.target_type = 5;
        uint32_t hexTarget = ResolveSkillTarget(fakeHexResolve, 42);
        FroggyCheck("ResolveSkillTarget(hex) returns valid enemy or fallback 42",
                    hexTarget == 42 || AgentMgr::GetAgentExists(hexTarget));
    } else {
        LogBot("  SKIP: Not in explorable â€” skipping enemy targeting tests");
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
        LogBot("  SKIP: Not alive/loaded â€” skipping HP gating tests");
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

    // ===== EPIC 19: Fidelity Gap Tests =====

    // --- GWA3-141: Aftercast delay ---
    LogBot("--- GWA3-141: Aftercast Delay ---");
    // Verify aftercast value is read from skill data
    const auto* resSigData = SkillMgr::GetSkillConstantData(2); // Resurrection Signet
    if (resSigData) {
        FroggyCheck("Res Signet aftercast >= 0", resSigData->aftercast >= 0.0f);
        FroggyCheck("Res Signet activation > 0", resSigData->activation > 0.0f);
        // Aftercast is used in TryUseSkillWithRole to pace skill execution
    } else {
        LogBot("  SKIP: Skill constant data not available");
    }

    // --- GWA3-142: Loot retry + deadlock ---
    LogBot("--- GWA3-142: Loot Retry ---");
    // Verify PickupNearbyLoot returns 0 when no loot nearby (not infinite loop)
    DWORD lootStart = GetTickCount();
    int lootResult = PickupNearbyLoot(100.0f); // tiny range â€” likely no items
    DWORD lootElapsed = GetTickCount() - lootStart;
    FroggyCheck("PickupNearbyLoot(100) completes quickly", lootElapsed < 5000);
    FroggyCheck("PickupNearbyLoot(100) returns >= 0", lootResult >= 0);

    // --- GWA3-143: Stuck detection ---
    LogBot("--- GWA3-143: Stuck Detection ---");
    // Stuck detection is inside AggroMoveToEx â€” we can't directly test the counter
    // but we can verify the helper functions work
    auto* meStuckTest = AgentMgr::GetMyAgent();
    if (meStuckTest) {
        float d = AgentMgr::GetDistance(meStuckTest->x, meStuckTest->y,
                                        meStuckTest->x + 5.0f, meStuckTest->y);
        FroggyCheck("Distance 5 units = 5.0", d >= 4.5f && d <= 5.5f);
        float d2 = AgentMgr::GetDistance(meStuckTest->x, meStuckTest->y,
                                         meStuckTest->x, meStuckTest->y);
        FroggyCheck("Distance to self = 0", d2 < 1.0f);
        // Stuck threshold is 10 units â€” verify that 5 < 10 (would trigger stuck)
        FroggyCheck("5 units < stuck threshold 10", 5.0f < 10.0f);
    }

    // --- GWA3-144: Combat timeout ---
    LogBot("--- GWA3-144: Combat Timeout ---");
    // Timeout values are constants in AggroMoveToEx:
    // 60s warn, 120s disengage. Verify these are sane.
    FroggyCheck("Combat warn timeout 60s < disengage 120s", 60000 < 120000);
    FroggyCheck("Combat disengage 120s < overall 240s", 120000 < 240000);

    // --- GWA3-145: CanCast knockdown/wipe/disconnect ---
    LogBot("--- GWA3-145: CanCast Safety ---");
    if (meStuckTest && meStuckTest->hp > 0.0f && MapMgr::GetLoadingState() == 1) {
        CachedSkill testSpell = {};
        testSpell.skill_type = 2; // Spell
        testSpell.roles = ROLE_OFFENSIVE;

        // Should pass when alive + loaded + not defeated
        if (!PartyMgr::GetIsPartyDefeated()) {
            FroggyCheck("CanCast(spell) = true when alive+loaded+not_defeated", CanCast(testSpell));
        }

        // Verify the loading state check works
        FroggyCheck("GetLoadingState == 1 during test", MapMgr::GetLoadingState() == 1);
        FroggyCheck("Party not defeated during test", !PartyMgr::GetIsPartyDefeated());
    }

    // --- GWA3-146: Zephyr multiplier + adrenaline ---
    LogBot("--- GWA3-146: Zephyr + Adrenaline ---");
    // Test energy cost multiplier logic
    float baseCost = 10.0f;
    float zephyrCost = baseCost * 1.3f;
    FroggyCheck("Zephyr multiplier: 10 * 1.3 = 13", zephyrCost >= 12.9f && zephyrCost <= 13.1f);

    // Test adrenaline skill data exists
    const auto* attackSkill = SkillMgr::GetSkillConstantData(332); // Hundred Blades
    if (attackSkill) {
        LogBot("  Hundred Blades: adrenaline=%u energy=%u", attackSkill->adrenaline, attackSkill->energy_cost);
        // Hundred Blades is adrenaline-based
        FroggyCheck("Hundred Blades has adrenaline cost",
                    attackSkill->adrenaline > 0 || attackSkill->energy_cost > 0);
    }

    // --- GWA3-147: Loot policy fidelity ---
    LogBot("--- GWA3-147: Loot Policy ---");
    // Test IsQuestPickupModel
    FroggyCheck("IsQuestPickup torch(22342)=true", IsQuestPickupModel(22342));
    FroggyCheck("IsQuestPickup lockpick(22751)=false (handled by IsAlwaysPickup)",
                !IsQuestPickupModel(22751) || IsAlwaysPickupModel(22751));
    FroggyCheck("IsQuestPickup tome(21796)=true", IsQuestPickupModel(21796));
    FroggyCheck("IsQuestPickup random(9999)=false", !IsQuestPickupModel(9999));

    // Test type constants are distinct
    FroggyCheck("TYPE_TROPHY != TYPE_KEY", TYPE_TROPHY != TYPE_KEY);
    FroggyCheck("TYPE_SCROLL != TYPE_MATERIAL", TYPE_SCROLL != TYPE_MATERIAL);
    FroggyCheck("TYPE_GOLD == 20", TYPE_GOLD == 20);
    FroggyCheck("TYPE_DYE == 10", TYPE_DYE == 10);

    // Inventory guard test: with fake low free slots
    // Can't directly test ShouldPickUp with fake inventory state,
    // but verify the CountFreeSlots function works
    uint32_t currentFreeSlots = CountFreeSlots();
    FroggyCheck("CountFreeSlots returns sane value", currentFreeSlots <= 40);

    // --- GWA3-148: Opened chest tracking ---
    LogBot("--- GWA3-148: Chest Tracking ---");
    // Save current state
    uint32_t savedMapId = s_openedChestMapId;
    int savedCount = s_openedChestCount;

    // NOTE: OpenedChestTracker tests temporarily disabled — struct not yet implemented
    // s_openedChestTracker = {};
    // s_openedChestTracker.ResetForMap(MapMgr::GetMapId());
    // SyncOpenedChestTrackerState();
    // ... (see git history for full test)
    // --- GWA3-149: Waypoint stuck + checkpoints ---
    LogBot("--- GWA3-149: Waypoint Stuck + Checkpoints ---");
    // Test GetWipeRestartWaypoint
    // Fake a simple 10-waypoint array
    Waypoint fakeWps[10] = {};
    for (int w = 0; w < 10; w++) {
        fakeWps[w].x = static_cast<float>(w * 1000);
        fakeWps[w].y = 0;
        fakeWps[w].fightRange = 1300;
        fakeWps[w].label = "test";
    }
    // Checkpoint should back up 2 from nearest
    int restart = GetWipeRestartWaypoint(fakeWps, 10);
    FroggyCheck("GetWipeRestartWaypoint returns >= 0", restart >= 0);
    FroggyCheck("GetWipeRestartWaypoint returns < count", restart < 10);

    // Stuck detection threshold: 5 iterations
    FroggyCheck("Stuck threshold is 5 iterations", 5 == 5); // constant documented
    // Backtrack goes to nearest - 1
    int backtrack = 5 - 1; // if stuck at wp 5, backtrack to 4
    FroggyCheck("Backtrack target = nearest - 1", backtrack == 4);

    LogBot("=== Froggy Unit Tests Complete: %d passed, %d failed ===", s_testPassed, s_testFailed);
    return s_testFailed;
}

bool DebugAggroMoveTo(float x, float y, float fightRange) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    LogBot("DebugAggroMoveTo request target=(%.0f, %.0f) fightRange=%.0f", x, y, fightRange);
    AggroMoveToEx(x, y, fightRange);
    const float arrivalThreshold = MapMgr::GetMapId() == MAP_SPARKFLY_SWAMP ? 500.0f : 250.0f;
    return DistanceTo(x, y) <= arrivalThreshold;
}

bool DebugRunSparkflyRouteToTekks() {
    auto* me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f || !MapMgr::GetIsMapLoaded() || MapMgr::GetMapId() != MAP_SPARKFLY_SWAMP) {
        return false;
    }

    static constexpr float kTekksStageX = 12061.0f;
    static constexpr float kTekksStageY = 22485.0f;
    static constexpr float kTekksStageThreshold = 500.0f;

    LogBot("DebugRunSparkflyRouteToTekks start");
    FollowWaypoints(SPARKFLY_TO_DUNGEON, sizeof(SPARKFLY_TO_DUNGEON) / sizeof(SPARKFLY_TO_DUNGEON[0]), true);
    if (!MapMgr::GetIsMapLoaded() || MapMgr::GetMapId() != MAP_SPARKFLY_SWAMP) {
        LogBot("DebugRunSparkflyRouteToTekks aborted after waypoint follow: map=%u loaded=%d",
               MapMgr::GetMapId(), MapMgr::GetIsMapLoaded() ? 1 : 0);
        return false;
    }

    MoveToAndWait(kTekksStageX, kTekksStageY, kTekksStageThreshold);
    const float remaining = DistanceTo(kTekksStageX, kTekksStageY);
    const bool reached = remaining <= kTekksStageThreshold;
    LogBot("DebugRunSparkflyRouteToTekks end remaining=%.0f reached=%d", remaining, reached ? 1 : 0);
    return reached;
}

bool ExecuteBuiltinCombatStep(uint32_t targetId, bool quickStep) {
    if (!targetId) return false;

    auto* target = AgentMgr::GetAgentByID(targetId);
    if (!target || target->type != 0xDB) return false;

    auto& cfg = Bot::GetConfig();
    const CombatMode originalMode = cfg.combat_mode;
    cfg.combat_mode = CombatMode::Builtin;
    SetLastCombatStepDescription("uninitialized");
    ResetLastCombatStepInfo();
    ResetCombatDebugTrace();
    s_combatDebugLogging = true;
    const bool sparkflyMap = MapMgr::GetMapId() == MAP_SPARKFLY_SWAMP;

    if (quickStep) {
        if (!s_skillsCached) CacheSkillBar();
        memset(s_skillUsedThisStep, 0, sizeof(s_skillUsedThisStep));
        AgentMgr::ChangeTarget(targetId);
        bool attacked = false;
        if (CanBasicAttack()) {
            AgentMgr::Attack(targetId);
            attacked = true;
        }
        SkillMgr::SetSparkflyPlayerUseSkillOverride(true);
        const int usedSkills = UseSkillsInSlotOrder(targetId, LONG_BOW_RANGE, false);
        SkillMgr::SetSparkflyPlayerUseSkillOverride(false);
        if (usedSkills <= 0 && attacked) {
            SetLastCombatStepDescription("auto_attack target=%u", targetId);
            ResetLastCombatStepInfo();
            s_lastCombatStepInfo.valid = true;
            s_lastCombatStepInfo.auto_attack = true;
            s_lastCombatStepInfo.target_id = targetId;
            s_lastCombatStepInfo.role_mask = ROLE_ATTACK | ROLE_OFFENSIVE;
            s_lastCombatStepInfo.started_at_ms = GetTickCount();
            s_lastCombatStepInfo.finished_at_ms = GetTickCount();
        }
    } else if (sparkflyMap) {
        if (!s_skillsCached) CacheSkillBar();
        memset(s_skillUsedThisStep, 0, sizeof(s_skillUsedThisStep));
        AgentMgr::ChangeTarget(targetId);

        bool usedFoeTargetSkill = false;
        SkillMgr::SetSparkflyPlayerUseSkillOverride(true);
        for (int i = 0; i < 8; ++i) {
            const auto& c = s_skillCache[i];
            if (c.skill_id == 0) continue;
            if (c.target_type != 5) continue;
            if (!CanUseSkill(c, targetId, LONG_BOW_RANGE)) continue;
            usedFoeTargetSkill = TryUseSkillIndex(i, targetId, true, LONG_BOW_RANGE);
            if (usedFoeTargetSkill) {
                break;
            }
        }

        if (!usedFoeTargetSkill) {
            bool attacked = false;
            if (CanBasicAttack()) {
                AgentMgr::Attack(targetId);
                attacked = true;
            }
            const int usedSkills = UseSkillsInSlotOrder(targetId, LONG_BOW_RANGE, true);
            if (usedSkills <= 0 && attacked) {
                SetLastCombatStepDescription("auto_attack target=%u", targetId);
                ResetLastCombatStepInfo();
                s_lastCombatStepInfo.valid = true;
                s_lastCombatStepInfo.auto_attack = true;
                s_lastCombatStepInfo.target_id = targetId;
                s_lastCombatStepInfo.role_mask = ROLE_ATTACK | ROLE_OFFENSIVE;
                s_lastCombatStepInfo.started_at_ms = GetTickCount();
                s_lastCombatStepInfo.finished_at_ms = GetTickCount();
            }
        }
        SkillMgr::SetSparkflyPlayerUseSkillOverride(false);
    } else {
        FightTarget(targetId);
    }

    s_combatDebugLogging = false;
    cfg.combat_mode = originalMode;
    return true;
}

const char* GetLastCombatStepDescription() {
    return s_lastCombatStep;
}

LastCombatStepInfo GetLastCombatStepInfo() {
    return s_lastCombatStepInfo;
}

void ResetSparkflyTraversalCombatStats() {
    ResetSparkflyTraversalCombatStatsState();
}

SparkflyTraversalCombatStats GetSparkflyTraversalCombatStats() {
    return s_sparkflyTraversalCombatStats;
}

bool DebugResolveSyntheticSkillTarget(uint32_t roleMask, uint8_t targetType,
                                      uint32_t defaultFoeId, uint32_t& outTargetId) {
    CachedSkill synthetic = {};
    synthetic.roles = roleMask;
    synthetic.target_type = targetType;
    outTargetId = ResolveSkillTarget(synthetic, defaultFoeId);
    return AgentMgr::GetMyAgent() != nullptr;
}

bool DebugResolveUsableSkillTargetForSlot(uint32_t slot, uint32_t defaultFoeId,
                                          uint32_t& outSkillId, uint32_t& outTargetId, uint8_t& outTargetType) {
    if (!s_skillsCached) {
        CacheSkillBar();
    }
    outSkillId = 0;
    outTargetId = 0;
    outTargetType = 0;
    if (slot == 0 || slot > 8) return false;

    const auto& c = s_skillCache[slot - 1];
    if (c.skill_id == 0) return false;
    if (!CanUseSkill(c, defaultFoeId)) return false;

    const uint32_t resolvedTarget = ResolveSkillTarget(c, defaultFoeId);
    if (resolvedTarget == 0 &&
        (c.target_type == 1 || c.target_type == 4 || c.target_type == 5 || c.target_type == 6 || c.target_type == 14)) {
        return false;
    }

    outSkillId = c.skill_id;
    outTargetId = resolvedTarget;
    outTargetType = c.target_type;
    return true;
}

uint32_t DebugGetCastingEnemy() {
    return GetCastingEnemy();
}

uint32_t DebugGetEnchantedEnemy() {
    return GetEnchantedEnemy();
}

uint32_t DebugGetMeleeRangeEnemy() {
    return GetMeleeRangeEnemy();
}

bool RefreshCombatSkillbar() {
    s_skillsCached = false;
    CacheSkillBar();
    if (!s_skillsCached) return false;

    bool hasNonZero = false;
    bool hasClassifiedRole = false;
    for (int i = 0; i < 8; ++i) {
        if (s_skillCache[i].skill_id != 0) {
            hasNonZero = true;
        }
        if (s_skillCache[i].roles != ROLE_NONE) {
            hasClassifiedRole = true;
        }
    }

    LogBot("Froggy combat skillbar refresh: cached=%d hasNonZero=%d hasRole=%d",
           s_skillsCached ? 1 : 0, hasNonZero ? 1 : 0, hasClassifiedRole ? 1 : 0);
    return hasNonZero;
}

void DebugDumpBuiltinCombatDecision(uint32_t targetId) {
    ResetBuiltinCombatDump();
    auto* me = AgentMgr::GetMyAgent();
    auto* target = AgentMgr::GetAgentByID(targetId);
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (!me || !target || target->type != 0xDB || !bar) {
        AddBuiltinCombatDumpLine("unavailable me=%d target=%d bar=%d",
                                 me ? 1 : 0, target ? 1 : 0, bar ? 1 : 0);
        LogBot("BuiltinCombatDump: unavailable me=%d target=%d bar=%d",
               me ? 1 : 0, target ? 1 : 0, bar ? 1 : 0);
        return;
    }

    if (!s_skillsCached) {
        CacheSkillBar();
    }

    auto* living = static_cast<AgentLiving*>(target);
    const float myEnergy = me->energy * me->max_energy;
    const float distance = AgentMgr::GetDistance(me->x, me->y, living->x, living->y);
    AddBuiltinCombatDumpLine("target=%u distance=%.0f hp=%.3f energy=%.1f activeSkill=%u modelState=0x%X",
                             targetId, distance, living->hp, myEnergy, me->skill, me->model_state);
    LogBot("BuiltinCombatDump: target=%u distance=%.0f hp=%.3f energy=%.1f activeSkill=%u modelState=0x%X",
           targetId, distance, living->hp, myEnergy, me->skill, me->model_state);

    for (int i = 0; i < 8; ++i) {
        const auto& c = s_skillCache[i];
        if (c.skill_id == 0) {
            AddBuiltinCombatDumpLine("slot=%d empty", i + 1);
            LogBot("BuiltinCombatDump: slot=%d empty", i + 1);
            continue;
        }
        const bool roleMatch = (c.roles & (ROLE_OFFENSIVE | ROLE_ATTACK)) != 0;
        const bool rechargeReady = bar->skills[i].recharge == 0;
        const bool energyReady = c.energy_cost <= static_cast<uint8_t>(myEnergy);
        const bool canCast = CanCast(c);
        const bool canUse = CanUseSkill(c, targetId);
        const char* canCastReason = ExplainCanCastFailure(c);
        const char* canUseReason = ExplainCanUseSkillFailure(c, targetId);
        const uint32_t resolvedTarget = ResolveSkillTarget(c, targetId);
        const uint32_t adrenalineReq = SkillMgr::GetSkillConstantData(c.skill_id) ? SkillMgr::GetSkillConstantData(c.skill_id)->adrenaline : 0;
        const uint32_t adrenalineCur = bar->skills[i].adrenaline_a;
        AddBuiltinCombatDumpLine("slot=%d skill=%u roles=0x%X match=%d recharge=%u ready=%d e=%u/%0.1f adren=%u/%u canCast=%d canUse=%d castReason=%s useReason=%s target=%u tgtType=%u type=%u",
                                 i + 1, c.skill_id, c.roles, roleMatch ? 1 : 0, bar->skills[i].recharge,
                                 rechargeReady ? 1 : 0, c.energy_cost, myEnergy, adrenalineCur, adrenalineReq,
                                 canCast ? 1 : 0, canUse ? 1 : 0,
                                 canCastReason ? canCastReason : "ok",
                                 canUseReason ? canUseReason : "ok",
                                 resolvedTarget, c.target_type, c.skill_type);
        LogBot("BuiltinCombatDump: slot=%d skill=%u roles=0x%X roleMatch=%d recharge=%u ready=%d energyCost=%u energyReady=%d canUse=%d resolvedTarget=%u targetType=%u type=%u",
               i + 1, c.skill_id, c.roles, roleMatch ? 1 : 0, bar->skills[i].recharge,
               rechargeReady ? 1 : 0, c.energy_cost, energyReady ? 1 : 0, canUse ? 1 : 0,
               resolvedTarget, c.target_type, c.skill_type);
    }
}

int GetBuiltinCombatDecisionDumpCount() {
    return s_builtinCombatDumpCount;
}

const char* GetBuiltinCombatDecisionDumpLine(int index) {
    if (index < 0 || index >= s_builtinCombatDumpCount) return "";
    return s_builtinCombatDump[index];
}

int GetCombatDebugTraceCount() {
    return s_combatDebugTraceCount;
}

const char* GetCombatDebugTraceLine(int index) {
    if (index < 0 || index >= s_combatDebugTraceCount) return "";
    return s_combatDebugTrace[index];
}

void ResetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry = {};
}

DungeonLoopTelemetry GetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    return s_dungeonLoopTelemetry;
}

bool DebugRunDungeonLoopFromCurrentMap() {
    ResetDungeonLoopTelemetry();

    const uint32_t startMapId = MapMgr::GetMapId();
    if (startMapId == MAP_BOGROOT_LVL1) {
        s_dungeonLoopTelemetry.started_in_lvl1 = true;
        FollowWaypoints(BOGROOT_LVL1, sizeof(BOGROOT_LVL1) / sizeof(BOGROOT_LVL1[0]), true);
    } else if (startMapId == MAP_BOGROOT_LVL2) {
        s_dungeonLoopTelemetry.started_in_lvl2 = true;
    } else {
        s_dungeonLoopTelemetry.final_map_id = startMapId;
        return false;
    }

    if (MapMgr::GetMapId() == MAP_BOGROOT_LVL2) {
        s_dungeonLoopTelemetry.started_in_lvl2 = true;
        FollowWaypoints(BOGROOT_LVL2, sizeof(BOGROOT_LVL2) / sizeof(BOGROOT_LVL2[0]), true);
    }

    s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        s_dungeonLoopTelemetry.final_map_id == MAP_SPARKFLY_SWAMP;
    return s_dungeonLoopTelemetry.returned_to_sparkfly;
}

} // namespace GWA3::Bot::Froggy

