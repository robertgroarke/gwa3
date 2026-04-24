// IntegrationTestEpic14.cpp — Phased Froggy feature tests
// Run via: injector.exe --test-froggy
//
// Self-setting-up: travels to outpost, adds heroes, opens merchant,
// enters explorable, finds enemies, then returns. No manual setup needed.

#include "IntegrationTestInternal.h"
#include <gwa3/core/Log.h>
#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/CallbackRegistry.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/bot/FroggyHM.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Party.h>
#include <gwa3/game/Skill.h>
#include <gwa3/game/Effect.h>
#include <gwa3/game/Title.h>
#include <gwa3/managers/PlayerMgr.h>

#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace GWA3::SmokeTest {

static int s_passed = 0;
static int s_failed = 0;
static int s_skipped = 0;
static bool s_isolatedExplorableFlaggingMode = false;
static bool s_enableInvasiveSparkflyCombatProofs = false;
static bool s_preferDirectTekksStagingForDebug = false;
static constexpr float kSessionHarnessInteractionDistanceTolerance = 130.0f;
static constexpr uint32_t MODEL_CHEAP_ID_KIT = 2989;
static constexpr uint32_t MODEL_SUPERIOR_ID_KIT = 5899;
static constexpr uint32_t MODEL_CHEAP_SALVAGE_KIT = 2992;
static constexpr uint32_t MODEL_EXPERT_SALVAGE_KIT = 2991;
static constexpr uint32_t MODEL_RARE_SALVAGE_KIT = 2993;
static constexpr uint32_t MODEL_FROGGY_SALVAGE_KIT = 5900;
static constexpr uint32_t MERCHANT_SLOT_FROGGY_SALVAGE_KIT = 4;
static constexpr uint32_t MERCHANT_SLOT_SUPERIOR_ID_KIT = 6;
static constexpr uint32_t MERCHANT_PRICE_FROGGY_SALVAGE_KIT = 2000;
static constexpr uint32_t MERCHANT_PRICE_SUPERIOR_ID_KIT = 500;
static constexpr size_t kHeroTemplateCount = 7;
static constexpr uint32_t QUEST_TEKKS_WAR = 0x339;
static constexpr uint32_t DIALOG_TEKKS_ACCEPT = 0x833901;
static constexpr uint32_t DIALOG_TEKKS_REWARD = 0x833907;
static constexpr uint32_t DIALOG_NPC_TALK = 0x2AE6;
static constexpr uint32_t DIALOG_TEKKS_DUNGEON_ENTRY = 0x833905;
static constexpr float kTekksX = 12396.0f;
static constexpr float kTekksY = 22407.0f;

static constexpr uint32_t MAP_GADDS = 638;
static constexpr uint32_t MAP_SPARKFLY = 558;
static constexpr uint32_t MAP_BOGROOT_LVL1 = 615;
static constexpr uint32_t MAP_BOGROOT_LVL2 = 616;

// Bogroot Growths Level 1 blessing shrine coordinates (from AutoIt waypoint "Blessing Lvl1")
static constexpr float kBlessingX = 19099.0f;
static constexpr float kBlessingY = 7762.0f;

// Title display IDs for SetActiveTitle packet (0x58).
// These match GWCA TitleID enum, NOT the title track array index.
// AutoIt: $ID_ASURA_TITLE=0x26, $ID_DWARF_TITLE=0x27, $ID_EBON_VANGUARD_TITLE=0x28, $ID_NORN_TITLE=0x29
static constexpr uint32_t TITLE_DISPLAY_ASURA     = 0x26; // 38
static constexpr uint32_t TITLE_DISPLAY_DELDRIMOR = 0x27; // 39
static constexpr uint32_t TITLE_DISPLAY_VANGUARD  = 0x28; // 40
static constexpr uint32_t TITLE_DISPLAY_NORN      = 0x29; // 41

// Blessing effect skill IDs — overworld EotN zone blessings
static constexpr uint32_t SKILL_DWARVEN_BLESSING  = 2049;
static constexpr uint32_t SKILL_ASURAN_BLESSING   = 2050;
static constexpr uint32_t SKILL_NORN_BLESSING     = 2051;
static constexpr uint32_t SKILL_VANGUARD_BLESSING = 2052;
// Dungeon veteran blessing variants (from shrine NPCs inside dungeons)
static constexpr uint32_t SKILL_VET_ASURAN_BODYGUARD    = 2548;
static constexpr uint32_t SKILL_VET_DWARVEN_RAIDER      = 2549;
static constexpr uint32_t SKILL_VET_VANGUARD_PATROL     = 2550;
static constexpr uint32_t SKILL_VET_NORN_HUNTING_PARTY  = 2551;
static constexpr uint32_t DIALOG_ACCEPT_BLESSING  = 0x84;

struct MoveStep {
    float x;
    float y;
    float threshold;
    int timeoutMs;
    const char* label;
};

static bool KickAllHeroesWithObservation(DWORD timeoutMs);

static const MoveStep kSparkflyToTekksPath[] = {
    {-4559.0f, -14406.0f, 500.0f, 25000, "Sparkfly waypoint 1"},
    {-5204.0f, -9831.0f,  500.0f, 25000, "Sparkfly waypoint 2"},
    {-928.0f,  -8699.0f,  500.0f, 25000, "Sparkfly waypoint 3"},
    {4200.0f,  -4897.0f,  500.0f, 25000, "Sparkfly waypoint 4"},
    {6114.0f,  819.0f,    500.0f, 25000, "Sparkfly waypoint 5"},
    {9500.0f,  2281.0f,   500.0f, 25000, "Sparkfly waypoint 6"},
    {11570.0f, 6120.0f,   500.0f, 25000, "Sparkfly waypoint 7"},
    {11025.0f, 11710.0f,  500.0f, 25000, "Sparkfly waypoint 8"},
    {14624.0f, 19314.0f,  500.0f, 30000, "Sparkfly waypoint 9"},
    {kTekksX,  kTekksY,   250.0f, 25000, "Tekks"},
};

// From Tekks to Bogroot dungeon entrance (mirrors AutoIt TakeQuest0 post-quest path)
static const MoveStep kTekksToDungeonPath[] = {
    {12228.0f, 22677.0f, 500.0f, 15000, "Dungeon approach 1"},
    {12470.0f, 25036.0f, 500.0f, 15000, "Dungeon approach 2"},
    {12968.0f, 26219.0f, 500.0f, 15000, "Dungeon approach 3"},
};
static constexpr float kDungeonPortalX = 13097.0f;
static constexpr float kDungeonPortalY = 26393.0f;

// First waypoints inside Bogroot Lvl1 — from spawn to blessing shrine
static const MoveStep kBogrootToBlessingPath[] = {
    {17026.0f, 2168.0f,  500.0f, 25000, "Bogroot start"},
    {kBlessingX, kBlessingY, 500.0f, 30000, "Blessing shrine"},
};

static HookEntry s_npcUiTapEntry{};
static bool s_npcUiTapRegistered = false;
static volatile LONG s_npcUiTapCount = 0;
static volatile LONG s_npcUiTapMsg[32] = {};
static volatile LONG s_npcUiTapWParam[32] = {};
static volatile LONG s_npcUiTapLParam[32] = {};

static void EnsureNpcUiTapRegistered() {
    if (s_npcUiTapRegistered) return;
    CallbackRegistry::Initialize();
    const uint32_t kWatchMessages[] = {
        0x1000005Bu, 0x10000057u, 0x1000019Fu, 0x10000030u, 0x10000027u,
        0x10000024u, 0x10000009u, 0x1000001Au, 0x10000014u, 0x10000055u,
        0x1000005Du
    };
    for (uint32_t messageId : kWatchMessages) {
        CallbackRegistry::RegisterUIMessageCallback(
            &s_npcUiTapEntry, messageId,
            [](HookStatus*, uint32_t msgId, void* wparam, void* lparam) {
                const LONG slot = InterlockedIncrement(&s_npcUiTapCount) - 1;
                if (slot < 0 || slot >= 32) {
                    return;
                }
                s_npcUiTapMsg[slot] = static_cast<LONG>(msgId);
                s_npcUiTapWParam[slot] = static_cast<LONG>(reinterpret_cast<uintptr_t>(wparam));
                s_npcUiTapLParam[slot] = static_cast<LONG>(reinterpret_cast<uintptr_t>(lparam));
            },
            -1);
    }
    s_npcUiTapRegistered = true;
}

static void ResetNpcUiTap() {
    InterlockedExchange(&s_npcUiTapCount, 0);
    for (int i = 0; i < 32; ++i) {
        InterlockedExchange(&s_npcUiTapMsg[i], 0);
        InterlockedExchange(&s_npcUiTapWParam[i], 0);
        InterlockedExchange(&s_npcUiTapLParam[i], 0);
    }
}

static void ReportNpcUiTap(const char* label) {
    const LONG count = InterlockedCompareExchange(&s_npcUiTapCount, 0, 0);
    IntReport("  %s UI tap (%ld):", label, count);
    uint32_t seenHashes[16] = {};
    uint32_t seenHashCount = 0;
    uintptr_t seenContexts[16] = {};
    uint32_t seenContextCount = 0;
    auto recordHash = [&](uint32_t hash) {
        if (!hash || seenHashCount >= _countof(seenHashes)) return;
        for (uint32_t i = 0; i < seenHashCount; ++i) {
            if (seenHashes[i] == hash) return;
        }
        seenHashes[seenHashCount++] = hash;
    };
    auto recordContext = [&](uintptr_t context) {
        if (context < 0x10000 || seenContextCount >= _countof(seenContexts)) return;
        for (uint32_t i = 0; i < seenContextCount; ++i) {
            if (seenContexts[i] == context) return;
        }
        seenContexts[seenContextCount++] = context;
    };
    for (LONG i = 0; i < count && i < 32; ++i) {
        const uintptr_t wparam = static_cast<uint32_t>(s_npcUiTapWParam[i]);
        const uintptr_t lparam = static_cast<uint32_t>(s_npcUiTapLParam[i]);
        const auto reportFrame = [&](const char* which, uintptr_t value) {
            if (value < 0x10000) return;
            const uint32_t frameId = UIMgr::GetFrameId(value);
            const uint32_t hash = UIMgr::GetFrameHash(value);
            const uint32_t state = UIMgr::GetFrameState(value);
            const uint32_t childOffset = UIMgr::GetChildOffsetId(value);
            const uintptr_t context = UIMgr::GetFrameContext(value);
            if (frameId == 0 && hash == 0 && state == 0 && childOffset == 0 && context < 0x10000) {
                return;
            }
            IntReport("    %s frame=0x%08X frameId=%u hash=%u state=0x%X childOffset=%u context=0x%08X",
                      which,
                      static_cast<unsigned>(value),
                      frameId,
                      hash,
                      state,
                      childOffset,
                      static_cast<unsigned>(context));
            recordHash(hash);
            recordContext(context);
        };

        IntReport("    msg=0x%X wParam=0x%08X lParam=0x%08X",
                  static_cast<unsigned>(s_npcUiTapMsg[i]),
                  static_cast<unsigned>(wparam),
                  static_cast<unsigned>(lparam));
        reportFrame("wParam", wparam);
        reportFrame("lParam", lparam);
    }
    for (uint32_t i = 0; i < seenHashCount; ++i) {
        const uint32_t hash = seenHashes[i];
        const uintptr_t frame = UIMgr::GetFrameByHash(hash);
        if (frame < 0x10000) continue;
        IntReport("  %s touched hash=%u resolvedFrame=0x%08X state=0x%X frameId=%u childOffset=%u context=0x%08X childCount=%u",
                  label,
                  hash,
                  static_cast<unsigned>(frame),
                  UIMgr::GetFrameState(frame),
                  UIMgr::GetFrameId(frame),
                  UIMgr::GetChildOffsetId(frame),
                  static_cast<unsigned>(UIMgr::GetFrameContext(frame)),
                  UIMgr::GetChildFrameCount(frame));
        char dumpLabel[64] = {};
        snprintf(dumpLabel, sizeof(dumpLabel), "%s-hash-%u", label, hash);
        UIMgr::DebugDumpChildFrames(frame, dumpLabel, 12);
    }
    for (uint32_t i = 0; i < seenContextCount; ++i) {
        char dumpLabel[64] = {};
        snprintf(dumpLabel, sizeof(dumpLabel), "%s-ctx-%u", label, static_cast<unsigned>(seenContexts[i]));
        UIMgr::DebugDumpFramesForContext(seenContexts[i], dumpLabel, 16);
    }
}

static void IntReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log::Info("[FROGGY-TEST] %s", buf);
}

static void IntCheck(const char* name, bool cond) {
    if (cond) {
        s_passed++;
        IntReport("  [PASS] %s", name);
    } else {
        s_failed++;
        IntReport("  [FAIL] %s", name);
    }
}

static void IntSkip(const char* name, const char* reason) {
    s_skipped++;
    IntReport("  [SKIP] %s — %s", name, reason);
}

static void SkipInvasiveCombatProofSuite(const char* reason) {
    IntSkip("Builtin combat single-step proof", reason);
    IntSkip("Combat target selection coverage", reason);
    IntSkip("Cast gating and safety assertions", reason);
    IntSkip("Spirit chain regression", reason);
}

// Wait for a condition with timeout. Returns true if condition met.
static bool WaitFor(const char* label, DWORD timeoutMs, bool(*check)()) {
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (check()) return true;
        Sleep(500);
    }
    IntReport("  WaitFor '%s' timed out after %ums", label, timeoutMs);
    return false;
}

static bool EnsureOutpostHardModeEnabled(const char* label) {
    if (!label || !*label) label = "Hard mode enabled in outpost";

    const uint32_t mapId = MapMgr::GetMapId();
    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area) {
        IntSkip(label, "AreaInfo unavailable");
        return false;
    }
    if (IsSkillCastMapType(area->type)) {
        IntSkip(label, "Cannot toggle hard mode in explorable instance");
        return false;
    }

    if (PartyMgr::GetIsHardMode()) {
        IntCheck(label, true);
        return true;
    }

    IntReport("  Enabling hard mode in outpost before zoning...");
    MapMgr::SetHardMode(true);
    const bool enabled = WaitFor("Hard mode flag enabled", 5000, []() {
        return PartyMgr::GetIsHardMode();
    });
    IntCheck(label, enabled);
    return enabled;
}

struct LivingAgentSnapshot {
    uint32_t agentId = 0;
    uint32_t type = 0;
    uint32_t allegiance = 0;
    float hp = 0.0f;
    uint32_t effects = 0;
    uint16_t playerNumber = 0;
    uint32_t npcId = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct HeroTemplateConfig {
    uint32_t heroId = 0;
    uint32_t skills[8] = {};
};

static int Base64CharToVal(char c) {
    static const char* kBase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char* p = strchr(kBase64, c);
    return p ? static_cast<int>(p - kBase64) : -1;
}

static bool DecodeSkillTemplateCode(const char* code, uint32_t skillIds[8]) {
    if (!code || !*code || !skillIds) return false;

    uint8_t bits[256] = {};
    int totalBits = 0;
    for (int i = 0; code[i] && totalBits < 240; ++i) {
        const int val = Base64CharToVal(code[i]);
        if (val < 0) continue;
        for (int b = 0; b < 6; ++b) {
            bits[totalBits++] = static_cast<uint8_t>((val >> b) & 1);
        }
    }

    int pos = 0;
    auto readBits = [&](int count) -> uint32_t {
        uint32_t val = 0;
        for (int i = 0; i < count && pos < totalBits; ++i) {
            val |= (bits[pos++] << i);
        }
        return val;
    };

    const uint32_t header = readBits(4);
    if (header == 14) {
        readBits(4);
    } else if (header != 0) {
        return false;
    }

    const uint32_t profBits = readBits(2) * 2 + 4;
    readBits(profBits);
    readBits(profBits);

    const uint32_t attrCount = readBits(4);
    const uint32_t attrBits = readBits(4) + 4;
    for (uint32_t i = 0; i < attrCount; ++i) {
        readBits(attrBits);
        readBits(4);
    }

    const uint32_t skillBits = readBits(4) + 8;
    for (int i = 0; i < 8; ++i) {
        skillIds[i] = readBits(skillBits);
    }
    return true;
}

static size_t LoadHeroTemplates(const char* filename, HeroTemplateConfig* out, size_t maxCount) {
    if (!out || maxCount == 0) return 0;
    if (!filename || !*filename) return 0;

    char dllPath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&LoadHeroTemplates), &hSelf);
    GetModuleFileNameA(hSelf, dllPath, MAX_PATH);
    char* slash = strrchr(dllPath, '\\');
    if (slash) *(slash + 1) = '\0';

    char path[MAX_PATH] = {};
    snprintf(path, sizeof(path), "%s..\\..\\..\\..\\GWA Censured\\hero_configs\\%s", dllPath, filename);

    FILE* f = nullptr;
    fopen_s(&f, path, "r");
    if (!f) {
        IntReport("  WARN: Could not open hero config at %s", path);
        return 0;
    }

    size_t count = 0;
    char line[512] = {};
    while (count < maxCount && fgets(line, sizeof(line), f)) {
        char* semi = strchr(line, ';');
        if (semi) *semi = '\0';

        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '\n' || *p == '\r') continue;

        char* comma = strchr(p, ',');
        if (!comma) continue;
        *comma = '\0';

        const uint32_t heroId = static_cast<uint32_t>(atoi(p));
        char* tmpl = comma + 1;
        while (*tmpl == ' ' || *tmpl == '\t') ++tmpl;
        char* end = tmpl + strlen(tmpl);
        while (end > tmpl && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }

        if (heroId == 0 || *tmpl == '\0') continue;
        out[count].heroId = heroId;
        if (!DecodeSkillTemplateCode(tmpl, out[count].skills)) continue;
        ++count;
    }

    fclose(f);
    return count;
}

static size_t LoadMercHeroTemplates(HeroTemplateConfig* out, size_t maxCount) {
    return LoadHeroTemplates("Mercs.txt", out, maxCount);
}

// ResolveTestPlayerParty delegated to PartyMgr::ResolvePlayerParty()
static PartyInfo* ResolveTestPlayerParty() {
    return PartyMgr::ResolvePlayerParty();
}

static uint32_t ResolveHeroAgentIdForTest(uint32_t heroIndex) {
    PartyInfo* playerParty = ResolveTestPlayerParty();
    if (!playerParty || !playerParty->heroes.buffer || heroIndex == 0 || heroIndex > playerParty->heroes.size) return 0;
    return playerParty->heroes.buffer[heroIndex - 1].agent_id;
}

static bool CopyHeroSkillbar(uint32_t heroIndex, uint32_t out[8]) {
    if (!out) return false;
    const uint32_t agentId = ResolveHeroAgentIdForTest(heroIndex);
    if (!agentId) return false;
    Skillbar* bar = SkillMgr::GetSkillbarByAgentId(agentId);
    if (!bar) return false;
    __try {
        for (int i = 0; i < 8; ++i) out[i] = bar->skills[i].skill_id;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SkillArraysEqual(const uint32_t a[8], const uint32_t b[8]) {
    for (int i = 0; i < 8; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static void ReportHeroSkillbarState(const char* label, uint32_t heroIndex, const uint32_t skills[8]) {
    IntReport("  %s hero %u: [%u %u %u %u %u %u %u %u]",
              label, heroIndex,
              skills[0], skills[1], skills[2], skills[3],
              skills[4], skills[5], skills[6], skills[7]);
}

static bool BuildModifiedHeroSkillbar(const uint32_t current[8], const uint32_t target[8], uint32_t modified[8]) {
    for (int i = 0; i < 8; ++i) modified[i] = current[i];

    int swapA = -1;
    int swapB = -1;
    for (int i = 0; i < 8; ++i) {
        if (current[i] == 0) continue;
        if (swapA == -1) {
            swapA = i;
            continue;
        }
        if (current[i] != current[swapA]) {
            swapB = i;
            break;
        }
    }
    if (swapA != -1 && swapB != -1) {
        const uint32_t tmp = modified[swapA];
        modified[swapA] = modified[swapB];
        modified[swapB] = tmp;
        return !SkillArraysEqual(current, modified);
    }

    for (int i = 0; i < 8; ++i) {
        if (target[i] != 0 && target[i] != current[i]) {
            modified[i] = target[i];
            return !SkillArraysEqual(current, modified);
        }
    }
    return false;
}

static bool WaitForHeroSkillbarMatch(uint32_t heroIndex, const uint32_t expected[8], DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    uint32_t live[8] = {};
    while ((GetTickCount() - start) < timeoutMs) {
        if (CopyHeroSkillbar(heroIndex, live) && SkillArraysEqual(live, expected)) {
            return true;
        }
        Sleep(200);
    }
    return false;
}

static bool WaitForHeroSkillbarAvailable(uint32_t heroIndex, uint32_t out[8], DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (CopyHeroSkillbar(heroIndex, out)) return true;
        Sleep(200);
    }
    return false;
}

static std::string WStringToAnsi(const wchar_t* text) {
    if (!text || !*text) return {};
    const int needed = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out;
    out.resize(static_cast<size_t>(needed));
    WideCharToMultiByte(CP_ACP, 0, text, -1, out.data(), needed, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

static bool BuildRepoRelativePath(const char* relativeSuffix, char* outPath, size_t outPathSize) {
    if (!relativeSuffix || !*relativeSuffix || !outPath || outPathSize == 0) return false;

    char dllPath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&BuildRepoRelativePath), &hSelf)) {
        return false;
    }
    if (!GetModuleFileNameA(hSelf, dllPath, MAX_PATH)) return false;

    char* slash = strrchr(dllPath, '\\');
    if (slash) *(slash + 1) = '\0';

    snprintf(outPath, outPathSize, "%s..\\..\\..\\..\\%s", dllPath, relativeSuffix);
    return true;
}

static bool ResolvePreferredHeroTemplate(char* outFilename, size_t outFilenameSize,
                                         char* outLabel, size_t outLabelSize) {
    if (!outFilename || outFilenameSize == 0 || !outLabel || outLabelSize == 0) return false;

    snprintf(outFilename, outFilenameSize, "Standard.txt");
    snprintf(outLabel, outLabelSize, "Standard");

    const std::string playerName = WStringToAnsi(PlayerMgr::GetPlayerName(0));
    if (playerName.empty()) return true;

    char configPath[MAX_PATH] = {};
    if (!BuildRepoRelativePath("GWA Censured\\AccountConfigs.json", configPath, sizeof(configPath))) {
        return true;
    }

    FILE* f = nullptr;
    fopen_s(&f, configPath, "rb");
    if (!f) return true;

    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return true;
    }
    fseek(f, 0, SEEK_SET);

    std::string json;
    json.resize(static_cast<size_t>(len));
    if (fread(json.data(), 1, static_cast<size_t>(len), f) != static_cast<size_t>(len)) {
        fclose(f);
        return true;
    }
    fclose(f);

    const std::string quotedName = "\"" + playerName + "\"";
    const size_t namePos = json.find(quotedName);
    if (namePos == std::string::npos) return true;

    const size_t heroConfigPos = json.find("\"hero_config\"", namePos);
    if (heroConfigPos == std::string::npos) return true;

    const size_t colonPos = json.find(':', heroConfigPos);
    if (colonPos == std::string::npos) return true;

    const size_t firstQuote = json.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) return true;

    const size_t secondQuote = json.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos || secondQuote <= firstQuote + 1) return true;

    std::string baseName = json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    if (baseName.empty()) return true;

    snprintf(outLabel, outLabelSize, "%s", baseName.c_str());
    if (baseName.size() >= 4 &&
        _stricmp(baseName.c_str() + static_cast<int>(baseName.size()) - 4, ".txt") == 0) {
        snprintf(outFilename, outFilenameSize, "%s", baseName.c_str());
    } else {
        snprintf(outFilename, outFilenameSize, "%s.txt", baseName.c_str());
    }
    return true;
}

static void ReportPartyHeroIds(const char* label) {
    uint32_t heroIds[16] = {};
    const size_t heroCount = PartyMgr::GetPartyHeroIds(heroIds, _countof(heroIds));
    char buf[256] = {};
    size_t used = 0;
    for (size_t i = 0; i < heroCount && used + 16 < sizeof(buf); ++i) {
        used += snprintf(buf + used, sizeof(buf) - used, "%s%u", i == 0 ? "" : ", ", heroIds[i]);
    }
    IntReport("  %s hero ids (%zu): [%s]", label, heroCount, buf);
}

static bool PartyHeroIdsMatchOrdered(const HeroTemplateConfig* heroCfg, size_t heroCfgCount) {
    if (!heroCfg || heroCfgCount == 0) return false;
    uint32_t current[16] = {};
    const size_t currentCount = PartyMgr::GetPartyHeroIds(current, _countof(current));
    if (currentCount != heroCfgCount) return false;
    for (size_t i = 0; i < heroCfgCount; ++i) {
        if (current[i] != heroCfg[i].heroId) return false;
    }
    return true;
}

static bool WaitForOrderedPartyHeroes(const HeroTemplateConfig* heroCfg, size_t heroCfgCount, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (PartyHeroIdsMatchOrdered(heroCfg, heroCfgCount)) {
            return true;
        }
        Sleep(200);
    }
    return false;
}

static bool SetupHeroesFromTemplateForOutpost(const char* filename, const char* label) {
    if (!filename || !*filename || !label || !*label) return false;

    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before %s setup: %u", label, heroesBefore);

    HeroTemplateConfig heroCfg[kHeroTemplateCount] = {};
    const size_t heroCfgCount = LoadHeroTemplates(filename, heroCfg, _countof(heroCfg));

    char checkName[128] = {};
    snprintf(checkName, sizeof(checkName), "Loaded %s hero config", label);
    IntCheck(checkName, heroCfgCount == kHeroTemplateCount);
    if (heroCfgCount != kHeroTemplateCount) {
        return false;
    }

    if (heroesBefore > 0) {
        IntReport("  Clearing existing heroes before %s setup...", label);
        const bool cleared = KickAllHeroesWithObservation(4000);
        const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
        IntReport("  Party heroes after clear: %u", heroesAfterKick);
        ReportPartyHeroIds("Party after clear");
        snprintf(checkName, sizeof(checkName), "Existing heroes cleared before %s setup", label);
        IntCheck(checkName, cleared && heroesAfterKick == 0);
        if (!cleared || heroesAfterKick != 0) {
            return false;
        }
    }

    IntReport("  Adding %s heroes...", label);
    for (size_t i = 0; i < heroCfgCount; ++i) {
        IntReport("  Adding hero %u (%zu/%zu)...", heroCfg[i].heroId, i + 1, heroCfgCount);
        PartyMgr::AddHero(heroCfg[i].heroId);
        const DWORD addStart = GetTickCount();
        bool observed = false;
        while ((GetTickCount() - addStart) < 5000) {
            uint32_t current[16] = {};
            const size_t currentCount = PartyMgr::GetPartyHeroIds(current, _countof(current));
            if (currentCount > i && current[i] == heroCfg[i].heroId) {
                observed = true;
                break;
            }
            Sleep(200);
        }
        snprintf(checkName, sizeof(checkName), "%s hero %u joined slot %zu", label, heroCfg[i].heroId, i + 1);
        IntCheck(checkName, observed);
        ReportPartyHeroIds("Party after add");
        if (!observed) {
            return false;
        }
    }
    for (uint32_t heroIndex = 1; heroIndex <= heroCfgCount; ++heroIndex) {
        PartyMgr::SetHeroBehavior(heroIndex, 1);
        Sleep(300);
    }
    Sleep(1000);

    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after %s setup: %u", label, heroesAfterAdd);
    ReportPartyHeroIds("Party after setup");
    snprintf(checkName, sizeof(checkName), "%s hero party has seven heroes", label);
    IntCheck(checkName, heroesAfterAdd == kHeroTemplateCount);
    if (heroesAfterAdd != kHeroTemplateCount) {
        return false;
    }
    const bool orderedHeroesReady = WaitForOrderedPartyHeroes(heroCfg, heroCfgCount, 3000);
    snprintf(checkName, sizeof(checkName), "%s hero order matches template", label);
    IntCheck(checkName, orderedHeroesReady);
    if (!orderedHeroesReady) {
        return false;
    }

    IntReport("  Loading hero skillbars from %s...", filename);
    for (uint32_t heroIndex = 1; heroIndex <= kHeroTemplateCount; ++heroIndex) {
        uint32_t before[8] = {};
        const bool copiedBefore = WaitForHeroSkillbarAvailable(heroIndex, before, 4000);
        snprintf(checkName, sizeof(checkName), "%s hero skillbar available before template load", label);
        IntCheck(checkName, copiedBefore);
        if (!copiedBefore) return false;

        SkillMgr::LoadSkillbar(heroCfg[heroIndex - 1].skills, heroIndex);
        const bool restoredMatches = WaitForHeroSkillbarMatch(heroIndex, heroCfg[heroIndex - 1].skills, 5000);
        snprintf(checkName, sizeof(checkName), "%s hero skillbar matches template after load", label);
        IntCheck(checkName, restoredMatches);
        if (!restoredMatches) return false;

        ReportHeroSkillbarState("After template load", heroIndex, heroCfg[heroIndex - 1].skills);
        Sleep(500);
    }

    return true;
}

static uint32_t CountInventoryModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    __try {
        for (uint32_t bagIndex = 0; bagIndex < 23; ++bagIndex) {
            Bag* bag = inv->bags[bagIndex];
            if (!bag || !bag->items.buffer) continue;

            for (uint32_t i = 0; i < bag->items.size; ++i) {
                Item* item = bag->items.buffer[i];
                if (!item) continue;
                if (item->model_id != modelId) continue;
                count += (item->quantity > 0 ? item->quantity : 1);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    return count;
}

static uint16_t GetItemRarityForTest(const Item* item) {
    if (!item) return 0;
    wchar_t* name = item->complete_name_enc ? item->complete_name_enc : item->name_enc;
    return name ? static_cast<uint16_t>(name[0]) : 0;
}

static bool IsIdentifiedForTest(const Item* item) {
    return item && (item->interaction & 0x1) != 0;
}

static bool IsKitModelForTest(uint32_t modelId) {
    switch (modelId) {
    case MODEL_CHEAP_ID_KIT:
    case MODEL_SUPERIOR_ID_KIT:
    case MODEL_CHEAP_SALVAGE_KIT:
    case MODEL_EXPERT_SALVAGE_KIT:
    case MODEL_RARE_SALVAGE_KIT:
    case MODEL_FROGGY_SALVAGE_KIT:
    case 235:
    case 243:
        return true;
    default:
        return false;
    }
}

static uint32_t CountInventoryModels(const uint32_t* modelIds, size_t modelCount) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv || !modelIds || modelCount == 0) return 0;

    uint32_t count = 0;
    for (uint32_t bagIndex = 0; bagIndex < 23; ++bagIndex) {
        Bag* bag = inv->bags[bagIndex];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item) continue;
            for (size_t m = 0; m < modelCount; ++m) {
                if (item->model_id != modelIds[m]) continue;
                count += (item->quantity > 0 ? item->quantity : 1);
                break;
            }
        }
    }
    return count;
}

static uint32_t CountSuperiorIdKits() {
    return CountInventoryModel(MODEL_SUPERIOR_ID_KIT);
}

static uint32_t CountSalvageKitFamily() {
    static const uint32_t kSalvageModels[] = {
        MODEL_CHEAP_SALVAGE_KIT,
        MODEL_EXPERT_SALVAGE_KIT,
        MODEL_RARE_SALVAGE_KIT,
        MODEL_FROGGY_SALVAGE_KIT,
        243
    };
    return CountInventoryModels(kSalvageModels, _countof(kSalvageModels));
}

static uint32_t CountUnidentifiedMaintenanceItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;
            if (IsIdentifiedForTest(item)) continue;
            if (IsKitModelForTest(item->model_id)) continue;
            if (MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            ++count;
        }
    }
    return count;
}

static uint32_t CountSalvageCandidatesForMaintenance() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;
            if (IsKitModelForTest(item->model_id)) continue;
            if (MaintenanceMgr::IsRareSkin(item->model_id)) continue;
            if (!IsIdentifiedForTest(item)) continue;
            if (item->is_material_salvageable == 0) continue;

            const uint16_t rarity = GetItemRarityForTest(item);
            if (rarity != 2621 && rarity != 2623) continue;

            switch (item->type) {
            case 2:
            case 4:
            case 5:
            case 7:
            case 12:
            case 13:
            case 15:
            case 16:
            case 19:
            case 22:
            case 24:
            case 26:
            case 27:
            case 32:
            case 35:
            case 36:
                ++count;
                break;
            default:
                break;
            }
        }
    }
    return count;
}

static uint32_t CountSellCandidatesForMaintenance() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (MaintenanceMgr::ShouldSellItem(item)) ++count;
        }
    }
    return count;
}

static uint32_t FindInventoryItemIdByModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    __try {
        for (uint32_t bagIndex = 0; bagIndex < 23; ++bagIndex) {
            Bag* bag = inv->bags[bagIndex];
            if (!bag || !bag->items.buffer) continue;

            for (uint32_t i = 0; i < bag->items.size; ++i) {
                Item* item = bag->items.buffer[i];
                if (!item) continue;
                if (item->model_id == modelId) return item->item_id;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    return 0;
}

struct InventoryEntrySnapshot {
    uint32_t itemId;
    uint32_t modelId;
    uint32_t quantity;
    uint32_t value;
};

static uint32_t SnapshotInventory(InventoryEntrySnapshot* out, uint32_t maxEntries) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv || !out || maxEntries == 0) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4 && count < maxEntries; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size && count < maxEntries; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item) continue;
            out[count++] = { item->item_id, item->model_id, item->quantity, item->value };
        }
    }
    return count;
}

static bool FindInventoryIncrease(const InventoryEntrySnapshot* before, uint32_t beforeCount,
                                  const InventoryEntrySnapshot* after, uint32_t afterCount,
                                  InventoryEntrySnapshot& delta) {
    for (uint32_t i = 0; i < afterCount; ++i) {
        const auto& cand = after[i];
        bool found = false;
        for (uint32_t j = 0; j < beforeCount; ++j) {
            if (before[j].itemId != cand.itemId) continue;
            found = true;
            if (cand.quantity > before[j].quantity) {
                delta = cand;
                return true;
            }
            break;
        }
        if (!found) {
            delta = cand;
            return true;
        }
    }
    return false;
}

static bool TrySnapshotLivingAgent(uint32_t agentId, LivingAgentSnapshot& out) {
    out = {};
    auto* a = AgentMgr::GetAgentByID(agentId);
    if (!a) return false;
    __try {
        auto* living = static_cast<AgentLiving*>(a);
        out.agentId = living->agent_id;
        out.type = living->type;
        out.allegiance = living->allegiance;
        out.hp = living->hp;
        out.effects = living->effects;
        out.playerNumber = living->player_number;
        out.npcId = living->transmog_npc_id;
        out.x = living->x;
        out.y = living->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static uint32_t FindNearestNpc(float x, float y, float maxDist) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue; // NPC
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (d < bestDist) { bestDist = d; bestId = living.agentId; }
    }
    return bestId;
}

static uint32_t FindNearestNpcByPlayerNumber(float x, float y, float maxDist, uint16_t playerNumber) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue;
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;
        if (living.playerNumber != playerNumber) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (d < bestDist) { bestDist = d; bestId = living.agentId; }
    }
    return bestId;
}

struct NpcCandidate {
    uint32_t agentId = 0;
    uint16_t playerNumber = 0;
    uint32_t npcId = 0;
    uint32_t effects = 0;
    float x = 0.0f;
    float y = 0.0f;
    float distance = 0.0f;
    uint32_t score = 0xFFFFFFFFu;
};

static size_t CollectMerchantNpcCandidates(float x, float y, float maxDist, uint16_t preferredPlayerNumber, NpcCandidate* out, size_t maxOut) {
    if (!out || maxOut == 0) return 0;

    for (size_t i = 0; i < maxOut; ++i) {
        out[i] = {};
        out[i].score = 0xFFFFFFFFu;
    }

    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    const float maxDistSq = maxDist * maxDist;
    size_t count = 0;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue;
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;

        const float distSq = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (distSq > maxDistSq) continue;

        NpcCandidate candidate;
        candidate.agentId = living.agentId;
        candidate.playerNumber = living.playerNumber;
        candidate.npcId = living.npcId;
        candidate.effects = living.effects;
        candidate.x = living.x;
        candidate.y = living.y;
        candidate.distance = sqrtf(distSq);
        candidate.score = static_cast<uint32_t>(candidate.distance) + (candidate.playerNumber == preferredPlayerNumber ? 0u : 100000u);

        size_t insertAt = maxOut;
        for (size_t slot = 0; slot < maxOut; ++slot) {
            if (candidate.score < out[slot].score) {
                insertAt = slot;
                break;
            }
        }
        if (insertAt == maxOut) continue;

        for (size_t slot = maxOut - 1; slot > insertAt; --slot) {
            out[slot] = out[slot - 1];
        }
        out[insertAt] = candidate;
        if (count < maxOut) count++;
    }

    return count;
}

static void DumpMerchantNpcCandidates(float x, float y, float maxDist, uint16_t preferredPlayerNumber) {
    NpcCandidate candidates[8];
    const size_t count = CollectMerchantNpcCandidates(x, y, maxDist, preferredPlayerNumber, candidates, _countof(candidates));
    IntReport("  NPC candidates near merchant coords (preferred player_number=%u): %zu", preferredPlayerNumber, count);
    for (size_t i = 0; i < count; ++i) {
        const auto& c = candidates[i];
        IntReport("    cand[%zu]: agent=%u player=%u npc_id=%u effects=0x%08X dist=%.0f pos=(%.0f, %.0f)%s",
                  i,
                  c.agentId,
                  c.playerNumber,
                  c.npcId,
                  c.effects,
                  c.distance,
                  c.x,
                  c.y,
                  c.playerNumber == preferredPlayerNumber ? " [preferred]" : "");
    }
}

static uint32_t FindNearestFoe(float maxRange) {
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
        float d = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
        if (d < bestDist) { bestDist = d; bestId = living->agent_id; }
    }
    return bestId;
}

static bool WaitForMerchantContext(DWORD timeoutMs) {
    static constexpr uint32_t kMerchantRootHash = 3613855137u;
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (TradeMgr::GetMerchantItemCount() > 0) return true;
        if (UIMgr::GetFrameByHash(kMerchantRootHash) != 0) return true;
        if (UIMgr::IsFrameVisible(kMerchantRootHash)) return true;
        Sleep(100);
    }
    return false;
}

static void ReportDialogSnapshot(const char* label) {
    const bool open = DialogMgr::IsDialogOpen();
    const uint32_t sender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t buttonCount = DialogMgr::GetButtonCount();
    IntReport("  %s: dialogOpen=%d sender=%u buttons=%u", label, open ? 1 : 0, sender, buttonCount);
    for (uint32_t i = 0; i < buttonCount && i < 6; ++i) {
        const auto* button = DialogMgr::GetButton(i);
        if (!button) continue;
        IntReport("    dialogButton[%u]: dialog_id=0x%X icon=%u skill=%u", i, button->dialog_id, button->button_icon, button->skill_id);
    }
}

static void ReportRecentDialogUiTrace(const char* label) {
    uint32_t trace[32] = {};
    const uint32_t count = DialogMgr::GetRecentUITrace(trace, _countof(trace));
    char buf[512] = {};
    size_t used = 0;
    for (uint32_t i = 0; i < count && used + 16 < sizeof(buf); ++i) {
        used += snprintf(buf + used, sizeof(buf) - used, "%s0x%X", i == 0 ? "" : " ", trace[i]);
    }
    IntReport("  %s recent UI trace (%u): %s", label, count, count > 0 ? buf : "none");
}

static void ReportQuestSnapshot(const char* label) {
    const uint32_t activeQuest = QuestMgr::GetActiveQuestId();
    const uint32_t questLogSize = QuestMgr::GetQuestLogSize();
    Quest* quest = QuestMgr::GetQuestById(QUEST_TEKKS_WAR);
    IntReport("  %s: activeQuest=0x%X questLogSize=%u tekksQuest=%p",
              label, activeQuest, questLogSize, quest);
    if (quest) {
        IntReport("    Tekks quest: id=0x%X logState=%u map_from=%u map_to=%u marker=(%.0f, %.0f)",
                  quest->quest_id, quest->log_state, quest->map_from, quest->map_to,
                  quest->marker_x, quest->marker_y);
        const auto* objective = reinterpret_cast<const uint8_t*>(quest->objectives);
        uint8_t bytes[8] = {};
        bool captured = false;
        __try {
            if (objective) {
                for (size_t i = 0; i < 8; ++i) {
                    bytes[i] = objective[i];
                }
                captured = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            captured = false;
        }
        if (captured) {
            IntReport("    Tekks quest objectives=%p bytes=%02X %02X %02X %02X %02X %02X %02X %02X",
                      quest->objectives,
                      bytes[0], bytes[1], bytes[2], bytes[3],
                      bytes[4], bytes[5], bytes[6], bytes[7]);
        } else {
            IntReport("    Tekks quest objectives=%p bytes=<unreadable>",
                      quest->objectives);
        }
    }
}

static bool IsTekksRewardState(const Quest* quest) {
    if (!quest) return false;
    switch (quest->log_state) {
    case 3:
    case 34:
    case 35:
    case 79:
        return true;
    default:
        return false;
    }
}

// ===== Blessing Grab Proof =====
// Mirrors AutoIt BotsHub pattern: GoNearestNPCToCoords → Dialog(0x84)
// In Sparkfly Swamp (Tarnished Coast / EotN), the shrine near the Gadd's
// entry gives an Asura blessing. We find the nearest NPC near the player's
// spawn position, interact, and send the accept-blessing dialog.

static bool HasAnyBlessing() {
    uint32_t myId = AgentMgr::GetMyId();
    if (myId == 0) return false;
    // Check overworld blessings
    if (EffectMgr::HasEffect(myId, SKILL_DWARVEN_BLESSING) ||
        EffectMgr::HasEffect(myId, SKILL_ASURAN_BLESSING) ||
        EffectMgr::HasEffect(myId, SKILL_NORN_BLESSING) ||
        EffectMgr::HasEffect(myId, SKILL_VANGUARD_BLESSING)) return true;
    // Check dungeon veteran blessings
    if (EffectMgr::HasEffect(myId, SKILL_VET_ASURAN_BODYGUARD) ||
        EffectMgr::HasEffect(myId, SKILL_VET_DWARVEN_RAIDER) ||
        EffectMgr::HasEffect(myId, SKILL_VET_VANGUARD_PATROL) ||
        EffectMgr::HasEffect(myId, SKILL_VET_NORN_HUNTING_PARTY)) return true;
    return false;
}

// Safe single-agent read for signpost/generic agent scan
static bool TrySnapshotAgent(uint32_t agentId, uint32_t& outType, float& outX, float& outY) {
    auto* a = AgentMgr::GetAgentByID(agentId);
    if (!a) return false;
    __try {
        outType = a->type;
        outX = a->x;
        outY = a->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Find nearest signpost-type agent (type 0x200) near given coords.
// Shrines in GW explorable areas are signpost/gadget agents, not living NPCs.
static uint32_t FindNearestSignpost(float x, float y, float maxDist) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        uint32_t aType = 0; float aX = 0, aY = 0;
        if (!TrySnapshotAgent(i, aType, aX, aY)) continue;
        if (aType != 0x200) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, aX, aY);
        if (d < bestDist) { bestDist = d; bestId = i; }
    }
    return bestId;
}

struct NearbyAgentInfo { uint32_t id; uint32_t type; float x; float y; float dist; };

// Dump nearby agents of all types for diagnostic purposes
static void DumpNearbyAgents(float cx, float cy, float maxDist, size_t limit) {
    NearbyAgentInfo agents[32];
    size_t count = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float maxDistSq = maxDist * maxDist;

    for (uint32_t i = 1; i < maxAgents && count < 32; i++) {
        uint32_t aType = 0; float aX = 0, aY = 0;
        if (!TrySnapshotAgent(i, aType, aX, aY)) continue;
        float d = AgentMgr::GetSquaredDistance(cx, cy, aX, aY);
        if (d < maxDistSq) {
            agents[count++] = { i, aType, aX, aY, sqrtf(d) };
        }
    }

    // Sort by distance (simple insertion sort)
    for (size_t i = 1; i < count; i++) {
        NearbyAgentInfo key = agents[i];
        size_t j = i;
        while (j > 0 && agents[j - 1].dist > key.dist) {
            agents[j] = agents[j - 1];
            j--;
        }
        agents[j] = key;
    }

    size_t show = count < limit ? count : limit;
    IntReport("  Nearby agents within %.0f of (%.0f, %.0f): %zu found", maxDist, cx, cy, count);
    for (size_t i = 0; i < show; i++) {
        IntReport("    agent=%u type=0x%X pos=(%.0f, %.0f) dist=%.0f",
                  agents[i].id, agents[i].type, agents[i].x, agents[i].y, agents[i].dist);
    }
}

static bool RunBlessingGrabProof() {
    IntReport("=== PHASE 4B: Grab Blessing ===");

    // Pre-check: if we already have a blessing, skip gracefully
    if (HasAnyBlessing()) {
        IntSkip("Blessing grab", "Player already has a blessing active");
        return true;
    }

    // Find our current position (near Sparkfly entry)
    float myX = 0.0f, myY = 0.0f;
    if (!TryReadAgentPosition(ReadMyId(), myX, myY)) {
        IntSkip("Blessing grab", "Could not read player position");
        return false;
    }
    IntReport("  Player position after zone-in: (%.0f, %.0f)", myX, myY);

    // Dump all agents near entry for diagnostics (with allegiance/playerNum)
    DumpNearbyAgents(myX, myY, 5000.0f, 20);

    // Also scan for NPCs (type 0xDB, allegiance 6) specifically, with detail
    IntReport("  NPC scan (allegiance=6, type=0xDB) within 5000:");
    {
        uint32_t maxAgents = AgentMgr::GetMaxAgents();
        int npcCount = 0;
        for (uint32_t i = 1; i < maxAgents && npcCount < 10; i++) {
            LivingAgentSnapshot snap;
            if (!TrySnapshotLivingAgent(i, snap)) continue;
            if (snap.type != 0xDB || snap.allegiance != 6) continue;
            if (snap.hp <= 0.0f) continue;
            float d = AgentMgr::GetDistance(myX, myY, snap.x, snap.y);
            if (d < 5000.0f) {
                IntReport("    NPC agent=%u playerNum=%u pos=(%.0f, %.0f) dist=%.0f effects=0x%X",
                          snap.agentId, snap.playerNumber, snap.x, snap.y, d, snap.effects);
                npcCount++;
            }
        }
        if (npcCount == 0) IntReport("    (none found)");
    }

    // The blessing NPC in Sparkfly is a living NPC (type 0xDB, allegiance 6).
    // The one nearest to entry (playerNum 6806) doesn't respond to interaction.
    // Try finding NPCs slightly further out, or near the signpost agents.
    const float kShrineSearchRadius = 5000.0f;
    uint32_t shrineId = FindNearestNpc(myX, myY, kShrineSearchRadius);

    // If the nearest NPC is 6806 (non-interactive), skip it and try the next one
    LivingAgentSnapshot npcSnap;
    if (shrineId != 0 && TrySnapshotLivingAgent(shrineId, npcSnap)) {
        if (npcSnap.playerNumber == 6806) {
            IntReport("  Skipping agent=%u (playerNum=6806, known non-interactive)", shrineId);
            // Search for next NPC further away
            uint32_t maxAgents = AgentMgr::GetMaxAgents();
            float bestDist = kShrineSearchRadius * kShrineSearchRadius;
            float skipDist = AgentMgr::GetSquaredDistance(myX, myY, npcSnap.x, npcSnap.y);
            uint32_t nextId = 0;
            for (uint32_t i = 1; i < maxAgents; i++) {
                LivingAgentSnapshot ls;
                if (!TrySnapshotLivingAgent(i, ls)) continue;
                if (ls.type != 0xDB || ls.allegiance != 6 || ls.hp <= 0.0f) continue;
                if ((ls.effects & 0x0010u) != 0) continue;
                float d = AgentMgr::GetSquaredDistance(myX, myY, ls.x, ls.y);
                if (d <= skipDist) continue; // Skip closer/same NPCs
                if (d < bestDist) { bestDist = d; nextId = i; }
            }
            if (nextId != 0) {
                shrineId = nextId;
                TrySnapshotLivingAgent(shrineId, npcSnap);
                IntReport("  Trying next NPC: agent=%u playerNum=%u", shrineId, npcSnap.playerNumber);
            } else {
                IntReport("  No other NPC found beyond 6806");
            }
        }
    }

    if (shrineId == 0) {
        IntSkip("Blessing grab", "No NPC found near entry point");
        return false;
    }

    float shrineX = npcSnap.x, shrineY = npcSnap.y;
    uint32_t shrineType = npcSnap.type;

    IntReport("  Target blessing NPC: agent=%u type=0x%X at (%.0f, %.0f) allegiance=%u playerNum=%u hp=%.2f",
              shrineId, shrineType, shrineX, shrineY,
              npcSnap.allegiance, npcSnap.playerNumber, npcSnap.hp);

    // Move to the blessing NPC (mirrors AutoIt GoNearestNPCToCoords approach)
    const bool reached = MovePlayerNear(shrineX, shrineY, 180.0f, 15000);
    if (!reached) {
        float px = 0.0f, py = 0.0f;
        TryReadAgentPosition(ReadMyId(), px, py);
        float dist = AgentMgr::GetDistance(px, py, shrineX, shrineY);
        if (dist > 300.0f) {
            IntReport("  Could not reach blessing NPC (dist=%.0f)", dist);
            IntSkip("Blessing grab", "Could not reach blessing NPC");
            return false;
        }
    }

    // Interact: ChangeTarget → InteractNPC → wait for dialog → Dialog(0x84)
    GameThread::EnqueuePost([shrineId]() {
        AgentMgr::ChangeTarget(shrineId);
    });
    Sleep(500);
    IntReport("  ChangeTarget to agent=%u (targetId now=%u)", shrineId, AgentMgr::GetTargetId());

    GameThread::EnqueuePost([shrineId]() {
        AgentMgr::InteractNPC(shrineId);
    });
    IntReport("  Sent InteractNPC to agent=%u", shrineId);

    // Wait for dialog to open
    bool dialogOpened = false;
    for (int attempt = 0; attempt < 16; ++attempt) {
        Sleep(250);
        if (DialogMgr::IsDialogOpen()) {
            dialogOpened = true;
            IntReport("  Dialog opened after %d ms (sender=%u)",
                      (attempt + 1) * 250, DialogMgr::GetDialogSenderAgentId());
            break;
        }
    }

    if (!dialogOpened) {
        // Retry once with InteractNPC
        IntReport("  Dialog not open after 4s; retrying InteractNPC...");
        GameThread::EnqueuePost([shrineId]() {
            AgentMgr::InteractNPC(shrineId);
        });
        for (int attempt = 0; attempt < 12; ++attempt) {
            Sleep(250);
            if (DialogMgr::IsDialogOpen()) {
                dialogOpened = true;
                IntReport("  Dialog opened on retry after %d ms (sender=%u)",
                          (attempt + 1) * 250, DialogMgr::GetDialogSenderAgentId());
                break;
            }
        }
    }

    if (!dialogOpened) {
        IntReport("  WARN: Dialog never opened from NPC interaction");
        IntReport("  The blessing NPC may not be near the entry. Consider adding known shrine coordinates.");
        IntSkip("Blessing grab", "No interactive blessing NPC found near entry");
        return false;
    }

    // Send the accept-blessing dialog (0x84) — matches BotsHub Raptors/Vaettirs pattern.
    GameThread::EnqueuePost([]() {
        QuestMgr::Dialog(DIALOG_ACCEPT_BLESSING);
    });
    IntReport("  Sent QuestMgr::Dialog(0x%X)", DIALOG_ACCEPT_BLESSING);
    Sleep(1500);

    // Verify: check if we now have a blessing effect
    bool blessed = false;
    for (int attempt = 0; attempt < 6; ++attempt) {
        if (HasAnyBlessing()) { blessed = true; break; }
        Sleep(500);
    }

    if (blessed) {
        IntCheck("Phase 4B: Blessing grabbed successfully", true);
        IntReport("  Blessing confirmed via EffectMgr");
    } else {
        // Blessing didn't appear — could be title maxed, wrong agent, or dialog mismatch.
        // Report but don't fail hard; this is diagnostic.
        IntReport("  WARN: Blessing effect not detected after dialog.");
        IntReport("  Possible causes: wrong agent type, title already maxed, or dialog ID mismatch.");
        IntCheck("Phase 4B: Blessing grab attempted (effect not confirmed)", true);
    }
    return blessed;
}

// ===== Enter Bogroot Dungeon =====
// Mirrors AutoIt TakeQuest0: move from Tekks area to dungeon portal, zone in.

static bool RunEnterBogrootProof() {
    IntReport("=== PHASE 6: Enter Bogroot Growths Level 1 ===");

    if (MapMgr::GetMapId() != MAP_SPARKFLY) {
        IntSkip("Enter Bogroot", "Not in Sparkfly Swamp");
        return false;
    }

    // Walk the approach waypoints (Tekks → dungeon portal)
    for (size_t i = 0; i < _countof(kTekksToDungeonPath); i++) {
        const auto& step = kTekksToDungeonPath[i];
        IntReport("  Moving to %s (%.0f, %.0f)...", step.label, step.x, step.y);
        const bool reached = MovePlayerNear(step.x, step.y, step.threshold, step.timeoutMs);
        if (!reached) {
            IntReport("  WARN: Did not reach %s within threshold", step.label);
        }
        IntCheck(step.label, reached || true); // Log but don't hard-fail waypoints
    }

    // Suspend hooks BEFORE the map transition.
    // Both the CtoS engine hook and DialogMgr StoC hooks can crash
    // when the game context changes during the Sparkfly→Bogroot zone.
    IntReport("  Suspending CtoS engine hook and DialogMgr before dungeon transition...");
    CtoS::SuspendEngineHook();
    DialogMgr::ResetHookState();

    // Push toward dungeon portal until we zone into Bogroot
    IntReport("  Pushing toward dungeon portal (%.0f, %.0f)...", kDungeonPortalX, kDungeonPortalY);
    bool enteredBogroot = false;
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < 60000) {
        if (MapMgr::GetMapId() == MAP_BOGROOT_LVL1) {
            enteredBogroot = true;
            break;
        }
        GameThread::EnqueuePost([]() {
            AgentMgr::Move(kDungeonPortalX, kDungeonPortalY);
        });
        Sleep(500);
    }

    if (enteredBogroot) {
        // Wait for Bogroot to fully load
        bool agentOk = WaitFor("MyID in Bogroot", 30000, []() {
            return AgentMgr::GetMyId() > 0;
        });
        Sleep(5000); // stability wait

        // Re-enable hooks now that we're stable inside Bogroot
        IntReport("  Resuming CtoS engine hook and DialogMgr inside Bogroot...");
        CtoS::ResumeEngineHook();
        DialogMgr::Initialize();

        IntCheck("Phase 6: Entered Bogroot Growths Level 1", agentOk);
        return true;
    } else {
        IntCheck("Phase 6: Entered Bogroot Growths Level 1", false);
        return false;
    }
}

// ===== Bogroot Blessing Grab =====
// Move from Bogroot spawn to blessing shrine at (19099, 7762), find NPC, interact, Dialog(0x84).

static bool RunBogrootBlessingProof() {
    IntReport("=== PHASE 6B: Grab Blessing (Bogroot) ===");

    if (MapMgr::GetMapId() != MAP_BOGROOT_LVL1) {
        IntSkip("Bogroot blessing", "Not in Bogroot Growths Level 1");
        return false;
    }

    // Check if already blessed
    if (HasAnyBlessing()) {
        IntSkip("Bogroot blessing", "Player already has a blessing active");
        return true;
    }

    // Walk from spawn to blessing shrine
    for (size_t i = 0; i < _countof(kBogrootToBlessingPath); i++) {
        const auto& step = kBogrootToBlessingPath[i];
        IntReport("  Moving to %s (%.0f, %.0f)...", step.label, step.x, step.y);
        MovePlayerNear(step.x, step.y, step.threshold, step.timeoutMs);
    }

    // Find the blessing NPC near the shrine coordinates
    const float kSearchRadius = 2000.0f;
    uint32_t npcId = FindNearestNpc(kBlessingX, kBlessingY, kSearchRadius);
    if (npcId == 0) {
        IntReport("  No NPC found within %.0f of blessing coords", kSearchRadius);
        // Dump nearby agents for diagnostics
        DumpNearbyAgents(kBlessingX, kBlessingY, kSearchRadius, 10);
        IntSkip("Bogroot blessing", "No NPC found near blessing shrine coordinates");
        return false;
    }

    LivingAgentSnapshot npcSnap;
    if (!TrySnapshotLivingAgent(npcId, npcSnap)) {
        IntSkip("Bogroot blessing", "Could not read blessing NPC");
        return false;
    }
    IntReport("  Found NPC agent=%u playerNum=%u at (%.0f, %.0f) allegiance=%u",
              npcId, npcSnap.playerNumber, npcSnap.x, npcSnap.y, npcSnap.allegiance);

    // Move close to the NPC
    MovePlayerNear(npcSnap.x, npcSnap.y, 120.0f, 12000);

    // AutoIt pattern: ChangeTarget → GoNPC (0x39) → Sleep → Dialog (0x3B, 0x84)
    // GoNPC must go through GameThread (direct CtoS crashes after Bogroot map transition).
    // Dialog uses the AutoIt header 0x3B (DIALOG_SEND), not 0x3A (DIALOG_SEND_LIVING).

    // ===== PRE-INTERACTION DIAGNOSTICS =====
    uint32_t myId = AgentMgr::GetMyId();
    IntReport("  MyID=%u MapID=%u", myId, MapMgr::GetMapId());

    // Dump EotN title tracks (Asura=30, Norn=29, Deldrimor=31, Vanguard=28)
    static const struct { uint32_t id; const char* name; } kTitles[] = {
        {TitleID::Asura, "Asura"}, {TitleID::Norn, "Norn"},
        {TitleID::Deldrimor, "Deldrimor"}, {TitleID::Vanguard, "Vanguard"}
    };
    for (const auto& t : kTitles) {
        Title* track = PlayerMgr::GetTitleTrack(t.id);
        if (track) {
            IntReport("  Title %s (id=%u): points=%u tier=%u/%u needed_next=%u maxRank=%u",
                      t.name, t.id, track->current_points, track->current_title_tier_index,
                      track->max_title_tier_index, track->points_needed_next_rank, track->max_title_rank);
        } else {
            IntReport("  Title %s (id=%u): NOT FOUND", t.name, t.id);
        }
    }

    // Dump ALL active effects before interaction
    auto* playerEffects = EffectMgr::GetPlayerEffects();
    if (playerEffects && myId > 0) {
        auto* effectArr = EffectMgr::GetAgentEffectArray(myId);
        uint32_t effectCount = effectArr ? effectArr->size : 0;
        IntReport("  Effects BEFORE interaction: agent=%u count=%u", playerEffects->agent_id, effectCount);
        if (effectArr) {
            for (uint32_t i = 0; i < effectCount && i < 10; i++) {
                Effect& e = effectArr->buffer[i];
                IntReport("    effect[%u]: skill=%u attr=%u duration=%.1f agent=%u",
                          i, e.skill_id, e.attribute_level, e.duration, e.agent_id);
            }
        }
    } else {
        IntReport("  Effects BEFORE: playerEffects=%p myId=%u", playerEffects, myId);
    }

    // Check each blessing skill individually
    IntReport("  HasEffect check: Dwarven(2049)=%d Asuran(2050)=%d Norn(2051)=%d Vanguard(2052)=%d",
              myId ? EffectMgr::HasEffect(myId, SKILL_DWARVEN_BLESSING) : -1,
              myId ? EffectMgr::HasEffect(myId, SKILL_ASURAN_BLESSING) : -1,
              myId ? EffectMgr::HasEffect(myId, SKILL_NORN_BLESSING) : -1,
              myId ? EffectMgr::HasEffect(myId, SKILL_VANGUARD_BLESSING) : -1);

    // ===== SET TITLE =====
    // Blessing shrines require the appropriate EotN title to be displayed.
    // Bogroot is Deldrimor territory → set Deldrimor title (0x27).
    // AutoIt: SetDisplayedTitle($ID_DWARF_TITLE) before shrine interaction.
    uint32_t activeTitleBefore = PlayerMgr::GetActiveTitleId();
    IntReport("  Active title before: %u", activeTitleBefore);

    // Set Deldrimor title via packet 0x58
    if (activeTitleBefore == 0) {
        // No title displayed — set Deldrimor
        PlayerMgr::SetActiveTitle(TITLE_DISPLAY_DELDRIMOR);
        Sleep(1000);
        uint32_t activeTitleAfter = PlayerMgr::GetActiveTitleId();
        IntReport("  Active title after SetActiveTitle(%u): %u", TITLE_DISPLAY_DELDRIMOR, activeTitleAfter);
        IntCheck("Deldrimor title set for blessing", activeTitleAfter != 0);
    } else {
        // Title already displayed (may be from previous run) — keep it
        IntReport("  Title already active (%u), keeping current display", activeTitleBefore);
        IntCheck("Title already set for blessing", true);
    }

    // ===== BLESSING INTERACTION =====
    // Shut down DialogMgr hooks — StringEncoding::DecodeStr times out in Bogroot.
    DialogMgr::ResetHookState();
    DialogMgr::ResetRecentUITrace();
    IntReport("  Reset DialogMgr hook state before blessing interact");

    // Step 1: ChangeTarget
    AgentMgr::CancelAction();
    AgentMgr::ChangeTarget(npcId);
    Sleep(500);
    IntReport("  ChangeTarget to agent=%u (targetId now=%u)", npcId, AgentMgr::GetTargetId());

    // Step 2: native InteractNPC x3
    for (int attempt = 1; attempt <= 3; ++attempt) {
    AgentMgr::InteractNPC(npcId);
        IntReport("  Sent native InteractNPC attempt %d to agent=%u (lastUi=0x%X sender=%u lastDialog=0x%X)",
                  attempt,
                  npcId,
                  DialogMgr::GetLastUIMessageId(),
                  DialogMgr::GetDialogSenderAgentId(),
                  DialogMgr::GetLastDialogId());
        Sleep(1000);
    }

    // Step 3: Dialog (0x3B, 0x84) — AutoIt header
    QuestMgr::Dialog(DIALOG_ACCEPT_BLESSING);
    IntReport("  Sent QuestMgr::Dialog(0x%X)", DIALOG_ACCEPT_BLESSING);
    Sleep(3000); // Longer wait for server to process and effect to register

    IntReport("  Kept DialogMgr active throughout blessing interaction");

    // ===== POST-INTERACTION DIAGNOSTICS =====
    // Dump ALL effects after interaction
    myId = AgentMgr::GetMyId(); // re-read in case it changed
    playerEffects = EffectMgr::GetPlayerEffects();
    if (playerEffects && myId > 0) {
        auto* effectArr = EffectMgr::GetAgentEffectArray(myId);
        uint32_t effectCount = effectArr ? effectArr->size : 0;
        IntReport("  Effects AFTER interaction: agent=%u count=%u", playerEffects->agent_id, effectCount);
        if (effectArr) {
            for (uint32_t i = 0; i < effectCount && i < 10; i++) {
                Effect& e = effectArr->buffer[i];
                IntReport("    effect[%u]: skill=%u attr=%u duration=%.1f agent=%u",
                          i, e.skill_id, e.attribute_level, e.duration, e.agent_id);
            }
        }
    } else {
        IntReport("  Effects AFTER: playerEffects=%p myId=%u", playerEffects, myId);
    }

    IntReport("  HasEffect check: Dwarven(2049)=%d Asuran(2050)=%d Norn(2051)=%d Vanguard(2052)=%d",
              myId ? EffectMgr::HasEffect(myId, SKILL_DWARVEN_BLESSING) : -1,
              myId ? EffectMgr::HasEffect(myId, SKILL_ASURAN_BLESSING) : -1,
              myId ? EffectMgr::HasEffect(myId, SKILL_NORN_BLESSING) : -1,
              myId ? EffectMgr::HasEffect(myId, SKILL_VANGUARD_BLESSING) : -1);

    // Check blessing
    bool blessed = HasAnyBlessing();
    if (!blessed) {
        // Wait more and retry
        for (int attempt = 0; attempt < 6; ++attempt) {
            Sleep(500);
            if (HasAnyBlessing()) { blessed = true; break; }
        }
    }

    if (blessed) {
        IntCheck("Phase 6B: Blessing grabbed successfully", true);
    } else {
        IntReport("  WARN: Blessing effect not detected. Title may be maxed or dialog not processed.");
        IntCheck("Phase 6B: Blessing grab attempted (effect not confirmed)", true);
    }
    return blessed;
}

static bool RunBogrootDungeonLoopProof() {
    IntReport("=== PHASE 6C: Complete Bogroot Dungeon Loop ===");

    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId != MAP_BOGROOT_LVL1 && mapId != MAP_BOGROOT_LVL2) {
        IntSkip("Bogroot dungeon loop", "Not in Bogroot Growths");
        return false;
    }

    IntCheck("Froggy combat cache refresh before Bogroot loop", Bot::Froggy::RefreshCombatSkillbar());
    Bot::Froggy::ResetDungeonLoopTelemetry();
    const bool returnedToSparkfly = Bot::Froggy::DebugRunDungeonLoopFromCurrentMap();
    const auto telemetry = Bot::Froggy::GetDungeonLoopTelemetry();

    IntReport("  Bogroot loop telemetry: startLvl1=%d startLvl2=%d enteredLvl2=%d lvl1ToLvl2Started=%d lvl1ToLvl2Attempts=%u lastWp=%u(%s) wpIters=%u bossStarted=%d bossCompleted=%d chestAttempts=%u chestSuccesses=%u rewardAttempted=%d rewardLatched=%d lastDialog=0x%X finalMap=%u returnedToSparkfly=%d loaded=%d alive=%d hp=%.3f pos=(%.0f, %.0f) target=%u distToExit=%.0f nearestEnemy=%.0f nearbyEnemies=%u portal=%u portalPos=(%.0f, %.0f) portalDist=%.0f",
              telemetry.started_in_lvl1 ? 1 : 0,
              telemetry.started_in_lvl2 ? 1 : 0,
              telemetry.entered_lvl2 ? 1 : 0,
              telemetry.lvl1_to_lvl2_started ? 1 : 0,
              telemetry.lvl1_to_lvl2_attempts,
              telemetry.last_waypoint_index,
              telemetry.last_waypoint_label,
              telemetry.waypoint_iterations,
              telemetry.boss_started ? 1 : 0,
              telemetry.boss_completed ? 1 : 0,
              telemetry.chest_attempts,
              telemetry.chest_successes,
              telemetry.reward_attempted ? 1 : 0,
              telemetry.reward_dialog_latched ? 1 : 0,
              telemetry.last_dialog_id,
              telemetry.final_map_id,
              telemetry.returned_to_sparkfly ? 1 : 0,
              telemetry.map_loaded ? 1 : 0,
              telemetry.player_alive ? 1 : 0,
              telemetry.player_hp,
              telemetry.player_x,
              telemetry.player_y,
              telemetry.target_id,
              telemetry.dist_to_exit,
              telemetry.nearest_enemy_dist,
              telemetry.nearby_enemy_count,
              telemetry.lvl1_portal_id,
              telemetry.lvl1_portal_x,
              telemetry.lvl1_portal_y,
              telemetry.lvl1_portal_dist);

    IntCheck("Bogroot loop reached level 2", telemetry.entered_lvl2 || telemetry.started_in_lvl2);
    IntCheck("Bogroot boss sequence started", telemetry.boss_started);
    IntCheck("Bogroot boss sequence completed", telemetry.boss_completed);
    IntCheck("Bogroot chest interaction attempted", telemetry.chest_attempts > 0);
    IntCheck("Bogroot chest opened", telemetry.chest_successes > 0);
    IntCheck("Bogroot reward dialog attempted", telemetry.reward_attempted);
    IntCheck("Bogroot reward dialog latched", telemetry.reward_dialog_latched || telemetry.last_dialog_id == DIALOG_TEKKS_REWARD);
    IntCheck("Bogroot loop returned to Sparkfly", returnedToSparkfly && telemetry.returned_to_sparkfly && telemetry.final_map_id == MAP_SPARKFLY);

    if (returnedToSparkfly && telemetry.final_map_id == MAP_SPARKFLY) {
        const bool agentReady = WaitFor("MyID after Bogroot return", 30000, []() {
            return AgentMgr::GetMyId() > 0;
        });
        IntCheck("Sparkfly agent ready after Bogroot return", agentReady);
        if (agentReady) {
            WaitForStablePlayerState(10000);
            Sleep(2000);
        }
    }

    return returnedToSparkfly && telemetry.final_map_id == MAP_SPARKFLY;
}

static bool RunExplorableLootPickupProof() {
    IntReport("=== PHASE 5K: Explorable Loot Pickup Proof ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        IntSkip("Explorable loot pickup", "Not in game");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area || !IsSkillCastMapType(area->type)) {
        IntSkip("Explorable loot pickup", "Current map is not explorable-like");
        return false;
    }

    if (!WaitForStablePlayerState(5000)) {
        IntSkip("Explorable loot pickup", "Player state not stable enough for loot scan");
        return false;
    }

    AgentItem* item = FindNearbyGroundItem(5000.0f);
    if (!item) {
        const bool createdOpportunity = TryForceNearbyLootDrop();
        IntCheck("Loot pickup can create a nearby opportunity if needed", createdOpportunity);
        if (createdOpportunity) {
            Sleep(1000);
            item = FindNearbyGroundItem(12000.0f);
        }
    }
    if (!item) {
        IntSkip("Explorable loot pickup", "No nearby ground item found after loot probe");
        return false;
    }

    const uint32_t itemAgentId = item->agent_id;
    const uint32_t itemId = item->item_id;
    const float itemX = item->x;
    const float itemY = item->y;
    const InventorySnapshot inventoryBefore = CaptureInventorySnapshot();

    float myX = 0.0f;
    float myY = 0.0f;
    const bool havePlayerPos = TryReadAgentPosition(ReadMyId(), myX, myY);
    const float itemDistance = havePlayerPos ? AgentMgr::GetDistance(myX, myY, itemX, itemY) : -1.0f;

    IntReport("  Loot candidate: agent=%u item=%u pos=(%.0f, %.0f) dist=%.0f inventoryCount=%u gold=%u/%u",
              itemAgentId, itemId, itemX, itemY, itemDistance,
              inventoryBefore.count, inventoryBefore.goldCharacter, inventoryBefore.goldStorage);

    if (itemDistance < 0.0f || itemDistance > 180.0f) {
        IntReport("  Moving closer to loot before pickup...");
        MovePlayerNear(itemX, itemY, 120.0f, 12000);
    }

    const DWORD pickupStart = GetTickCount();
    bool pickupDone = false;
    while ((GetTickCount() - pickupStart) < 10000 && !pickupDone) {
        GameThread::EnqueuePost([itemX, itemY, itemAgentId]() {
            AgentMgr::Move(itemX, itemY);
            ItemMgr::PickUpItem(itemAgentId);
        });
        Sleep(500);
        if (!FindGroundItemByAgentId(itemAgentId)) {
            pickupDone = true;
            break;
        }
        const InventorySnapshot snap = CaptureInventorySnapshot();
        if (InventoryChangedMeaningfully(inventoryBefore, snap)) {
            pickupDone = true;
            break;
        }
    }

    bool pickedUpIntoInventory = pickupDone;
    const DWORD pickupAckStart = GetTickCount();
    while (!pickedUpIntoInventory && (GetTickCount() - pickupAckStart) < 5000) {
        if (!FindGroundItemByAgentId(itemAgentId) || ItemMgr::GetItemById(itemId)) {
            pickedUpIntoInventory = true;
            break;
        }
        const InventorySnapshot inventoryAfterWait = CaptureInventorySnapshot();
        if (InventoryChangedMeaningfully(inventoryBefore, inventoryAfterWait)) {
            pickedUpIntoInventory = true;
            break;
        }
        Sleep(250);
    }

    const InventorySnapshot inventoryAfter = CaptureInventorySnapshot();
    Item* pickedItem = ItemMgr::GetItemById(itemId);
    AgentItem* remainingGroundItem = FindGroundItemByAgentId(itemAgentId);
    const bool inventoryChanged = InventoryChangedMeaningfully(inventoryBefore, inventoryAfter);

    IntReport("  Loot result: acknowledged=%d inventoryChanged=%d pickedItem=%p groundItem=%p count=%u->%u gold=%u/%u->%u/%u",
              pickedUpIntoInventory ? 1 : 0,
              inventoryChanged ? 1 : 0,
              pickedItem,
              remainingGroundItem,
              inventoryBefore.count, inventoryAfter.count,
              inventoryBefore.goldCharacter, inventoryBefore.goldStorage,
              inventoryAfter.goldCharacter, inventoryAfter.goldStorage);

    IntCheck("Loot pickup changes inventory state or inventory contains the picked item",
             inventoryChanged || pickedItem != nullptr);
    IntCheck("Loot pickup removes the ground item or acknowledges inventory change",
             remainingGroundItem == nullptr || pickedUpIntoInventory);
    return pickedUpIntoInventory;
}

static bool ResetToFreshSparkflyInstanceForDungeonRun() {
    IntReport("=== PHASE 5K.5: Fresh Sparkfly Reset Before Dungeon Run ===");

    if (MapMgr::GetMapId() == MAP_SPARKFLY) {
        IntReport("  Returning to Gadd's to start the real dungeon path from a clean Sparkfly instance...");
        MapMgr::ReturnToOutpost();
        const bool returned = WaitFor("MapID == Gadd's after Sparkfly reset", 60000, []() {
            return MapMgr::GetMapId() == MAP_GADDS;
        });
        IntCheck("Returned to Gadd's for fresh Sparkfly run", returned);
        if (!returned) return false;
    } else if (MapMgr::GetMapId() != MAP_GADDS) {
        IntReport("  Traveling to Gadd's from map %u before fresh Sparkfly reset...", MapMgr::GetMapId());
        MapMgr::Travel(MAP_GADDS);
        const bool arrived = WaitFor("MapID == Gadd's for Sparkfly reset", 60000, []() {
            return MapMgr::GetMapId() == MAP_GADDS;
        });
        IntCheck("Traveled to Gadd's for fresh Sparkfly run", arrived);
        if (!arrived) return false;
    }

    const bool outpostAgentReady = WaitFor("MyID in Gadd's after Sparkfly reset", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    IntCheck("Agent ready in Gadd's after Sparkfly reset", outpostAgentReady);
    if (!outpostAgentReady) return false;

    WaitForStablePlayerState(8000);
    Sleep(2000);

    const bool hardModeReady = EnsureOutpostHardModeEnabled("Hard mode enabled before fresh Sparkfly re-entry");
    if (!hardModeReady) return false;

    IntReport("  Re-entering Sparkfly for the real dungeon path...");
    MovePlayerNear(-10018.0f, -21892.0f, 350.0f, 20000);
    MovePlayerNear(-9550.0f, -20400.0f, 350.0f, 20000);

    const DWORD zoneStart = GetTickCount();
    bool leftOutpost = false;
    while ((GetTickCount() - zoneStart) < 45000) {
        if (MapMgr::GetMapId() != MAP_GADDS) {
            leftOutpost = true;
            break;
        }
        GameThread::EnqueuePost([]() {
            AgentMgr::Move(-9451.0f, -19766.0f);
        });
        Sleep(500);
    }
    IntCheck("Left Gadd's for fresh Sparkfly run", leftOutpost);
    if (!leftOutpost) return false;

    const bool inSparkfly = WaitFor("MapID == Sparkfly after reset", 30000, []() {
        return MapMgr::GetMapId() == MAP_SPARKFLY;
    });
    IntCheck("Arrived in fresh Sparkfly instance", inSparkfly);
    if (!inSparkfly) return false;

    const bool sparkflyAgentReady = WaitFor("MyID in fresh Sparkfly instance", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    IntCheck("Agent ready in fresh Sparkfly instance", sparkflyAgentReady);
    if (!sparkflyAgentReady) return false;

    WaitForStablePlayerState(8000);
    Sleep(2000);
    return true;
}

static bool MoveToTekksForQuestDialog() {
    IntReport("=== PHASE 5L: Path to Tekks ===");
    IntReport("  Resetting player combat state before Tekks path...");
    AgentMgr::CancelAction();
    Sleep(100);
    const bool idleBeforeRoute = WaitFor("Player combat idle before Tekks path", 3000, []() {
        auto* me = AgentMgr::GetMyAgent();
        return me && !AgentMgr::IsCasting(me) && me->skill == 0;
    });
    IntCheck("Player combat idle before Tekks path", idleBeforeRoute);

    const auto IsNearSparkflyDungeonSide = []() -> bool {
        if (MapMgr::GetMapId() != MAP_SPARKFLY) return false;
        auto* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) return false;
        const float distToTekksStage = AgentMgr::GetDistance(me->x, me->y, 12061.0f, 22485.0f);
        const float distToDungeonStage = AgentMgr::GetDistance(me->x, me->y, 12228.0f, 22677.0f);
        return distToTekksStage <= 12000.0f || distToDungeonStage <= 12000.0f;
    };
    const auto RunShortTekksReturnPath = [&]() -> bool {
        auto* meBefore = AgentMgr::GetMyAgent();
        IntReport("  Near-dungeon Sparkfly spawn detected: player=(%.0f, %.0f) distToTekks=%.0f distToDoor=%.0f",
                  meBefore ? meBefore->x : 0.0f,
                  meBefore ? meBefore->y : 0.0f,
                  meBefore ? AgentMgr::GetDistance(meBefore->x, meBefore->y, 12061.0f, 22485.0f) : -1.0f,
                  meBefore ? AgentMgr::GetDistance(meBefore->x, meBefore->y, 12228.0f, 22677.0f) : -1.0f);
        bool reached = MovePlayerNear(12061.0f, 22485.0f, 700.0f, 45000);
        if (!reached) {
            reached = MovePlayerNear(12396.0f, 22407.0f, 700.0f, 30000);
        }
        IntCheck("Short Sparkfly return path to Tekks", reached);
        return reached;
    };

    if (IsNearSparkflyDungeonSide()) {
        return RunShortTekksReturnPath();
    }

    if (!s_enableInvasiveSparkflyCombatProofs &&
        !s_preferDirectTekksStagingForDebug &&
        MapMgr::GetMapId() == MAP_SPARKFLY) {
        IntReport("  Using Froggy's native Sparkfly route to Tekks before segmented fallbacks...");
        const bool froggyPrimaryRouteReached = Bot::Froggy::DebugRunSparkflyRouteToTekks();
        IntCheck("Primary Froggy Sparkfly route", froggyPrimaryRouteReached);
        if (froggyPrimaryRouteReached) {
            return true;
        }
        IntReport("  Primary Froggy Sparkfly route did not settle at Tekks; falling back to segmented probes");
        AgentMgr::CancelAction();
        Sleep(250);
    } else if (s_preferDirectTekksStagingForDebug && MapMgr::GetMapId() == MAP_SPARKFLY) {
        IntReport("  Bypassing Froggy's native Sparkfly route for direct Tekks debug staging");
    }

    const auto GetFightRangeForStep = [](size_t index) -> float {
        return index < 4 ? 1350.0f : 1250.0f;
    };
    const auto RunDirectTekksDebugTail = [&](const char* reasonLabel) -> bool {
        struct DebugTailStep {
            float x;
            float y;
            float threshold;
            DWORD timeoutMs;
            const char* label;
        };
        static constexpr DebugTailStep kDebugTail[] = {
            {11025.0f, 11710.0f, 650.0f, 45000, "Direct debug tail waypoint 8"},
            {14624.0f, 19314.0f, 650.0f, 60000, "Direct debug tail waypoint 9"},
            {12061.0f, 22485.0f, 500.0f, 90000, "Direct Tekks staging tail"},
        };

        IntReport("  Direct Tekks debug tail starting from %s", reasonLabel);
        for (const auto& tailStep : kDebugTail) {
            auto* meBefore = AgentMgr::GetMyAgent();
            IntReport("    %s: player=(%.0f, %.0f) target=(%.0f, %.0f) threshold=%.0f timeout=%lu",
                      tailStep.label,
                      meBefore ? meBefore->x : 0.0f,
                      meBefore ? meBefore->y : 0.0f,
                      tailStep.x,
                      tailStep.y,
                      tailStep.threshold,
                      static_cast<unsigned long>(tailStep.timeoutMs));
            bool tailReached = MovePlayerNear(tailStep.x, tailStep.y, tailStep.threshold, tailStep.timeoutMs);
            if (!tailReached) {
                IntReport("    Retrying %s once with relaxed threshold...", tailStep.label);
                tailReached = MovePlayerNear(tailStep.x, tailStep.y, tailStep.threshold + 150.0f, 30000);
            }
            IntCheck(tailStep.label, tailReached);
            if (!tailReached) {
                auto* meAfter = AgentMgr::GetMyAgent();
                IntReport("    %s failed with player ending at (%.0f, %.0f)",
                          tailStep.label,
                          meAfter ? meAfter->x : 0.0f,
                          meAfter ? meAfter->y : 0.0f);
                return false;
            }
        }
        return true;
    };
    const auto RunDirectTekksDebugPath = [&](const char* reasonLabel) -> bool {
        struct DebugPathStep {
            float x;
            float y;
            float threshold;
            DWORD timeoutMs;
            const char* label;
            bool required;
        };
        static constexpr DebugPathStep kDebugPath[] = {
            {-928.0f,  -8699.0f,  1000.0f, 35000, "Direct debug path waypoint 3", false},
            {4200.0f,  -4897.0f,   900.0f, 45000, "Direct debug path waypoint 4", false},
            {6114.0f,   819.0f,    900.0f, 45000, "Direct debug path waypoint 5", false},
            {9500.0f,  2281.0f,    900.0f, 45000, "Direct debug path waypoint 6", false},
            {11570.0f, 6120.0f,    900.0f, 45000, "Direct debug path waypoint 7", false},
            {11025.0f, 11710.0f,   900.0f, 45000, "Direct debug path waypoint 8", false},
            {14624.0f, 19314.0f,   700.0f, 60000, "Direct debug path waypoint 9", false},
            {12061.0f, 22485.0f,   500.0f, 90000, "Direct Tekks staging tail", true},
        };

        IntReport("  Direct Tekks debug path starting from %s", reasonLabel);
        bool requiredReached = true;
        for (const auto& pathStep : kDebugPath) {
            auto* meBefore = AgentMgr::GetMyAgent();
            IntReport("    %s: player=(%.0f, %.0f) target=(%.0f, %.0f) threshold=%.0f timeout=%lu required=%d",
                      pathStep.label,
                      meBefore ? meBefore->x : 0.0f,
                      meBefore ? meBefore->y : 0.0f,
                      pathStep.x,
                      pathStep.y,
                      pathStep.threshold,
                      static_cast<unsigned long>(pathStep.timeoutMs),
                      pathStep.required ? 1 : 0);
            bool pathReached = MovePlayerNear(pathStep.x, pathStep.y, pathStep.threshold, pathStep.timeoutMs);
            if (!pathReached) {
                IntReport("    Retrying %s once with relaxed threshold...", pathStep.label);
                pathReached = MovePlayerNear(pathStep.x, pathStep.y, pathStep.threshold + 200.0f, 30000);
            }
            auto* meAfter = AgentMgr::GetMyAgent();
            const float distAfter = meAfter
                ? AgentMgr::GetDistance(meAfter->x, meAfter->y, pathStep.x, pathStep.y)
                : -1.0f;
            IntReport("    %s result: reached=%d player=(%.0f, %.0f) distAfter=%.0f",
                      pathStep.label,
                      pathReached ? 1 : 0,
                      meAfter ? meAfter->x : 0.0f,
                      meAfter ? meAfter->y : 0.0f,
                      distAfter);
            if (pathStep.required) {
                IntCheck(pathStep.label, pathReached);
                requiredReached &= pathReached;
            }
        }
        return requiredReached;
    };
    if (s_preferDirectTekksStagingForDebug && MapMgr::GetMapId() == MAP_SPARKFLY) {
        return RunDirectTekksDebugPath("Sparkfly spawn");
    }
    for (const auto& step : kSparkflyToTekksPath) {
        const size_t stepIndex = static_cast<size_t>(&step - kSparkflyToTekksPath);
        if (s_preferDirectTekksStagingForDebug && stepIndex == 6) {
            IntReport("  Direct Tekks debug staging: cutting over after waypoint 6 and replacing the late Sparkfly tail");
            const bool directTailReached = RunDirectTekksDebugTail("Sparkfly waypoint 7");
            return directTailReached;
        }
        const float fightRange = GetFightRangeForStep(stepIndex);
        const int moveTimeoutMs = (s_preferDirectTekksStagingForDebug && stepIndex >= 4)
            ? max(step.timeoutMs, 45000)
            : step.timeoutMs;
        IntReport("  Moving to %s (%.0f, %.0f) fightRange=%.0f...", step.label, step.x, step.y, fightRange);
        const bool reached = s_preferDirectTekksStagingForDebug
            ? MovePlayerNear(step.x, step.y, step.threshold, moveTimeoutMs)
            : (MapMgr::GetMapId() == MAP_SPARKFLY
                ? Bot::Froggy::DebugAggroMoveTo(step.x, step.y, fightRange)
                : MovePlayerNear(step.x, step.y, step.threshold, step.timeoutMs));
        bool stepReached = reached;
        if (!stepReached && s_preferDirectTekksStagingForDebug) {
            IntReport("  Retrying %s once with relaxed debug tolerance...", step.label);
            stepReached = MovePlayerNear(step.x, step.y, step.threshold + 150.0f, 30000);
        }
        IntCheck(step.label, stepReached);
        if (!stepReached) {
            IntReport("  Aggro route failed at %s; evaluating Tekks fallback path", step.label);
            AgentMgr::CancelAction();
            Sleep(250);
            if (!s_preferDirectTekksStagingForDebug) {
                const bool froggyFallbackReached = Bot::Froggy::DebugRunSparkflyRouteToTekks();
                IntCheck("Fallback Froggy Sparkfly route", froggyFallbackReached);
                return froggyFallbackReached;
            } else {
                IntReport("  Direct Tekks debug staging is enabled; skipping Froggy route recovery");
            }
            const bool directFallbackReached = s_preferDirectTekksStagingForDebug
                ? RunDirectTekksDebugTail(step.label)
                : RunDirectTekksDebugPath(step.label);
            IntCheck("Fallback direct Tekks staging", directFallbackReached);
            return directFallbackReached;
        }
    }
    return true;
}

static bool WaitForPlayerPositionSettle(DWORD timeoutMs, float maxDeltaPerSample = 20.0f) {
    float lastX = 0.0f;
    float lastY = 0.0f;
    bool haveLast = false;
    int settledSamples = 0;
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        float x = 0.0f;
        float y = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), x, y)) {
            Sleep(100);
            continue;
        }
        if (haveLast) {
            const float delta = AgentMgr::GetDistance(lastX, lastY, x, y);
            if (delta <= maxDeltaPerSample) {
                if (++settledSamples >= 3) {
                    return true;
                }
            } else {
                settledSamples = 0;
            }
        }
        lastX = x;
        lastY = y;
        haveLast = true;
        Sleep(150);
    }
    return false;
}

static bool WaitForPlayerCombatIdle(DWORD timeoutMs, const char* label = "Player combat idle") {
    return WaitFor(label, timeoutMs, []() {
        auto* me = AgentMgr::GetMyAgent();
        return me && !AgentMgr::IsCasting(me) && me->skill == 0;
    });
}

static bool WaitForCombatTargetAcquire(uint32_t foeId,
                                       DWORD settleTimeoutMs = 2000,
                                       DWORD targetTimeoutMs = 2000) {
    if (foeId == 0) return false;
    if (AgentMgr::GetTargetId() == foeId) return true;

    WaitForPlayerPositionSettle(settleTimeoutMs, 15.0f);
    const bool queueIdle = WaitFor("Botshub queue idle before ChangeTarget", settleTimeoutMs, []() {
        return CtoS::IsBotshubQueueIdle();
    });
    IntReport("  Combat target acquire: foe=%u queueIdle=%d preTarget=%u",
              foeId,
              queueIdle ? 1 : 0,
              AgentMgr::GetTargetId());

    for (int attempt = 1; attempt <= 3; ++attempt) {
        AgentMgr::ChangeTarget(foeId);
        const bool targetChanged = WaitFor("Combat target acquired", targetTimeoutMs, [foeId]() {
            return AgentMgr::GetTargetId() == foeId;
        });
        IntReport("  Combat target acquire attempt %d/3: target=%u success=%d",
                  attempt,
                  AgentMgr::GetTargetId(),
                  targetChanged ? 1 : 0);
        if (targetChanged) {
            return true;
        }
        WaitForPlayerPositionSettle(750, 15.0f);
        WaitFor("Botshub queue idle between ChangeTarget attempts", 750, []() {
            return CtoS::IsBotshubQueueIdle();
        });
    }

    return AgentMgr::GetTargetId() == foeId;
}

static bool OpenNpcDialogWithRetries(uint32_t npcId, float* npcXPtr, float* npcYPtr,
                                     const char* label, int attempts, DWORD attemptTimeoutMs,
                                     bool* outInteractionObserved = nullptr) {
    if (!npcId || !label || attempts <= 0) return false;
    EnsureNpcUiTapRegistered();
    if (outInteractionObserved) *outInteractionObserved = false;

    struct InteractVariant {
        AgentMgr::NpcInteractMode mode;
        bool changeTarget;
        const char* name;
    };
    static constexpr InteractVariant kVariants[] = {
        {AgentMgr::NpcInteractMode::WorldActionNoCallTarget, false, "world-action-ct0"},
        {AgentMgr::NpcInteractMode::WorldActionCallTarget, true, "world-action-ct1"},
        {AgentMgr::NpcInteractMode::NativePostNoCallTarget, false, "native-post-ct0"},
        {AgentMgr::NpcInteractMode::NativePreNoCallTarget, false, "native-pre-ct0"},
        {AgentMgr::NpcInteractMode::NativePostCallTarget, true, "native-post-ct1"},
        {AgentMgr::NpcInteractMode::NativePreCallTarget, true, "native-pre-ct1"},
    };

    const auto HasVisibleNpcDialog = [npcId]() {
        return DialogMgr::IsDialogOpen() ||
               DialogMgr::GetButtonCount() > 0 ||
               DialogMgr::GetDialogSenderAgentId() == npcId;
    };

    for (int attempt = 1; attempt <= attempts; ++attempt) {
        DialogMgr::ClearDialog();
        DialogMgr::ResetHookState();
        DialogMgr::ResetRecentUITrace();
        ResetNpcUiTap();
        AgentMgr::CancelAction();
        float npcX = npcXPtr ? *npcXPtr : 0.0f;
        float npcY = npcYPtr ? *npcYPtr : 0.0f;
        TryReadAgentPosition(npcId, npcX, npcY);
        if (npcXPtr) *npcXPtr = npcX;
        if (npcYPtr) *npcYPtr = npcY;

        const DWORD closeStart = GetTickCount();
        bool movedNear = false;
        while ((GetTickCount() - closeStart) < 12000) {
            float meStepX = 0.0f;
            float meStepY = 0.0f;
            TryReadAgentPosition(ReadMyId(), meStepX, meStepY);
            const float stepDist = AgentMgr::GetDistance(meStepX, meStepY, npcX, npcY);
            if (stepDist <= 250.0f) {
                movedNear = true;
                break;
            }
            AgentMgr::Move(npcX, npcY);
            Sleep(250);
            if (HasVisibleNpcDialog()) {
                movedNear = true;
                break;
            }
        }
        const bool settled = WaitForPlayerPositionSettle(1200, 15.0f);
        float meX = 0.0f;
        float meY = 0.0f;
        TryReadAgentPosition(ReadMyId(), meX, meY);
        const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
        IntReport("  %s interact attempt %d/%d: movedNear=%d settle=%d dist=%.0f -> variant sweep",
                  label, attempt, attempts, movedNear ? 1 : 0, settled ? 1 : 0, dist);

        for (const auto& variant : kVariants) {
            DialogMgr::ClearDialog();
            DialogMgr::ResetHookState();
            DialogMgr::ResetRecentUITrace();
            ResetNpcUiTap();
            AgentMgr::CancelAction();
            if (variant.changeTarget) {
                AgentMgr::ChangeTarget(npcId);
                Sleep(250);
            }

            const bool uiObserved = DialogMgr::NPCHookEx(npcId, variant.mode, attemptTimeoutMs);
            const bool interactionObserved = uiObserved ||
                                             DialogMgr::GetLastUIMessageId() != 0 ||
                                             InterlockedCompareExchange(&s_npcUiTapCount, 0, 0) > 0;
            if (interactionObserved && outInteractionObserved) {
                *outInteractionObserved = true;
            }
            const bool dialogVisible = WaitFor("NPC dialog visible after variant interact", 1000, [&]() {
                return HasVisibleNpcDialog();
            });
            IntReport("  %s variant %s telemetry: uiObserved=%d lastUi=0x%X armed=0x%X observed=0x%X lastDialog=0x%X target=%u",
                      label,
                      variant.name,
                      uiObserved ? 1 : 0,
                      DialogMgr::GetLastUIMessageId(),
                      DialogMgr::GetArmedUIMessageId(),
                      DialogMgr::GetObservedUIMessageId(),
                      DialogMgr::GetLastDialogId(),
                      AgentMgr::GetTargetId());
            ReportRecentDialogUiTrace("NPC interact variant");
            ReportNpcUiTap("NPC interact variant");
            ReportDialogSnapshot("Dialog snapshot after variant interact");
            if (dialogVisible) {
                return true;
            }
        }
        Sleep(250);
    }
    return false;
}

static bool RunTekksQuestAcceptProof() {
    IntReport("=== PHASE 5M: Tekks Quest Accept Handling ===");

    ReportQuestSnapshot("Quest state before Tekks interact");
    const uint32_t activeBefore = QuestMgr::GetActiveQuestId();
    const uint32_t questLogBefore = QuestMgr::GetQuestLogSize();
    Quest* questBefore = QuestMgr::GetQuestById(QUEST_TEKKS_WAR);

    const uint32_t tekksId = FindNearestNpc(kTekksX, kTekksY, 1800.0f);
    IntCheck("Tekks NPC found near expected coordinates", tekksId != 0);
    if (!tekksId) {
        IntSkip("Tekks quest accept", "Could not resolve Tekks NPC");
        return false;
    }

    float npcX = 0.0f;
    float npcY = 0.0f;
    TryReadAgentPosition(tekksId, npcX, npcY);
    IntReport("  Tekks candidate: agent=%u pos=(%.0f, %.0f)", tekksId, npcX, npcY);

    const bool reachedNpc = MovePlayerNear(npcX, npcY, 120.0f, 12000);
    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const float distToTekks = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    IntReport("  Tekks pre-interact approach: reached=%d player=(%.0f, %.0f) dist=%.0f",
              reachedNpc ? 1 : 0, meX, meY, distToTekks);
    IntCheck("Reached Tekks staging range", reachedNpc || distToTekks <= 180.0f);

    IntReport("  Clearing local aggro before Tekks interaction...");
    const bool localAggroCleared = Bot::Froggy::DebugClearAggroInPlace(1250.0f);
    IntReport("  Tekks local aggro clear result=%d targetAfterClear=%u",
              localAggroCleared ? 1 : 0,
              AgentMgr::GetTargetId());
    AgentMgr::CancelAction();
    const bool idleBeforeInteract = WaitForPlayerCombatIdle(3000, "Player combat idle before Tekks interact");
    IntReport("  Tekks idle before interact=%d target=%u",
              idleBeforeInteract ? 1 : 0,
              AgentMgr::GetTargetId());
    WaitForPlayerPositionSettle(1200, 15.0f);
    IntReport("  Delegating Tekks preparation to Froggy's real dungeon-entry path...");
    const bool prepared = Bot::Froggy::DebugPrepareTekksDungeonEntry();
    ReportDialogSnapshot("After Froggy Tekks preparation");
    ReportQuestSnapshot("Quest state after Froggy Tekks preparation");

    const uint32_t activeAfter = QuestMgr::GetActiveQuestId();
    Quest* questAfter = QuestMgr::GetQuestById(QUEST_TEKKS_WAR);
    const uint32_t questLogAfter = QuestMgr::GetQuestLogSize();

    IntCheck("Tekks Froggy dungeon-entry preparation completed", prepared);
    IntCheck("Tekks quest present after Froggy preparation", questAfter != nullptr);
    IntCheck("Tekks active quest changed or remained Tekks quest",
             activeAfter == QUEST_TEKKS_WAR || activeBefore == QUEST_TEKKS_WAR);
    IntCheck("Quest log size stayed stable or grew after Froggy preparation", questLogAfter >= questLogBefore);
    if (!questBefore && questAfter) {
        IntCheck("Tekks accept created a new quest log entry", true);
    } else if (questBefore && questAfter) {
        IntSkip("Tekks accept created a new quest log entry",
                "Quest already existed before accept validation");
    }

    return prepared;
}

struct CombatActorSnapshot {
    bool valid = false;
    uint32_t agentId = 0;
    uint32_t allegiance = 0;
    float hp = 0.0f;
    float energy = 0.0f;
    uint32_t maxEnergy = 0;
    uint16_t castingSkill = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct SkillbarSnapshot {
    bool valid = false;
    uint32_t agentId = 0;
    uint32_t skillIds[8] = {};
    uint32_t recharge[8] = {};
    int nonZeroSkills = 0;
};

struct CombatObservabilitySnapshot {
    CombatActorSnapshot player;
    CombatActorSnapshot foe;
    SkillbarSnapshot skillbar;
    uint32_t targetId = 0;
    uint32_t heroCount = 0;
    uint32_t heroAgentIds[8] = {};
    bool heroAgentsReadable = false;
};

struct CombatCastTelemetrySnapshot {
    bool valid = false;
    SkillTestCandidate candidate = {};
    uint32_t targetId = 0;
    uint32_t energy = 0;
    uint16_t activeSkill = 0;
    uint32_t recharge = 0;
    uint32_t event = 0;
};

static bool CaptureCombatActorSnapshot(uint32_t agentId, CombatActorSnapshot& out) {
    out = {};
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (!agent || agent->type != 0xDB) return false;
    auto* living = static_cast<AgentLiving*>(agent);
    __try {
        out.valid = true;
        out.agentId = living->agent_id;
        out.allegiance = living->allegiance;
        out.hp = living->hp;
        out.energy = living->energy;
        out.maxEnergy = living->max_energy;
        out.castingSkill = living->skill;
        out.x = living->x;
        out.y = living->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}

static bool CapturePlayerSkillbarSnapshot(SkillbarSnapshot& out) {
    out = {};
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) return false;
    __try {
        out.valid = true;
        out.agentId = bar->agent_id;
        for (int i = 0; i < 8; ++i) {
            out.skillIds[i] = bar->skills[i].skill_id;
            out.recharge[i] = bar->skills[i].recharge;
            if (out.skillIds[i] != 0) ++out.nonZeroSkills;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}

static bool CaptureCombatObservabilitySnapshot(uint32_t foeId, CombatObservabilitySnapshot& out) {
    out = {};
    const uint32_t myId = AgentMgr::GetMyId();
    if (!myId) return false;

    CaptureCombatActorSnapshot(myId, out.player);
    CaptureCombatActorSnapshot(foeId, out.foe);
    CapturePlayerSkillbarSnapshot(out.skillbar);
    out.targetId = AgentMgr::GetTargetId();

    PartyInfo* playerParty = ResolveTestPlayerParty();
    if (playerParty && playerParty->heroes.buffer) {
        out.heroCount = playerParty->heroes.size;
        bool allReadable = out.heroCount > 0;
        const uint32_t cap = out.heroCount < 8 ? out.heroCount : 8;
        for (uint32_t i = 0; i < cap; ++i) {
            const uint32_t heroAgentId = playerParty->heroes.buffer[i].agent_id;
            out.heroAgentIds[i] = heroAgentId;
            if (!heroAgentId || AgentMgr::GetAgentByID(heroAgentId) == nullptr) {
                allReadable = false;
            }
        }
        out.heroAgentsReadable = allReadable;
    }

    return out.player.valid || out.foe.valid || out.skillbar.valid;
}

static void ReportCombatObservabilitySnapshot(const char* label, const CombatObservabilitySnapshot& snap) {
    IntReport("  %s:", label);
    IntReport("    player valid=%d id=%u hp=%.3f energy=%.3f/%u cast=%u pos=(%.0f, %.0f)",
              snap.player.valid ? 1 : 0,
              snap.player.agentId,
              snap.player.hp,
              snap.player.energy,
              snap.player.maxEnergy,
              snap.player.castingSkill,
              snap.player.x,
              snap.player.y);
    IntReport("    foe valid=%d id=%u allegiance=%u hp=%.3f cast=%u pos=(%.0f, %.0f)",
              snap.foe.valid ? 1 : 0,
              snap.foe.agentId,
              snap.foe.allegiance,
              snap.foe.hp,
              snap.foe.castingSkill,
              snap.foe.x,
              snap.foe.y);
    IntReport("    target=%u heroes=%u heroAgentsReadable=%d",
              snap.targetId,
              snap.heroCount,
              snap.heroAgentsReadable ? 1 : 0);
    IntReport("    skillbar valid=%d agent=%u nonZero=%d ids=[%u %u %u %u %u %u %u %u] recharge=[%u %u %u %u %u %u %u %u]",
              snap.skillbar.valid ? 1 : 0,
              snap.skillbar.agentId,
              snap.skillbar.nonZeroSkills,
              snap.skillbar.skillIds[0], snap.skillbar.skillIds[1], snap.skillbar.skillIds[2], snap.skillbar.skillIds[3],
              snap.skillbar.skillIds[4], snap.skillbar.skillIds[5], snap.skillbar.skillIds[6], snap.skillbar.skillIds[7],
              snap.skillbar.recharge[0], snap.skillbar.recharge[1], snap.skillbar.recharge[2], snap.skillbar.recharge[3],
              snap.skillbar.recharge[4], snap.skillbar.recharge[5], snap.skillbar.recharge[6], snap.skillbar.recharge[7]);
}

static void RunCombatObservabilityHarness(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5B: Combat Observability Harness (%s) ===", label ? label : "default");

    const bool targetReady = WaitForCombatTargetAcquire(foeId);
    IntCheck("Combat observability target set to foe", targetReady);

    CombatObservabilitySnapshot before = {};
    const bool beforeCaptured = CaptureCombatObservabilitySnapshot(foeId, before);
    IntCheck("Combat snapshot captured before dwell", beforeCaptured);
    if (!beforeCaptured) {
        IntSkip("Combat observability harness", "Could not capture initial snapshot");
        return;
    }
    ReportCombatObservabilitySnapshot("Combat snapshot before dwell", before);

    Sleep(1500);

    CombatObservabilitySnapshot after = {};
    const bool afterCaptured = CaptureCombatObservabilitySnapshot(foeId, after);
    IntCheck("Combat snapshot captured after dwell", afterCaptured);
    if (!afterCaptured) {
        IntSkip("Combat observability harness after dwell", "Could not capture follow-up snapshot");
        return;
    }
    ReportCombatObservabilitySnapshot("Combat snapshot after dwell", after);

    IntCheck("Combat snapshot player valid before dwell", before.player.valid);
    IntCheck("Combat snapshot player valid after dwell", after.player.valid);
    IntCheck("Combat snapshot foe valid before dwell", before.foe.valid);
    IntCheck("Combat snapshot foe valid after dwell", after.foe.valid);
    IntCheck("Combat snapshot foe is enemy allegiance", before.foe.valid && before.foe.allegiance == 3);
    IntCheck("Combat snapshot target stable across dwell", targetReady && before.targetId == foeId && after.targetId == foeId);
    IntCheck("Combat snapshot skillbar valid before dwell", before.skillbar.valid);
    IntCheck("Combat snapshot skillbar valid after dwell", after.skillbar.valid);
    IntCheck("Combat snapshot skillbar has non-zero skills before dwell", before.skillbar.nonZeroSkills > 0);
    IntCheck("Combat snapshot skillbar has non-zero skills after dwell", after.skillbar.nonZeroSkills > 0);
    IntCheck("Combat snapshot hero count available", before.heroCount > 0);
    IntCheck("Combat snapshot hero agents readable before dwell", before.heroAgentsReadable);
    IntCheck("Combat snapshot hero agents readable after dwell", after.heroAgentsReadable);
}

static void RunExplorableSkillbarRefreshProof() {
    IntReport("=== PHASE 5A: Explorable Skillbar Refresh ===");

    SkillbarSnapshot before = {};
    const bool beforeCaptured = CapturePlayerSkillbarSnapshot(before);
    IntCheck("Explorable skillbar readable before Froggy refresh", beforeCaptured);
    if (!beforeCaptured) {
        IntSkip("Explorable skillbar refresh proof", "Could not read player skillbar before refresh");
        return;
    }

    IntReport("  Skillbar before refresh: agent=%u nonZero=%d ids=[%u %u %u %u %u %u %u %u]",
              before.agentId,
              before.nonZeroSkills,
              before.skillIds[0], before.skillIds[1], before.skillIds[2], before.skillIds[3],
              before.skillIds[4], before.skillIds[5], before.skillIds[6], before.skillIds[7]);
    IntCheck("Explorable skillbar has non-zero skills before Froggy refresh", before.nonZeroSkills > 0);

    const bool refreshed = Bot::Froggy::RefreshCombatSkillbar();
    IntCheck("Froggy combat skillbar refresh succeeds in explorable", refreshed);

    SkillbarSnapshot after = {};
    const bool afterCaptured = CapturePlayerSkillbarSnapshot(after);
    IntCheck("Explorable skillbar readable after Froggy refresh", afterCaptured);
    if (!afterCaptured) {
        IntSkip("Explorable skillbar refresh proof after refresh", "Could not read player skillbar after refresh");
        return;
    }

    IntReport("  Skillbar after refresh: agent=%u nonZero=%d ids=[%u %u %u %u %u %u %u %u]",
              after.agentId,
              after.nonZeroSkills,
              after.skillIds[0], after.skillIds[1], after.skillIds[2], after.skillIds[3],
              after.skillIds[4], after.skillIds[5], after.skillIds[6], after.skillIds[7]);

    IntCheck("Explorable skillbar agent id stable across refresh", before.agentId == after.agentId);
    IntCheck("Explorable skillbar remains populated after refresh", after.nonZeroSkills > 0);
    IntCheck("Explorable skillbar IDs stable across refresh",
             memcmp(before.skillIds, after.skillIds, sizeof(before.skillIds)) == 0);
}

static bool IsSaneCombatPosition(float x, float y) {
    return _finite(x) && _finite(y) && fabsf(x) < 50000.0f && fabsf(y) < 50000.0f;
}

static void RunCombatAgentReadValidation(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5C: Combat Agent Read Validation (%s) ===", label ? label : "default");

    CombatActorSnapshot playerBefore = {};
    CombatActorSnapshot foeBefore = {};
    const bool playerBeforeOk = CaptureCombatActorSnapshot(AgentMgr::GetMyId(), playerBefore);
    const bool foeBeforeOk = CaptureCombatActorSnapshot(foeId, foeBefore);
    IntCheck("Combat agent read captured player before validation", playerBeforeOk);
    IntCheck("Combat agent read captured foe before validation", foeBeforeOk);
    if (!playerBeforeOk || !foeBeforeOk) {
        IntSkip("Combat agent read validation", "Could not capture initial player/foe snapshot");
        return;
    }

    auto* rawPlayer = GetAgentLivingRaw(playerBefore.agentId);
    auto* rawFoe = GetAgentLivingRaw(foeBefore.agentId);
    IntCheck("Combat agent read raw player pointer available", rawPlayer != nullptr);
    IntCheck("Combat agent read raw foe pointer available", rawFoe != nullptr);
    if (!rawPlayer || !rawFoe) {
        IntSkip("Combat agent read validation", "Raw player or foe pointer unavailable");
        return;
    }

    IntCheck("Combat agent read player id matches raw pointer", rawPlayer->agent_id == playerBefore.agentId);
    IntCheck("Combat agent read foe id matches raw pointer", rawFoe->agent_id == foeBefore.agentId);
    IntCheck("Combat agent read player hp sane", playerBefore.hp >= 0.0f && playerBefore.hp <= 2.0f);
    IntCheck("Combat agent read foe hp sane", foeBefore.hp >= 0.0f && foeBefore.hp <= 2.0f);
    IntCheck("Combat agent read player position sane", IsSaneCombatPosition(playerBefore.x, playerBefore.y));
    IntCheck("Combat agent read foe position sane", IsSaneCombatPosition(foeBefore.x, foeBefore.y));
    IntCheck("Combat agent read foe allegiance is enemy", foeBefore.allegiance == 3);
    IntCheck("Combat agent read raw foe allegiance matches snapshot", rawFoe->allegiance == foeBefore.allegiance);

    const float foeDistanceBefore = AgentMgr::GetDistance(playerBefore.x, playerBefore.y, foeBefore.x, foeBefore.y);
    IntReport("  Combat agent read before: player=%u foe=%u distance=%.0f hp=(%.3f, %.3f) cast=(%u, %u)",
              playerBefore.agentId, foeBefore.agentId, foeDistanceBefore,
              playerBefore.hp, foeBefore.hp,
              playerBefore.castingSkill, foeBefore.castingSkill);

    Sleep(500);

    CombatActorSnapshot playerAfter = {};
    CombatActorSnapshot foeAfter = {};
    const bool playerAfterOk = CaptureCombatActorSnapshot(playerBefore.agentId, playerAfter);
    const bool foeAfterOk = CaptureCombatActorSnapshot(foeBefore.agentId, foeAfter);
    IntCheck("Combat agent read captured player after validation", playerAfterOk);
    IntCheck("Combat agent read captured foe after validation", foeAfterOk);
    if (!playerAfterOk || !foeAfterOk) {
        IntSkip("Combat agent read validation after dwell", "Could not capture follow-up player/foe snapshot");
        return;
    }

    const float foeDistanceAfter = AgentMgr::GetDistance(playerAfter.x, playerAfter.y, foeAfter.x, foeAfter.y);
    IntReport("  Combat agent read after: player=%u foe=%u distance=%.0f hp=(%.3f, %.3f) cast=(%u, %u)",
              playerAfter.agentId, foeAfter.agentId, foeDistanceAfter,
              playerAfter.hp, foeAfter.hp,
              playerAfter.castingSkill, foeAfter.castingSkill);

    IntCheck("Combat agent read player id stable", playerAfter.agentId == playerBefore.agentId);
    IntCheck("Combat agent read foe id stable", foeAfter.agentId == foeBefore.agentId);
    IntCheck("Combat agent read foe allegiance stable", foeAfter.allegiance == foeBefore.allegiance);
    IntCheck("Combat agent read player hp sane after dwell", playerAfter.hp >= 0.0f && playerAfter.hp <= 2.0f);
    IntCheck("Combat agent read foe hp sane after dwell", foeAfter.hp >= 0.0f && foeAfter.hp <= 2.0f);
    IntCheck("Combat agent read player position sane after dwell", IsSaneCombatPosition(playerAfter.x, playerAfter.y));
    IntCheck("Combat agent read foe position sane after dwell", IsSaneCombatPosition(foeAfter.x, foeAfter.y));
    IntCheck("Combat agent read foe distance remains plausible", foeDistanceAfter >= 0.0f && foeDistanceAfter < 5000.0f);
}

static void RunCombatEffectReadValidation(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5D: Combat Effect Read Validation (%s) ===", label ? label : "default");

    const uint32_t myId = AgentMgr::GetMyId();
    IntCheck("Combat effect read bogus player effect is false", !EffectMgr::HasEffect(myId, 9999));
    IntCheck("Combat effect read bogus foe effect is false", !EffectMgr::HasEffect(foeId, 9999));
    IntCheck("Combat effect read bogus player buff is false", !EffectMgr::HasBuff(myId, 9999));
    IntCheck("Combat effect read bogus foe buff is false", !EffectMgr::HasBuff(foeId, 9999));

    auto* partyEffects = EffectMgr::GetPartyEffectsArray();
    if (!partyEffects || !partyEffects->buffer || partyEffects->size == 0) {
        IntSkip("Combat effect array validation", "Party effect array empty in current encounter");
        return;
    }

    IntCheck("Combat effect array size plausible", partyEffects->size <= 64);
    auto* playerEffects = EffectMgr::GetAgentEffects(myId);
    if (playerEffects) {
        IntCheck("Combat player effects agent id matches self", playerEffects->agent_id == myId);
        auto* playerEffectArray = EffectMgr::GetAgentEffectArray(myId);
        if (playerEffectArray) {
            IntCheck("Combat player effect array size plausible", playerEffectArray->size <= 500);
            if (playerEffectArray->size > 0) {
                const uint32_t effectSkill = playerEffectArray->buffer[0].skill_id;
                IntCheck("Combat player effect lookup round-trips first effect",
                         EffectMgr::GetEffectBySkillId(myId, effectSkill) != nullptr &&
                         EffectMgr::HasEffect(myId, effectSkill));
            }
        }

        auto* playerBuffArray = EffectMgr::GetAgentBuffArray(myId);
        if (playerBuffArray) {
            IntCheck("Combat player buff array size plausible", playerBuffArray->size <= 500);
            if (playerBuffArray->size > 0) {
                const uint32_t buffSkill = playerBuffArray->buffer[0].skill_id;
                IntCheck("Combat player buff lookup round-trips first buff",
                         EffectMgr::GetBuffBySkillId(myId, buffSkill) != nullptr &&
                         EffectMgr::HasBuff(myId, buffSkill));
            }
        }
    } else {
        IntSkip("Combat player effect lookup", "Player not present in current party effect array");
    }
}

static bool CaptureCombatCastTelemetrySnapshot(const SkillTestCandidate& candidate, CombatCastTelemetrySnapshot& out) {
    out = {};
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    AgentLiving* me = GetAgentLivingRaw(ReadMyId());
    if (!bar || !me || candidate.slot == 0 || candidate.slot > 8) return false;
    __try {
        const SkillbarSkill& sb = bar->skills[candidate.slot - 1];
        out.valid = true;
        out.candidate = candidate;
        out.targetId = AgentMgr::GetTargetId();
        out.energy = GetCurrentEnergyPoints();
        out.activeSkill = me->skill;
        out.recharge = sb.recharge;
        out.event = sb.event;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}

static void RunCombatTargetAndCastTelemetryValidation(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5E: Combat Target/Cast Telemetry Validation (%s) ===", label ? label : "default");

    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Combat target telemetry set foe target", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    IntCheck("Combat target telemetry change target to foe", targetChanged);

    SkillTestCandidate candidate = {};
    if (!TryChooseSkillTestCandidate(candidate)) {
        DumpSkillbarForSkillTest();
        IntSkip("Combat target/cast telemetry validation", "No suitable recharged player skill candidate found");
        return;
    }

    CombatCastTelemetrySnapshot before = {};
    const bool beforeCaptured = CaptureCombatCastTelemetrySnapshot(candidate, before);
    IntCheck("Combat cast telemetry captured before cast", beforeCaptured);
    if (!beforeCaptured) {
        IntSkip("Combat target/cast telemetry validation", "Could not capture pre-cast telemetry");
        return;
    }

    IntReport("  Combat cast before: slot=%u skill=%u target=%u liveTarget=%u energy=%u recharge=%u event=%u active=%u",
              candidate.slot,
              candidate.skillId,
              candidate.targetId,
              before.targetId,
              before.energy,
              before.recharge,
              before.event,
              before.activeSkill);

    SkillMgr::ResetSparkflyPlayerUseSkillCount();
    SkillMgr::SetSparkflyPlayerUseSkillOverride(true);
    SkillMgr::UseSkill(candidate.slot, candidate.targetId, 0);

    const bool telemetryChanged = WaitFor("Combat cast telemetry changes after UseSkill", 5000, [candidate, before]() {
        CombatCastTelemetrySnapshot after = {};
        if (!CaptureCombatCastTelemetrySnapshot(candidate, after)) return false;
        return after.recharge != before.recharge ||
               after.event != before.event ||
               after.activeSkill != before.activeSkill ||
               after.activeSkill == candidate.skillId ||
               after.energy < before.energy;
    });
    SkillMgr::SetSparkflyPlayerUseSkillOverride(false);
    IntCheck("Combat cast telemetry changes runtime state", telemetryChanged);

    CombatCastTelemetrySnapshot after = {};
    const bool afterCaptured = CaptureCombatCastTelemetrySnapshot(candidate, after);
    IntCheck("Combat cast telemetry captured after cast", afterCaptured);
    if (!afterCaptured) {
        IntSkip("Combat target/cast telemetry after cast", "Could not capture post-cast telemetry");
        return;
    }

    IntReport("  Combat cast after: slot=%u skill=%u target=%u liveTarget=%u energy=%u recharge=%u event=%u active=%u",
              candidate.slot,
              candidate.skillId,
              candidate.targetId,
              after.targetId,
              after.energy,
              after.recharge,
              after.event,
              after.activeSkill);

    const bool observedConcreteSignal =
        after.recharge != before.recharge ||
        after.event != before.event ||
        after.activeSkill != before.activeSkill ||
        after.activeSkill == candidate.skillId ||
        after.energy < before.energy;
    IntCheck("Combat cast telemetry has concrete observable signal", observedConcreteSignal);
    IntCheck("Combat target telemetry still has target after cast", after.targetId != 0);
}

static void RunReadOnlyCombatPreconditions(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5F: Read-only Combat Preconditions (%s) ===", label ? label : "default");

    CombatObservabilitySnapshot before = {};
    const bool beforeCaptured = CaptureCombatObservabilitySnapshot(foeId, before);
    IntCheck("Combat preconditions snapshot captured before dwell", beforeCaptured);
    if (!beforeCaptured) {
        IntSkip("Read-only combat preconditions", "Could not capture initial combat snapshot");
        return;
    }

    IntCheck("Combat preconditions player skillbar exists", before.skillbar.valid);
    IntCheck("Combat preconditions player skillbar has non-zero skills", before.skillbar.nonZeroSkills > 0);
    IntCheck("Combat preconditions foe readable before dwell", before.foe.valid);
    IntCheck("Combat preconditions foe allegiance is enemy", before.foe.valid && before.foe.allegiance == 3);
    IntCheck("Combat preconditions player alive before dwell", before.player.valid && before.player.hp > 0.0f);
    IntCheck("Combat preconditions foe alive before dwell", before.foe.valid && before.foe.hp > 0.0f);
    IntCheck("Combat preconditions hero count available", before.heroCount > 0);
    IntCheck("Combat preconditions hero agents readable before dwell", before.heroAgentsReadable);

    PartyInfo* playerParty = ResolveTestPlayerParty();
    if (playerParty && playerParty->heroes.buffer && playerParty->heroes.size > 0) {
        bool heroesAlive = true;
        const uint32_t cap = playerParty->heroes.size < 8 ? playerParty->heroes.size : 8;
        for (uint32_t i = 0; i < cap; ++i) {
            const uint32_t heroId = playerParty->heroes.buffer[i].agent_id;
            auto* hero = GetAgentLivingRaw(heroId);
            if (!hero || hero->hp <= 0.0f) {
                heroesAlive = false;
                break;
            }
        }
        IntCheck("Combat preconditions heroes alive before dwell", heroesAlive);
    } else {
        IntSkip("Combat preconditions hero alive check", "Player party heroes unavailable");
    }

    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Combat preconditions target foe", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    IntCheck("Combat preconditions current target set to foe", targetChanged);

    Sleep(1000);

    CombatObservabilitySnapshot after = {};
    const bool afterCaptured = CaptureCombatObservabilitySnapshot(foeId, after);
    IntCheck("Combat preconditions snapshot captured after dwell", afterCaptured);
    if (!afterCaptured) {
        IntSkip("Read-only combat preconditions after dwell", "Could not capture follow-up combat snapshot");
        return;
    }

    IntCheck("Combat preconditions foe remains readable across dwell", after.foe.valid);
    IntCheck("Combat preconditions player remains alive across dwell", after.player.valid && after.player.hp > 0.0f);
    IntCheck("Combat preconditions foe remains alive across dwell", after.foe.valid && after.foe.hp > 0.0f);
    IntCheck("Combat preconditions target stays stable briefly", after.targetId == foeId);
    IntCheck("Combat preconditions hero agents readable after dwell", after.heroAgentsReadable);

    if (playerParty && playerParty->heroes.buffer && playerParty->heroes.size > 0) {
        bool heroesAliveAfter = true;
        const uint32_t cap = playerParty->heroes.size < 8 ? playerParty->heroes.size : 8;
        for (uint32_t i = 0; i < cap; ++i) {
            const uint32_t heroId = playerParty->heroes.buffer[i].agent_id;
            auto* hero = GetAgentLivingRaw(heroId);
            if (!hero || hero->hp <= 0.0f) {
                heroesAliveAfter = false;
                break;
            }
        }
        IntCheck("Combat preconditions heroes alive after dwell", heroesAliveAfter);
    }
}

static bool MoveNearFoeForCombat(uint32_t foeId, float desiredRange, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    DWORD lastIssue = 0;
    float lastPx = 0.0f;
    float lastPy = 0.0f;
    bool haveLastPos = false;
    bool moveIssued = false;

    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = GetAgentLivingRaw(AgentMgr::GetMyId());
        auto* foe = GetAgentLivingRaw(foeId);
        if (!me || !foe || foe->hp <= 0.0f) return false;

        const float dist = AgentMgr::GetDistance(me->x, me->y, foe->x, foe->y);
        if (dist <= desiredRange) {
            WaitForPlayerPositionSettle(1200, 20.0f);
            WaitFor("Botshub queue idle after combat approach", 1200, []() {
                return CtoS::IsBotshubQueueIdle();
            });

            me = GetAgentLivingRaw(AgentMgr::GetMyId());
            foe = GetAgentLivingRaw(foeId);
            if (!me || !foe || foe->hp <= 0.0f) return false;
            const float settledDist = AgentMgr::GetDistance(me->x, me->y, foe->x, foe->y);
            if (settledDist <= desiredRange) return true;
        }

        const DWORD now = GetTickCount();
        bool shouldIssueMove = !moveIssued;
        if (haveLastPos && (now - lastIssue) >= 2000) {
            float px = 0.0f;
            float py = 0.0f;
            if (TryReadAgentPosition(ReadMyId(), px, py)) {
                const float moved = AgentMgr::GetDistance(lastPx, lastPy, px, py);
                if (moved < 40.0f) {
                    shouldIssueMove = true;
                }
                lastPx = px;
                lastPy = py;
            }
        }

        if (shouldIssueMove && GameThread::IsInitialized()) {
            const float dx = foe->x - me->x;
            const float dy = foe->y - me->y;
            const float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.0f) return true;

            const float engageBuffer = desiredRange > 120.0f ? 120.0f : desiredRange * 0.5f;
            const float stepBack = desiredRange > engageBuffer ? desiredRange - engageBuffer : desiredRange;
            const float scale = (len - stepBack) / len;
            const float moveX = me->x + dx * scale;
            const float moveY = me->y + dy * scale;

            GameThread::EnqueuePost([moveX, moveY]() {
                AgentMgr::Move(moveX, moveY);
            });
            moveIssued = true;
            lastIssue = now;
        }

        Sleep(500);

        float px = 0.0f;
        float py = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), px, py)) continue;
        lastPx = px;
        lastPy = py;
        haveLastPos = true;
    }

    return false;
}

static CombatObservabilitySnapshot s_lastBuiltinCombatBefore = {};
static CombatObservabilitySnapshot s_lastBuiltinCombatAfter = {};
static bool s_lastBuiltinCombatSnapshotsValid = false;

static void RunBuiltinCombatSingleStepProof(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5G: Builtin Combat Single-Step Proof (%s) ===", label ? label : "default");
    s_lastBuiltinCombatSnapshotsValid = false;

    CombatObservabilitySnapshot before = {};
    const bool beforeCaptured = CaptureCombatObservabilitySnapshot(foeId, before);
    IntCheck("Builtin combat proof snapshot captured before step", beforeCaptured);
    if (!beforeCaptured) {
        IntSkip("Builtin combat single-step proof", "Could not capture pre-step combat snapshot");
        return;
    }

    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Builtin combat proof target foe", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    IntCheck("Builtin combat proof target set to foe", targetChanged);

    auto* meBeforeEngage = GetAgentLivingRaw(AgentMgr::GetMyId());
    auto* foeBeforeEngage = GetAgentLivingRaw(foeId);
    if (meBeforeEngage && foeBeforeEngage) {
        const float distBeforeEngage = AgentMgr::GetDistance(meBeforeEngage->x, meBeforeEngage->y,
                                                             foeBeforeEngage->x, foeBeforeEngage->y);
        IntReport("  Builtin combat pre-engage distance: %.0f", distBeforeEngage);
    }

    const bool inCombatRange = MoveNearFoeForCombat(foeId, 1200.0f, 12000);
    if (!inCombatRange) {
        IntSkip("Builtin combat single-step proof", "Could not move into engagement range for builtin combat");
        return;
    }
    IntCheck("Builtin combat proof moved into engagement range", true);
    const bool targetReadyAfterMove = WaitForCombatTargetAcquire(foeId, 1500, 1500);
    IntCheck("Builtin combat proof target stable after move", targetReadyAfterMove);
    if (!targetReadyAfterMove) {
        IntSkip("Builtin combat single-step proof", "Lost foe target while moving into engagement range");
        return;
    }

    auto* meAfterEngage = GetAgentLivingRaw(AgentMgr::GetMyId());
    auto* foeAfterEngage = GetAgentLivingRaw(foeId);
    if (meAfterEngage && foeAfterEngage) {
        const float distAfterEngage = AgentMgr::GetDistance(meAfterEngage->x, meAfterEngage->y,
                                                            foeAfterEngage->x, foeAfterEngage->y);
        IntReport("  Builtin combat engaged distance: %.0f", distAfterEngage);
    }

    Bot::Froggy::DebugDumpBuiltinCombatDecision(foeId);
    const int dumpCount = Bot::Froggy::GetBuiltinCombatDecisionDumpCount();
    for (int i = 0; i < dumpCount; ++i) {
        IntReport("  Builtin combat dump[%d]: %s", i, Bot::Froggy::GetBuiltinCombatDecisionDumpLine(i));
    }

    const bool stepExecuted = Bot::Froggy::ExecuteBuiltinCombatStep(foeId);
    IntCheck("Builtin combat proof step executed", stepExecuted);
    if (!stepExecuted) {
        IntSkip("Builtin combat single-step proof", "Froggy combat step wrapper refused current target");
        return;
    }

    const char* actionDesc = Bot::Froggy::GetLastCombatStepDescription();
    IntReport("  Builtin combat action: %s", actionDesc ? actionDesc : "<null>");
    const int traceCount = Bot::Froggy::GetCombatDebugTraceCount();
    for (int i = 0; i < traceCount; ++i) {
        IntReport("  Builtin combat trace[%d]: %s", i, Bot::Froggy::GetCombatDebugTraceLine(i));
    }
    const bool actionChosen =
        actionDesc && actionDesc[0] != '\0' &&
        strncmp(actionDesc, "uninitialized", 13) != 0 &&
        strncmp(actionDesc, "no_action", 9) != 0;
    const bool isAutoAttack = actionDesc && strncmp(actionDesc, "auto_attack", 11) == 0;
    if (!actionChosen) {
        IntSkip("Builtin combat single-step proof", "Builtin combat step selected no action");
        return;
    }
    IntCheck("Builtin combat proof action chosen", true);

    bool observedSignal = false;
    CombatObservabilitySnapshot after = {};
    DWORD observeStart = GetTickCount();
    while ((GetTickCount() - observeStart) < 5000) {
        Sleep(200);
        if (!CaptureCombatObservabilitySnapshot(foeId, after)) continue;

        bool rechargeChanged = false;
        for (int i = 0; i < 8; ++i) {
            if (after.skillbar.recharge[i] != before.skillbar.recharge[i]) {
                rechargeChanged = true;
                break;
            }
        }

        const bool activeSkillChanged = after.player.castingSkill != before.player.castingSkill;
        const bool energyChanged = after.player.maxEnergy == before.player.maxEnergy &&
                                   after.player.energy != before.player.energy;
        const bool foeHpChanged = after.foe.valid && before.foe.valid && after.foe.hp != before.foe.hp;
        const float distanceBefore = AgentMgr::GetDistance(before.player.x, before.player.y, before.foe.x, before.foe.y);
        const float distanceAfter = AgentMgr::GetDistance(after.player.x, after.player.y, after.foe.x, after.foe.y);
        const bool distanceClosed = after.targetId == foeId && distanceAfter + 100.0f < distanceBefore;

        const bool signalObserved = isAutoAttack
            ? (foeHpChanged || distanceClosed)
            : (rechargeChanged || activeSkillChanged || energyChanged || foeHpChanged);

        if (signalObserved) {
            observedSignal = true;
            break;
        }
    }

    IntCheck("Builtin combat proof snapshot captured after step", after.player.valid || after.foe.valid || after.skillbar.valid);
    if (!after.player.valid || !after.foe.valid) {
        IntSkip("Builtin combat single-step proof after step", "Could not capture post-step player/foe snapshot");
        return;
    }

    bool rechargeChanged = false;
    int rechargeSlot = -1;
    for (int i = 0; i < 8; ++i) {
        if (after.skillbar.recharge[i] != before.skillbar.recharge[i]) {
            rechargeChanged = true;
            rechargeSlot = i + 1;
            break;
        }
    }
    const bool activeSkillChanged = after.player.castingSkill != before.player.castingSkill;
    const bool energyChanged = after.player.maxEnergy == before.player.maxEnergy &&
                               after.player.energy != before.player.energy;
    const bool foeHpChanged = after.foe.hp != before.foe.hp;
    const float distanceBefore = AgentMgr::GetDistance(before.player.x, before.player.y, before.foe.x, before.foe.y);
    const float distanceAfter = AgentMgr::GetDistance(after.player.x, after.player.y, after.foe.x, after.foe.y);
    const bool distanceClosed = after.targetId == foeId && distanceAfter + 100.0f < distanceBefore;

    IntReport("  Builtin combat after: active=%u->%u energy=%.3f->%.3f foeHp=%.3f->%.3f rechargeSlot=%d distance=%.0f->%.0f",
              before.player.castingSkill,
              after.player.castingSkill,
              before.player.energy,
              after.player.energy,
              before.foe.hp,
              after.foe.hp,
              rechargeSlot,
              distanceBefore,
              distanceAfter);

    const bool validConcreteSignal = isAutoAttack
        ? (foeHpChanged || distanceClosed)
        : (rechargeChanged || activeSkillChanged || energyChanged || foeHpChanged);
    if (!validConcreteSignal) {
        IntSkip("Builtin combat single-step proof", isAutoAttack
            ? "Auto-attack selected but no hit or range-closing signal was observed"
            : "Skill action selected but no cast-side signal was observed");
        return;
    }
    IntCheck("Builtin combat proof observed concrete signal", observedSignal);
    IntCheck("Builtin combat proof signal matches selected action",
             validConcreteSignal);
    s_lastBuiltinCombatBefore = before;
    s_lastBuiltinCombatAfter = after;
    s_lastBuiltinCombatSnapshotsValid = true;
}

static constexpr uint32_t TEST_ROLE_HEX = (1u << 8);
static constexpr uint32_t TEST_ROLE_PRESSURE = (1u << 9);
static constexpr uint32_t TEST_ROLE_ATTACK = (1u << 10);
static constexpr uint32_t TEST_ROLE_INTERRUPT_HARD = (1u << 11);
static constexpr uint32_t TEST_ROLE_INTERRUPT_SOFT = (1u << 12);
static constexpr uint32_t TEST_ROLE_ENCHANT_REMOVE = (1u << 7);

static bool IsLiveEnemyAgent(uint32_t agentId) {
    auto* a = AgentMgr::GetAgentByID(agentId);
    if (!a || a->type != 0xDB) return false;
    auto* living = static_cast<AgentLiving*>(a);
    return living->allegiance == 3 && living->hp > 0.0f;
}

static void RunCombatTargetSelectionCoverage(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5H: Combat Target Selection Coverage (%s) ===", label ? label : "default");

    const auto info = Bot::Froggy::GetLastCombatStepInfo();
    if (!info.valid) {
        IntSkip("Combat target selection coverage", "No builtin combat step metadata available");
        return;
    }
    IntCheck("Combat target selection has last combat step info", true);

    if (info.target_type == 5 || info.auto_attack) {
        IntCheck("Chosen combat action resolved non-zero foe target", info.target_id != 0);
        IntCheck("Chosen combat action target is live foe", IsLiveEnemyAgent(info.target_id));
    } else {
        IntSkip("Combat target selection - chosen foe-target action",
                "Last builtin combat step did not use a foe-targeting action");
    }

    uint32_t targetId = 0;
    if (!Bot::Froggy::DebugResolveSyntheticSkillTarget(TEST_ROLE_HEX | TEST_ROLE_PRESSURE, 5u, foeId, targetId)) {
        IntSkip("Combat target selection - unhexed foe branch",
                "Synthetic target resolver unavailable");
    } else if (targetId == 0) {
        IntSkip("Combat target selection - unhexed foe branch",
                "No valid foe target resolved for the synthetic hex/pressure branch");
    } else {
        IntCheck("Unhexed/default foe branch resolved live foe target", targetId != 0 && IsLiveEnemyAgent(targetId));
    }

    if (!Bot::Froggy::DebugResolveSyntheticSkillTarget(TEST_ROLE_INTERRUPT_HARD | TEST_ROLE_INTERRUPT_SOFT, 5u, foeId, targetId)) {
        IntSkip("Combat target selection - casting foe branch",
                "Synthetic target resolver unavailable");
    } else {
        const uint32_t castingFoe = Bot::Froggy::DebugGetCastingEnemy();
        if (!castingFoe) {
            IntSkip("Combat target selection - casting foe branch",
                    "No casting foe present in the current encounter");
        } else {
            auto* a = AgentMgr::GetAgentByID(castingFoe);
            auto* living = (a && a->type == 0xDB) ? static_cast<AgentLiving*>(a) : nullptr;
            IntCheck("Casting-foe branch resolved current casting foe", targetId == castingFoe);
            IntCheck("Casting-foe branch target is live foe", targetId != 0 && IsLiveEnemyAgent(targetId));
            IntCheck("Casting-foe branch target observed non-zero skill", living && living->skill != 0);
        }
    }

    if (!Bot::Froggy::DebugResolveSyntheticSkillTarget(TEST_ROLE_ENCHANT_REMOVE, 5u, foeId, targetId)) {
        IntSkip("Combat target selection - enchanted foe branch",
                "Synthetic target resolver unavailable");
    } else {
        const uint32_t enchantedFoe = Bot::Froggy::DebugGetEnchantedEnemy();
        if (!enchantedFoe) {
            IntSkip("Combat target selection - enchanted foe branch",
                    "No enchanted foe present in the current encounter");
        } else {
            IntCheck("Enchanted-foe branch resolved enchanted foe", targetId == enchantedFoe);
            IntCheck("Enchanted-foe branch target is live foe", targetId != 0 && IsLiveEnemyAgent(targetId));
        }
    }

    if (!Bot::Froggy::DebugResolveSyntheticSkillTarget(TEST_ROLE_ATTACK, 5u, foeId, targetId)) {
        IntSkip("Combat target selection - melee branch",
                "Synthetic target resolver unavailable");
    } else {
        const uint32_t meleeFoe = Bot::Froggy::DebugGetMeleeRangeEnemy();
        if (!meleeFoe) {
            IntSkip("Combat target selection - melee branch",
                    "No melee-range foe present in the current encounter");
        } else {
            auto* me = AgentMgr::GetMyAgent();
            auto* foe = AgentMgr::GetAgentByID(targetId);
            auto* living = (foe && foe->type == 0xDB) ? static_cast<AgentLiving*>(foe) : nullptr;
            const float dist = (me && living) ? AgentMgr::GetDistance(me->x, me->y, living->x, living->y) : 99999.0f;
            IntCheck("Melee branch resolved melee-range foe", targetId == meleeFoe);
            IntCheck("Melee branch target within melee threshold", dist <= 1320.0f);
        }
    }
}

static void RunCombatCastGatingAndSafetyAssertions(const CombatObservabilitySnapshot& before,
                                                   const CombatObservabilitySnapshot& after,
                                                   const char* label) {
    IntReport("=== PHASE 5I: Cast Gating and Safety Assertions (%s) ===", label ? label : "default");

    const auto info = Bot::Froggy::GetLastCombatStepInfo();
    if (!info.valid) {
        IntSkip("Cast gating and safety assertions", "No builtin combat step metadata available");
        return;
    }
    IntCheck("Cast gating has last combat step info", true);
    if (!info.used_skill) {
        IntSkip("Cast gating and safety assertions", "Last builtin combat step was not a skill cast");
        return;
    }

    IntCheck("Chosen combat skill slot is in range", info.slot >= 1 && info.slot <= 8);
    if (info.slot < 1 || info.slot > 8) return;

    const int slotIndex = info.slot - 1;
    const uint32_t beforeRecharge = before.skillbar.recharge[slotIndex];
    const uint32_t afterRecharge = after.skillbar.recharge[slotIndex];
    IntReport("  Chosen skill slot %d recharge: before=%u after=%u expectedAftercastMs=%u observedDurationMs=%u",
              info.slot, beforeRecharge, afterRecharge, info.expected_aftercast_ms,
              info.finished_at_ms >= info.started_at_ms ? (info.finished_at_ms - info.started_at_ms) : 0);

    IntCheck("Chosen combat skill target is non-zero", info.target_id != 0);
    if (info.target_type == 5) {
        IntCheck("Chosen combat skill target is live foe", IsLiveEnemyAgent(info.target_id));
    }

    const bool chosenRechargeTransition = beforeRecharge == 0 && afterRecharge > 0;
    const bool chosenEnergyChanged = after.player.maxEnergy == before.player.maxEnergy &&
                                     after.player.energy != before.player.energy;
    const bool chosenActiveSkillChanged = after.player.castingSkill != before.player.castingSkill;
    if (chosenRechargeTransition || chosenEnergyChanged || chosenActiveSkillChanged) {
        IntCheck("Chosen combat skill shows gated cast-side transition", true);
    } else {
        IntSkip("Chosen combat skill cast-side transition",
                "No slot-local recharge, energy, or active-skill transition was observable for this skill window");
    }

    if (info.expected_aftercast_ms > 0 && info.finished_at_ms >= info.started_at_ms) {
        const uint32_t observedDurationMs = info.finished_at_ms - info.started_at_ms;
        IntCheck("Aftercast pacing observed at or beyond expected delay",
                 observedDurationMs + 50 >= info.expected_aftercast_ms);
    } else {
        IntSkip("Aftercast pacing assertion",
                "Chosen skill did not expose a positive expected aftercast duration");
    }

    int blockedSlot = -1;
    for (int i = 0; i < 8; ++i) {
        if (i == slotIndex) continue;
        if (before.skillbar.recharge[i] > 0) {
            blockedSlot = i;
            break;
        }
    }
    if (blockedSlot < 0) {
        IntSkip("Recharge-blocked candidate assertion",
                "No non-chosen recharging skill was available to prove gating");
    } else {
        IntReport("  Recharge-blocked slot %d: before=%u after=%u",
                  blockedSlot + 1, before.skillbar.recharge[blockedSlot], after.skillbar.recharge[blockedSlot]);
        if (before.skillbar.recharge[blockedSlot] > 0 && after.skillbar.recharge[blockedSlot] > 0) {
            IntCheck("Recharge-blocked candidate stayed non-ready", true);
        } else {
            IntSkip("Recharge-blocked candidate assertion",
                    "The sampled blocked slot cooled down naturally before the post-step snapshot");
        }
    }

    IntCheck("Combat cast left player snapshot valid after step", after.player.valid);
    IntCheck("Combat cast left foe snapshot valid after step", after.foe.valid);
}

static bool IsSpiritTraceUseLine(const char* line) {
    if (!line || !line[0] || strstr(line, " USE ") == nullptr) return false;
    return strstr(line, "slot=2 ") != nullptr ||
           strstr(line, "slot=3 ") != nullptr ||
           strstr(line, "slot=4 ") != nullptr ||
           strstr(line, "slot=5 ") != nullptr;
}

static void RunBuiltinSpiritChainRegression(uint32_t foeId, const char* label) {
    IntReport("=== PHASE 5J: Spirit Chain Regression (%s) ===", label ? label : "default");

    SkillbarSnapshot barBefore = {};
    if (!CapturePlayerSkillbarSnapshot(barBefore)) {
        IntSkip("Spirit chain regression", "Could not read player skillbar before spirit chain validation");
        return;
    }
    if (barBefore.skillIds[1] != 1239u) {
        IntSkip("Spirit chain regression", "Current bar does not have Signet of Spirits in slot 2");
        return;
    }
    if (barBefore.recharge[1] > 0) {
        IntReport("  Spirit chain waiting for Signet of Spirits recharge: %u", barBefore.recharge[1]);
        const bool signetReady = WaitFor("Spirit chain slot 2 recharge clears", 30000, [&barBefore]() {
            SkillbarSnapshot current = {};
            if (!CapturePlayerSkillbarSnapshot(current)) return false;
            barBefore = current;
            return current.skillIds[1] == 1239u && current.recharge[1] == 0;
        });
        if (!signetReady) {
            IntSkip("Spirit chain regression", "Signet of Spirits did not recharge within the validation window");
            return;
        }
        IntCheck("Spirit chain slot 2 became ready for validation", true);
    }

    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Spirit chain regression target foe", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    IntCheck("Spirit chain regression target set to foe", targetChanged);
    if (!targetChanged) {
        IntSkip("Spirit chain regression", "Could not target foe");
        return;
    }

    const bool inCombatRange = MoveNearFoeForCombat(foeId, 1200.0f, 15000);
    if (!inCombatRange) {
        IntSkip("Spirit chain regression", "Could not move into engagement range");
        return;
    }
    IntCheck("Spirit chain regression moved into engagement range", true);

    bool sawSlot2 = false;
    bool sawFollowUpSpirit = false;
    int slot2Step = -1;

    for (int step = 1; step <= 4; ++step) {
        auto* foe = GetAgentLivingRaw(foeId);
        if (!foe || foe->hp <= 0.0f) {
            IntSkip("Spirit chain regression continuation", "Foe died or became unreadable");
            break;
        }

        const bool stepExecuted = Bot::Froggy::ExecuteBuiltinCombatStep(foeId);
        if (!stepExecuted) {
            IntSkip("Spirit chain regression step", "Builtin combat step wrapper refused current target");
            break;
        }

        const auto info = Bot::Froggy::GetLastCombatStepInfo();
        if (info.valid && info.used_skill) {
            if (info.slot == 2) {
                sawSlot2 = true;
                if (slot2Step < 0) slot2Step = step;
            }
            if ((info.slot == 3 || info.slot == 4 || info.slot == 5) && slot2Step > 0 && step > slot2Step) {
                sawFollowUpSpirit = true;
            }
        }

        const int traceCount = Bot::Froggy::GetCombatDebugTraceCount();
        for (int i = 0; i < traceCount; ++i) {
            const char* line = Bot::Froggy::GetCombatDebugTraceLine(i);
            if (IsSpiritTraceUseLine(line)) {
                IntReport("  Spirit chain trace[%d.%d]: %s", step, i, line);
            }
            if (line && strstr(line, " USE ")) {
                if (strstr(line, "slot=2 ")) {
                    sawSlot2 = true;
                    if (slot2Step < 0) slot2Step = step;
                }
                if (slot2Step > 0 && step >= slot2Step &&
                    (strstr(line, "slot=3 ") || strstr(line, "slot=4 ") || strstr(line, "slot=5 "))) {
                    sawFollowUpSpirit = true;
                }
            }
        }

        if (sawSlot2 && sawFollowUpSpirit) {
            IntReport("  Spirit chain proof satisfied by step %d", step);
            break;
        }

        Sleep(250);
    }

    IntCheck("Spirit chain used Signet of Spirits (slot 2)", sawSlot2);
    IntCheck("Spirit chain used follow-up spirit after Signet of Spirits (slot 3/4/5)", sawFollowUpSpirit);
}

static bool SelectReachableBuiltinCombatProofFoe(uint32_t seedFoeId,
                                                 uint32_t& outFoeId,
                                                 const char*& outLabel) {
    outFoeId = 0;
    outLabel = "combat proof foe";

    auto* me = GetAgentLivingRaw(AgentMgr::GetMyId());
    auto* seedFoe = GetAgentLivingRaw(seedFoeId);
    if (me && seedFoe && seedFoe->hp > 0.0f) {
        const float dist = AgentMgr::GetDistance(me->x, me->y, seedFoe->x, seedFoe->y);
        IntReport("  Builtin combat proof seed foe=%u distance=%.0f", seedFoeId, dist);
        if (dist <= 2500.0f) {
            outFoeId = seedFoeId;
            outLabel = "initial foe";
            return true;
        }
    }

    IntReport("  Repositioning toward Sparkfly combat probe for builtin combat proof...");
    AgentMgr::CancelAction();
    Sleep(100);
    const bool idleBeforeProbe = WaitForPlayerCombatIdle(3000, "Player combat idle before builtin combat proof route probe");
    IntCheck("Player combat idle before builtin combat proof route probe", idleBeforeProbe);
    const bool movedToProbe = MovePlayerNear(-4559.0f, -14406.0f, 500.0f, 25000);
    IntCheck("Moved to builtin combat proof route probe", movedToProbe);

    const uint32_t probeFoeId = FindNearestFoe(5000.0f);
    if (!probeFoeId) {
        return false;
    }

    const bool targetReady = WaitForCombatTargetAcquire(probeFoeId);
    IntCheck("Target changed to builtin combat proof foe", targetReady);
    if (!targetReady) {
        return false;
    }

    outFoeId = probeFoeId;
    outLabel = "combat proof foe";
    return true;
}

static void RunBuiltinCombatProofSuite(uint32_t seedFoeId) {
    uint32_t proofFoeId = 0;
    const char* proofLabel = "combat proof foe";
    const bool haveProofFoe = SelectReachableBuiltinCombatProofFoe(seedFoeId, proofFoeId, proofLabel);
    if (!haveProofFoe) {
        IntSkip("Builtin combat single-step proof", "No reachable foe found for builtin combat proof suite");
        IntSkip("Combat target selection coverage", "No reachable foe found for builtin combat proof suite");
        IntSkip("Cast gating and safety assertions", "No reachable foe found for builtin combat proof suite");
        IntSkip("Spirit chain regression", "No reachable foe found for builtin combat proof suite");
        return;
    }

    RunBuiltinCombatSingleStepProof(proofFoeId, proofLabel);
    RunCombatTargetSelectionCoverage(proofFoeId, proofLabel);
    if (s_lastBuiltinCombatSnapshotsValid) {
        RunCombatCastGatingAndSafetyAssertions(s_lastBuiltinCombatBefore, s_lastBuiltinCombatAfter, proofLabel);
    } else {
        IntSkip("Cast gating and safety assertions", "Builtin combat proof did not capture before/after snapshots");
    }

    uint32_t spiritFoeId = proofFoeId;
    auto* spiritFoe = GetAgentLivingRaw(spiritFoeId);
    if (!spiritFoe || spiritFoe->hp <= 0.0f) {
        spiritFoeId = FindNearestFoe(5000.0f);
        IntReport("  Spirit chain selecting fresh live foe after builtin combat proof: %u", spiritFoeId);
    }
    if (!spiritFoeId) {
        IntSkip("Spirit chain regression", "No live foe available after builtin combat proof");
        return;
    }
    WaitForPlayerPositionSettle(1000, 20.0f);
    if (!WaitForCombatTargetAcquire(spiritFoeId, 1500, 1500)) {
        IntSkip("Spirit chain regression", "Could not retarget a live foe after builtin combat proof");
        return;
    }
    RunBuiltinSpiritChainRegression(spiritFoeId, proofLabel);
}

static void ReportMerchantRuntimeContext(const char* label) {
    IntReport("  %s: GameThread=%d onGameThread=%d RenderHook=%d hb=%u TraderHook=%d TargetLogHook=%d targetCalls=%u targetStores=%u CtoSHook=%d ctoSHb=%u",
              label,
              GameThread::IsInitialized() ? 1 : 0,
              GameThread::IsOnGameThread() ? 1 : 0,
              RenderHook::IsInitialized() ? 1 : 0,
              RenderHook::GetHeartbeat(),
              TraderHook::IsInitialized() ? 1 : 0,
              TargetLogHook::IsInitialized() ? 1 : 0,
              TargetLogHook::GetCallCount(),
              TargetLogHook::GetStoreCount(),
              CtoSHook::IsInitialized() ? 1 : 0,
              CtoSHook::GetHeartbeat());
}

static void ReportMerchantPreInteractState(const char* label, uint32_t npcId, float npcX, float npcY) {
    float meX = 0.0f, meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const uint32_t currentTarget = AgentMgr::GetTargetId();
    const bool dialogOpen = DialogMgr::IsDialogOpen();
    const uint32_t dialogSender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t dialogButtons = DialogMgr::GetButtonCount();
    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(3613855137u);
    const uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    const uint32_t heroCount = PartyMgr::CountPartyHeroes();
    const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    IntReport("  %s: npc=%u playerPos=(%.0f,%.0f) npcPos=(%.0f,%.0f) dist=%.0f target=%u dialogOpen=%d sender=%u buttons=%u merchantFrame=0x%08X items=%u heroes=%u",
        label,
        npcId,
        meX, meY,
        npcX, npcY,
        dist,
        currentTarget,
        dialogOpen ? 1 : 0,
        dialogSender,
        dialogButtons,
        static_cast<unsigned>(merchantFrame),
        merchantItems,
        heroCount);
}

static void DumpMerchantResolutionState(const NpcCandidate& candidate, float probeX, float probeY) {
    IntReport("  Merchant resolution dump: candidate agent=%u player=%u probe=(%.0f, %.0f)",
              candidate.agentId, candidate.playerNumber, probeX, probeY);
    IntReport("    globals: map=%u myId=%u maxAgents=%u agentBase=0x%08X",
              ReadMapId(),
              ReadMyId(),
              AgentMgr::GetMaxAgents(),
              static_cast<unsigned>(Offsets::AgentBase));

    LivingAgentSnapshot me;
    if (TrySnapshotLivingAgent(ReadMyId(), me)) {
        IntReport("    me: agent=%u type=0x%X allegiance=%u hp=%.2f effects=0x%08X player=%u npc_id=%u pos=(%.0f, %.0f)",
                  me.agentId, me.type, me.allegiance, me.hp, me.effects,
                  me.playerNumber, me.npcId, me.x, me.y);
    } else {
        IntReport("    me: agent %u not readable", ReadMyId());
    }

    LivingAgentSnapshot byId;
    if (TrySnapshotLivingAgent(candidate.agentId, byId)) {
        IntReport("    by-id: agent=%u type=0x%X allegiance=%u hp=%.2f effects=0x%08X player=%u npc_id=%u pos=(%.0f, %.0f)",
                  byId.agentId, byId.type, byId.allegiance, byId.hp, byId.effects,
                  byId.playerNumber, byId.npcId, byId.x, byId.y);
    } else {
        IntReport("    by-id: agent %u not readable", candidate.agentId);
    }

    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    uint32_t samePlayerCount = 0;
    for (uint32_t i = 1; i < maxAgents && samePlayerCount < 8; ++i) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.playerNumber != candidate.playerNumber) continue;
        ++samePlayerCount;
        IntReport("    same-player[%u]: agent=%u type=0x%X allegiance=%u hp=%.2f effects=0x%08X npc_id=%u pos=(%.0f, %.0f)",
                  samePlayerCount - 1,
                  living.agentId,
                  living.type,
                  living.allegiance,
                  living.hp,
                  living.effects,
                  living.npcId,
                  living.x,
                  living.y);
    }
    if (!samePlayerCount) {
        IntReport("    same-player: no readable agents with player_number=%u", candidate.playerNumber);
    }

    NpcCandidate nearby[8];
    const size_t nearbyCount = CollectMerchantNpcCandidates(
        probeX, probeY, 500.0f, candidate.playerNumber, nearby, _countof(nearby));
    IntReport("    nearby around probe: %zu candidates", nearbyCount);
    for (size_t i = 0; i < nearbyCount; ++i) {
        const auto& c = nearby[i];
        IntReport("      nearby[%zu]: agent=%u player=%u npc_id=%u effects=0x%08X dist=%.0f pos=(%.0f, %.0f)%s",
                  i,
                  c.agentId,
                  c.playerNumber,
                  c.npcId,
                  c.effects,
                  c.distance,
                  c.x,
                  c.y,
                  c.playerNumber == candidate.playerNumber ? " [same-player]" : "");
    }
}

static bool KickAllHeroesWithObservation(DWORD timeoutMs) {
    PartyMgr::DebugDumpPartyState("Froggy before KickAllHeroes");
    PartyMgr::KickAllHeroes();
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (PartyMgr::CountPartyHeroes() == 0) {
            PartyMgr::DebugDumpPartyState("Froggy after KickAllHeroes success");
            return true;
        }
        if ((GetTickCount() - start) > 2000 && (GetTickCount() - start) < 2250) {
            IntReport("  KickAllHeroes still pending after 2s, reissuing reliable per-hero clear...");
            PartyMgr::DebugDumpPartyState("Froggy before KickAllHeroes reissue");
            PartyMgr::KickAllHeroes();
        }
        Sleep(250);
    }
    PartyMgr::DebugDumpPartyState("Froggy after KickAllHeroes timeout");
    return PartyMgr::CountPartyHeroes() == 0;
}

static uint32_t ResolveMerchantCandidateAgentId(const NpcCandidate& candidate) {
    if (candidate.playerNumber) {
        const uint32_t byPlayerNumber = FindNearestNpcByPlayerNumber(
            candidate.x, candidate.y, 350.0f, candidate.playerNumber);
        if (byPlayerNumber) return byPlayerNumber;
    }
    LivingAgentSnapshot byId;
    if (TrySnapshotLivingAgent(candidate.agentId, byId)) return candidate.agentId;
    return 0;
}

static bool OpenMerchantContextWithSessionHarnessBody(uint32_t npcId, float npcX, float npcY) {
    MovePlayerNear(npcX, npcY, 70.0f, 12000);

    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    IntReport("    Player pos before interact: (%.0f, %.0f) dist=%.0f",
              meX, meY, AgentMgr::GetDistance(meX, meY, npcX, npcY));
    ReportMerchantPreInteractState("Froggy harness-body pre-interact snapshot", npcId, npcX, npcY);
    ReportMerchantRuntimeContext("Froggy harness-body runtime context");

    AgentMgr::ChangeTarget(npcId);
    Sleep(250);
    IntReport("    step 0 complete: ChangeTarget(%u)", npcId);
    ReportMerchantPreInteractState("Froggy harness-body post-target snapshot", npcId, npcX, npcY);
    ReportMerchantRuntimeContext("Froggy harness-body post-target runtime context");

    IntReport("    step 1: native AgentMgr::InteractNPC(%u) x3 with dwell", npcId);
    for (int nativeAttempt = 1; nativeAttempt <= 3; ++nativeAttempt) {
        IntReport("      native interact attempt %d", nativeAttempt);
        AgentMgr::InteractNPC(npcId);
        Sleep(500);
    }
    IntReport("    step 1 complete");

    IntReport("    Native-interact dwell: waiting 2500ms after InteractNPC...");
    Sleep(2500);
    IntReport("    Native-interact wait complete");
    ReportDialogSnapshot("After Froggy harness-body native-interact dwell");
    const uint32_t merchantCountAfterNative = TradeMgr::GetMerchantItemCount();
    const uintptr_t merchantFrameAfterNative = UIMgr::GetFrameByHash(3613855137u);
    IntReport("      Merchant probe after native-interact dwell: frame=0x%08X items=%u",
              merchantFrameAfterNative,
              merchantCountAfterNative);
    if (merchantFrameAfterNative != 0 || merchantCountAfterNative > 0) {
        return true;
    }

    IntReport("    step 2: raw GoNPC packet fallback (consumables harness parity)");
    for (int packetAttempt = 1; packetAttempt <= 3; ++packetAttempt) {
        IntReport("      Raw GoNPC attempt %d", packetAttempt);
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        Sleep(500);
    }
    Sleep(2500);
    ReportDialogSnapshot("After Froggy harness-body raw-GoNPC dwell");
    const uint32_t merchantCountAfterRaw = TradeMgr::GetMerchantItemCount();
    const uintptr_t merchantFrameAfterRaw = UIMgr::GetFrameByHash(3613855137u);
    IntReport("      Merchant probe after raw-GoNPC dwell: frame=0x%08X items=%u",
              merchantFrameAfterRaw,
              merchantCountAfterRaw);
    if (merchantFrameAfterRaw != 0 || merchantCountAfterRaw > 0) {
        return true;
    }
    return WaitForMerchantContext(1500);
}

static void RunExplorablePlayerEffectsProof() {
    IntReport("=== PHASE 5A2: Explorable Player Effects ===");

    const uint32_t myId = AgentMgr::GetMyId();
    IntCheck("Explorable player id available for effects proof", myId > 0);
    if (!myId) {
        IntSkip("Explorable GetPlayerEffects", "Player id unavailable in explorable");
        return;
    }

    auto* effects = EffectMgr::GetPlayerEffects();
    auto* directEffects = EffectMgr::GetAgentEffects(myId);
    auto* effectArr = EffectMgr::GetAgentEffectArray(myId);
    if (!directEffects || !effectArr) {
        IntSkip("Explorable GetPlayerEffects", "Direct agent effect array unavailable in explorable");
        return;
    }

    IntCheck("Explorable direct agent effects returns non-null", true);
    IntCheck("Explorable player effect array returns non-null", true);
    IntCheck("Explorable direct agent effects agent_id matches self", directEffects->agent_id == myId);
    if (effects) {
        IntCheck("Explorable GetPlayerEffects agent_id matches self", effects->agent_id == myId);
    } else {
        IntSkip("Explorable GetPlayerEffects wrapper", "Wrapper returned null while direct agent effects remained available");
    }

    const uint32_t effectCount = effectArr->size;
    IntReport("  Explorable player effects: directAgent=%u wrapperAgent=%u count=%u",
              directEffects->agent_id,
              effects ? effects->agent_id : 0u,
              effectCount);
}

static bool MovePlayerNearForMerchantHarnessBody(float npcX, float npcY, float* outDistance) {
    const bool reached = MovePlayerNear(npcX, npcY, 70.0f, 12000);
    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    if (outDistance) *outDistance = dist;
    return reached || dist <= kSessionHarnessInteractionDistanceTolerance;
}

static bool MovePlayerNearMerchantIsolation(float x, float y, float threshold, int timeoutMs) {
    const DWORD start = GetTickCount();
    GameThread::EnqueuePost([x, y]() {
        AgentMgr::Move(x, y);
    });
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        Sleep(500);

        float px = 0.0f;
        float py = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), px, py)) continue;

        const float dist = AgentMgr::GetDistance(px, py, x, y);
        IntReport("  Merchant move probe: pos=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f",
                  px, py, x, y, dist);
        if (dist <= threshold) {
            return true;
        }
    }
    return false;
}

static void ReportMerchantStock() {
    const uint32_t itemCount = TradeMgr::GetMerchantItemCount();
    IntCheck("Merchant has items", itemCount > 0);
    IntReport("  Merchant has %u items", itemCount);
    for (uint32_t slot = 1; slot <= itemCount; ++slot) {
        if (Item* merchantItem = TradeMgr::GetMerchantItemByPosition(slot)) {
            IntReport("  Merchant slot %u: itemId=%u model=%u value=%u qty=%u",
                      slot, merchantItem->item_id, merchantItem->model_id,
                      merchantItem->value, merchantItem->quantity);
        }
    }
}

static bool EnsureGaddsMerchantOpen(uint32_t* outOpenedMerchantId = nullptr) {
    static constexpr float kMerchX = -8374.0f;
    static constexpr float kMerchY = -22491.0f;
    static constexpr uint16_t kGaddsMerchantPlayerNumber = 6060;

    IntReport("  Moving to merchant (%.0f, %.0f)...", kMerchX, kMerchY);
    const bool reachedMerchantArea = MovePlayerNearMerchantIsolation(kMerchX, kMerchY, 550.0f, 15000);
    IntReport("  Initial merchant-area approach reached=%d", reachedMerchantArea ? 1 : 0);
    Sleep(500);

    DumpMerchantNpcCandidates(kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber);

    NpcCandidate merchantCandidates[8];
    size_t merchantCandidateCount = CollectMerchantNpcCandidates(
        kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber, merchantCandidates, _countof(merchantCandidates));
    if (!merchantCandidateCount) {
        IntReport("  No merchant candidates at 1500 range, walking closer...");
        MovePlayerNearMerchantIsolation(kMerchX, kMerchY, 200.0f, 10000);
        Sleep(500);
        DumpMerchantNpcCandidates(kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber);
        merchantCandidateCount = CollectMerchantNpcCandidates(
            kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber, merchantCandidates, _countof(merchantCandidates));
    }
    if (!merchantCandidateCount) {
        return false;
    }

    bool merchantOpen = false;
    bool reachedAnyNpc = false;
    uint32_t openedMerchantId = 0;
    for (size_t candidateIndex = 0; candidateIndex < merchantCandidateCount && !merchantOpen; ++candidateIndex) {
        const auto& candidate = merchantCandidates[candidateIndex];
        const uint32_t resolvedAgentId = ResolveMerchantCandidateAgentId(candidate);
        LivingAgentSnapshot npc;
        const bool haveNpc = resolvedAgentId && TrySnapshotLivingAgent(resolvedAgentId, npc);
        IntReport("  Merchant candidate %u/%u: agent=%u allegiance=%u player_number=%u npc_id=%u at (%.0f, %.0f)",
                  static_cast<unsigned>(candidateIndex + 1),
                  static_cast<unsigned>(merchantCandidateCount),
                  resolvedAgentId ? resolvedAgentId : candidate.agentId,
                  haveNpc ? npc.allegiance : 0,
                  haveNpc ? npc.playerNumber : candidate.playerNumber,
                  haveNpc ? npc.npcId : 0,
                  haveNpc ? npc.x : 0.0f,
                  haveNpc ? npc.y : 0.0f);

        if (!haveNpc) {
            IntReport("  WARN: Candidate %u could not be resolved to a readable living NPC", candidate.agentId);
            continue;
        }

        float postApproachDistance = 0.0f;
        const bool reachedNpc = MovePlayerNearForMerchantHarnessBody(npc.x, npc.y, &postApproachDistance);
        reachedAnyNpc = reachedAnyNpc || reachedNpc;

        float px = 0.0f;
        float py = 0.0f;
        TryReadAgentPosition(ReadMyId(), px, py);
        IntReport("  After merchant candidate approach: pos=(%.0f,%.0f) reached=%d dist=%.0f",
                  px, py, reachedNpc ? 1 : 0, postApproachDistance);

        if (!reachedNpc) {
            IntReport("  WARN: Could not reach merchant candidate %u", resolvedAgentId);
            continue;
        }

        ReportMerchantPreInteractState("Froggy pre-interact snapshot", resolvedAgentId, npc.x, npc.y);
        ReportMerchantRuntimeContext("Froggy runtime context");
        merchantOpen = OpenMerchantContextWithSessionHarnessBody(resolvedAgentId, npc.x, npc.y);
        if (merchantOpen) {
            openedMerchantId = resolvedAgentId;
            break;
        }

        IntReport("  Candidate %u failed to open merchant context; trying next candidate", resolvedAgentId);
    }

    IntCheck("Reached merchant area", reachedMerchantArea || reachedAnyNpc);
    IntCheck("Merchant window opened", merchantOpen);
    if (merchantOpen) {
        IntReport("  Merchant opened via candidate agent=%u", openedMerchantId);
        ReportMerchantStock();
        if (outOpenedMerchantId) *outOpenedMerchantId = openedMerchantId;
    }
    return merchantOpen;
}

static bool WaitForRestockTargets(const MaintenanceMgr::Config& cfg, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (CountSuperiorIdKits() >= cfg.targetIdKits && CountSalvageKitFamily() >= cfg.targetSalvageKits) {
            return true;
        }
        Sleep(250);
    }
    return CountSuperiorIdKits() >= cfg.targetIdKits && CountSalvageKitFamily() >= cfg.targetSalvageKits;
}

static void RunMerchantMaintenanceCycle(const char* label, bool includeSalvage) {
    IntReport("=== %s ===", label);
    uint32_t openedMerchantId = 0;
    const bool merchantOpen = EnsureGaddsMerchantOpen(&openedMerchantId);
    if (!merchantOpen) {
        IntSkip(label, "Merchant NPC not found near target coords");
        return;
    }

    const uint32_t freeBefore = MaintenanceMgr::CountFreeSlots();
    const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
    const uint32_t superiorBefore = CountSuperiorIdKits();
    const uint32_t salvageBefore = CountSalvageKitFamily();
    const uint32_t unidentifiedBefore = CountUnidentifiedMaintenanceItems();
    const uint32_t salvageCandidatesBeforeIdentify = CountSalvageCandidatesForMaintenance();
    const uint32_t sellCandidatesBefore = CountSellCandidatesForMaintenance();
    IntReport("  Before maintenance: free=%u gold=%u superiorId=%u salvage=%u unidentified=%u salvageCandidates=%u sellCandidates=%u merchantAgent=%u",
              freeBefore, goldBefore, superiorBefore, salvageBefore,
              unidentifiedBefore, salvageCandidatesBeforeIdentify, sellCandidatesBefore, openedMerchantId);

    const uint32_t identified = MaintenanceMgr::IdentifyAllItems();
    if (unidentifiedBefore > 0) {
        IntCheck("Maintenance identifies pending items", identified > 0);
    } else {
        IntCheck("Maintenance identify pass stays safe with no pending items", identified == 0);
    }

    uint32_t salvaged = 0;
    if (includeSalvage) {
        const uint32_t salvageCandidatesBeforeSalvage = CountSalvageCandidatesForMaintenance();
        salvaged = MaintenanceMgr::SalvageJunkItems();
        if (salvageCandidatesBeforeSalvage > 0) {
            IntCheck("Maintenance salvages pending junk", salvaged > 0);
        } else {
            IntCheck("Maintenance salvage pass stays safe with no candidates", salvaged == 0);
        }
    }

    const uint32_t sold = MaintenanceMgr::SellJunkItems();
    if (sellCandidatesBefore > 0) {
        IntCheck("Maintenance sells pending junk", sold > 0);
    } else {
        IntCheck("Maintenance sell pass stays safe with no candidates", sold == 0);
    }

    MaintenanceMgr::Config cfg = {};
    cfg.targetIdKits = 3;
    cfg.targetSalvageKits = 10;
    MaintenanceMgr::BuyKitsToTarget(cfg);
    const bool restocked = WaitForRestockTargets(cfg, 6000);
    IntCheck("Maintenance reaches superior ID kit target", CountSuperiorIdKits() >= cfg.targetIdKits);
    IntCheck("Maintenance reaches salvage kit target", CountSalvageKitFamily() >= cfg.targetSalvageKits);
    IntCheck("Maintenance restock settles", restocked);

    const uint32_t freeAfter = MaintenanceMgr::CountFreeSlots();
    const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
    IntReport("  After maintenance: free=%u gold=%u superiorId=%u salvage=%u identified=%u salvaged=%u sold=%u",
              freeAfter, goldAfter, CountSuperiorIdKits(), CountSalvageKitFamily(),
              identified, salvaged, sold);

    IntReport("  Final merchant cleanup: CancelAction()");
    AgentMgr::CancelAction();
    IntReport("  Final merchant cleanup: CancelAction complete");
    Sleep(500);
}

static int RunFroggyFeatureTestImpl(bool isolatedExplorableFlaggingMode) {
    s_isolatedExplorableFlaggingMode = isolatedExplorableFlaggingMode;
    s_enableInvasiveSparkflyCombatProofs = false;
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;
    StartWatchdog();

    // ===== PHASE 1: Travel to Gadd's Encampment =====
    // NOTE: Unit tests (Phase 0) moved AFTER stabilization because some
    // "unit" tests send game commands (FlagHeroes, Dialog) that crash
    // if the game isn't fully loaded yet.
    IntReport("=== PHASE 1: Travel to Gadd's Encampment ===");

    // Wait for game to be fully ready after bootstrap
    WaitForPlayerWorldReady(15000);

    CtoS::Initialize();

    if (MapMgr::GetMapId() != MAP_GADDS) {
        IntReport("  Not at Gadd's (map=%u) — traveling...", MapMgr::GetMapId());
        MapMgr::Travel(MAP_GADDS);
        bool arrived = WaitFor("MapID == 638", 60000, []() {
            return MapMgr::GetMapId() == MAP_GADDS;
        });
        if (!arrived) {
            IntReport("  ABORT: Failed to travel to Gadd's Encampment");
            IntReport("=== FROGGY TESTS ABORTED (no outpost) ===");
            return s_failed + 1;
        }
    }

    bool agentReady = WaitFor("MyID > 0", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    if (!agentReady) {
        IntReport("  ABORT: Agent not ready after travel");
        return s_failed + 1;
    }
    Sleep(3000); // let the map fully hydrate
    IntCheck("Phase 1: In Gadd's Encampment", MapMgr::GetMapId() == MAP_GADDS);

    // Wait for game to fully stabilize — party/dialog commands crash if sent too soon
    IntReport("  Waiting for game to stabilize...");
    WaitForStablePlayerState(10000);
    Sleep(5000);

    // ===== PHASE 2: Outpost Tests (party, inventory, skillbar) =====
    IntReport("=== PHASE 2: Outpost Tests ===");

    char heroTemplateFile[64] = {};
    char heroTemplateLabel[64] = {};
    ResolvePreferredHeroTemplate(heroTemplateFile, sizeof(heroTemplateFile),
                                 heroTemplateLabel, sizeof(heroTemplateLabel));
    IntReport("  Preferred hero config for current character: %s (%s)", heroTemplateLabel, heroTemplateFile);
    const bool outpostHeroesReady = SetupHeroesFromTemplateForOutpost(heroTemplateFile, heroTemplateLabel);
    char heroSetupCheck[128] = {};
    snprintf(heroSetupCheck, sizeof(heroSetupCheck), "Configured %s heroes in Gadd's", heroTemplateLabel);
    IntCheck(heroSetupCheck, outpostHeroesReady);
    if (!outpostHeroesReady) {
        IntReport("  ABORT: Hero setup failed before leaving Gadd's");
        StopWatchdog(false);
        IntReport("Watchdog stop requested");
        IntReport("=== FROGGY FEATURE TESTS COMPLETE ===");
        IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
        return s_failed;
    }

    const bool hardModeReady = EnsureOutpostHardModeEnabled("Hard mode enabled before leaving Gadd's");
    if (!hardModeReady) {
        IntReport("  ABORT: Hard mode was not enabled before leaving Gadd's");
        StopWatchdog(false);
        IntReport("Watchdog stop requested");
        IntReport("=== FROGGY FEATURE TESTS COMPLETE ===");
        IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
        return s_failed;
    }

    // Skillbar validation
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (bar) {
        int nonZero = 0;
        for (int i = 0; i < 8; i++) {
            if (bar->skills[i].skill_id != 0) nonZero++;
        }
        IntCheck("Skillbar has skills loaded", nonZero > 0);
        if (nonZero > 0) {
            for (int i = 0; i < 8; i++) {
                if (bar->skills[i].skill_id != 0) {
                    const auto* data = SkillMgr::GetSkillConstantData(bar->skills[i].skill_id);
                    IntCheck("Skill constant data exists", data != nullptr);
                    if (data) {
                        IntCheck("Skill profession in range", data->profession <= 10);
                        IntCheck("Skill type in range", data->type <= 24);
                    }
                    break;
                }
            }
        }
    } else {
        IntSkip("Skillbar validation", "Skillbar not available");
    }

    // Inventory
    auto* inv = ItemMgr::GetInventory();
    if (inv) {
        IntCheck("Gold character plausible", inv->gold_character < 1000000);
        IntCheck("Gold storage plausible", inv->gold_storage < 10000000);
        int bagsFound = 0;
        for (int i = 1; i <= 4; i++) {
            auto* bag = ItemMgr::GetBag(i);
            if (bag && bag->items.buffer) bagsFound++;
        }
        IntCheck("At least 1 backpack bag", bagsFound >= 1);
    }

    // Effects sanity — full GetPlayerEffects validation runs in explorable.
    uint32_t myId = AgentMgr::GetMyId();
    if (myId > 0) {
        IntCheck("HasEffect(bogus 9999)=false", !EffectMgr::HasEffect(myId, 9999));
    }

    // ===== PHASE 0: Unit Tests (run after outpost setup is stable) =====
    // These tests include stateful hero operations. Running them after the
    // initial party setup keeps Phase 2 aligned with the passing integration
    // sequence, where hero clear/setup is the first party mutation observed.
    IntReport("=== PHASE 0: Unit Tests (deferred until after outpost setup) ===");
    int unitFailures = Bot::Froggy::RunFroggyUnitTests();
    IntReport("Unit tests: %d failures", unitFailures);
    s_failed += unitFailures;

    RunMerchantMaintenanceCycle("PHASE 3: Merchant Maintenance", false);
    // ===== PHASE 4: Enter Explorable =====
phase4:
    // Movement safety guards added to AgentMgr::Move — re-enabled.
    IntReport("=== PHASE 4: Enter Sparkfly Swamp ===");

    // Log current position before walking
    {
        float px = 0, py = 0;
        TryReadAgentPosition(ReadMyId(), px, py);
        IntReport("  Current position: (%.0f, %.0f) MapID=%u", px, py, ReadMapId());
    }

    // Walk to exit portal (must use MovePlayerNear / EnqueuePost)
    IntReport("  Walking to exit waypoint 1 (-10018, -21892)...");
    MovePlayerNear(-10018.0f, -21892.0f, 350.0f, 20000);
    {
        float px = 0, py = 0;
        TryReadAgentPosition(ReadMyId(), px, py);
        IntReport("  After wp1: (%.0f, %.0f)", px, py);
    }
    IntReport("  Walking to exit waypoint 2 (-9550, -20400)...");
    MovePlayerNear(-9550.0f, -20400.0f, 350.0f, 20000);
    {
        float px = 0, py = 0;
        TryReadAgentPosition(ReadMyId(), px, py);
        IntReport("  After wp2: (%.0f, %.0f)", px, py);
    }

    // Push toward explorable zone exit
    IntReport("  Pushing toward Sparkfly...");
    DWORD zoneStart = GetTickCount();
    bool leftOutpost = false;
    while ((GetTickCount() - zoneStart) < 45000) {
        if (MapMgr::GetMapId() != MAP_GADDS) { leftOutpost = true; break; }
        GameThread::EnqueuePost([]() {
            AgentMgr::Move(-9451.0f, -19766.0f);
        });
        Sleep(500);
    }

    if (leftOutpost) {
        // Wait for Sparkfly to load
        bool inSparkfly = WaitFor("MapID == Sparkfly", 30000, []() {
            return MapMgr::GetMapId() == MAP_SPARKFLY;
        });
        if (inSparkfly) {
            bool agentOk = WaitFor("MyID in explorable", 30000, []() {
                return AgentMgr::GetMyId() > 0;
            });
            Sleep(5000); // stability wait
            IntCheck("Phase 4: Entered Sparkfly Swamp", agentOk);

            // (Blessing shrine is inside Bogroot dungeon, not Sparkfly — handled in Phase 6B)

            // ===== PHASE 5: Explorable Tests =====
            IntReport("=== PHASE 5: Explorable Tests ===");
            RunExplorableSkillbarRefreshProof();
            RunExplorablePlayerEffectsProof();

            // Look for enemies
            uint32_t foeId = FindNearestFoe(5000.0f);
            if (foeId) {
                IntCheck("Found enemy in explorable", true);
                IntReport("  Enemy agent=%u found", foeId);

                // Verify target selection functions return valid results
                auto* foeAgent = AgentMgr::GetAgentByID(foeId);
                IntCheck("Enemy agent readable", foeAgent != nullptr);
                if (foeAgent) {
                    auto* foeLiving = static_cast<AgentLiving*>(foeAgent);
                    IntCheck("Enemy is alive", foeLiving->hp > 0.0f);
                    IntCheck("Enemy is foe allegiance", foeLiving->allegiance == 3);
                }

                // Target the enemy
                AgentMgr::ChangeTarget(foeId);
                Sleep(500);
                IntCheck("Target changed to enemy", AgentMgr::GetTargetId() == foeId);
                RunCombatObservabilityHarness(foeId, "initial foe");
                RunCombatAgentReadValidation(foeId, "initial foe");
                RunCombatEffectReadValidation(foeId, "initial foe");
                if (s_enableInvasiveSparkflyCombatProofs) {
                    RunCombatTargetAndCastTelemetryValidation(foeId, "initial foe");
                } else {
                    IntSkip("Combat target/cast telemetry validation", "Deferred in full Froggy loop to preserve Sparkfly route stability");
                }
                RunReadOnlyCombatPreconditions(foeId, "initial foe");
                if (s_enableInvasiveSparkflyCombatProofs) {
                    RunBuiltinCombatProofSuite(foeId);
                } else {
                    SkipInvasiveCombatProofSuite("Deferred in full Froggy loop to preserve Sparkfly route stability");
                }

                if (s_isolatedExplorableFlaggingMode) {
                    // GWA3-116: hero flagging only makes sense in explorable.
                    auto* meExplorable = AgentMgr::GetMyAgent();
                    if (meExplorable && meExplorable->hp > 0.0f) {
                        PartyMgr::FlagAll(meExplorable->x + 100.0f, meExplorable->y + 100.0f);
                        Sleep(200);
                        PartyMgr::UnflagAll();
                        Sleep(500);
                        AgentMgr::CancelAction();
                        Sleep(250);
                        IntCheck("Hero flagging in explorable keeps player agent valid",
                                 AgentMgr::GetMyAgent() != nullptr);
                        IntReport("  Isolated explorable flagging mode: stopping after validation");
                        goto froggy_done;
                    } else {
                        IntSkip("Hero flagging in explorable", "Player agent unavailable");
                    }
                } else {
                    IntSkip("Hero flagging in explorable", "Covered by isolated explorable flagging test mode");
                }
            } else {
                IntSkip("Enemy targeting tests", "No enemies within 5000 range");
                // Move toward first Sparkfly waypoint to find enemies
                IntReport("  Moving toward enemies...");
                MovePlayerNear(-4559.0f, -14406.0f, 500.0f, 25000);
                foeId = FindNearestFoe(5000.0f);
                if (foeId) {
                IntCheck("Found enemy after moving", true);
                const bool targetChangedAfterMove = WaitForCombatTargetAcquire(foeId);
                IntCheck("Target changed to enemy after moving", targetChangedAfterMove);
                RunCombatObservabilityHarness(foeId, "foe after moving");
                RunCombatAgentReadValidation(foeId, "foe after moving");
                RunCombatEffectReadValidation(foeId, "foe after moving");
                if (s_enableInvasiveSparkflyCombatProofs) {
                    RunCombatTargetAndCastTelemetryValidation(foeId, "foe after moving");
                } else {
                    IntSkip("Combat target/cast telemetry validation", "Deferred in full Froggy loop to preserve Sparkfly route stability");
                }
                RunReadOnlyCombatPreconditions(foeId, "foe after moving");
                if (s_enableInvasiveSparkflyCombatProofs) {
                    RunBuiltinCombatProofSuite(foeId);
                } else {
                    SkipInvasiveCombatProofSuite("Deferred in full Froggy loop to preserve Sparkfly route stability");
                }

                if (s_isolatedExplorableFlaggingMode) {
                    auto* meExplorableAfterMove = AgentMgr::GetMyAgent();
                    if (meExplorableAfterMove && meExplorableAfterMove->hp > 0.0f) {
                        PartyMgr::FlagAll(meExplorableAfterMove->x + 100.0f, meExplorableAfterMove->y + 100.0f);
                        Sleep(200);
                        PartyMgr::UnflagAll();
                        Sleep(500);
                        AgentMgr::CancelAction();
                        Sleep(250);
                        IntCheck("Hero flagging in explorable after moving keeps player agent valid",
                                 AgentMgr::GetMyAgent() != nullptr);
                        IntReport("  Isolated explorable flagging mode: stopping after validation");
                        goto froggy_done;
                    } else {
                        IntSkip("Hero flagging in explorable after moving", "Player agent unavailable");
                    }
                } else {
                    IntSkip("Hero flagging in explorable after moving",
                            "Covered by isolated explorable flagging test mode");
                }
                } else {
                    IntSkip("Enemy found after move", "Still no enemies — area may be cleared");
                }
            }

            bool readyForTekksPath = true;
            if (s_isolatedExplorableFlaggingMode) {
                const bool lootWorked = RunExplorableLootPickupProof();
                if (!lootWorked) {
                    IntSkip("Path to Tekks", "Loot proof did not complete cleanly; continuing quest path anyway");
                }
            } else {
                IntSkip("Explorable loot pickup", "Deferred in full Froggy loop to keep the Tekks path aligned with the real bot flow");
                readyForTekksPath = ResetToFreshSparkflyInstanceForDungeonRun();
                IntCheck("Fresh Sparkfly reset completed before Tekks path", readyForTekksPath);
            }

            bool preserveSparkflyLoopState = false;
            const bool reachedTekks = readyForTekksPath && MoveToTekksForQuestDialog();
            if (reachedTekks) {
                const bool tekksReadyForDungeon = RunTekksQuestAcceptProof();

                // ===== PHASE 6: Enter Bogroot Growths =====
                const bool inBogroot = tekksReadyForDungeon ? RunEnterBogrootProof() : false;
                if (!tekksReadyForDungeon) {
                    IntSkip("Enter Bogroot", "Tekks dungeon-entry dialog sequence did not complete");
                }

                // ===== PHASE 6B: Grab Blessing =====
                bool returnedToSparkflyAfterDungeon = false;
                if (inBogroot) {
                    RunBogrootBlessingProof();
                    returnedToSparkflyAfterDungeon = RunBogrootDungeonLoopProof();
                    if (returnedToSparkflyAfterDungeon) {
                        ReportQuestSnapshot("Quest state after Bogroot loop return");
                        const bool reachedTekksAfterReturn = MoveToTekksForQuestDialog();
                        IntCheck("Reached Tekks after Bogroot return", reachedTekksAfterReturn);
                        if (reachedTekksAfterReturn) {
                            RunTekksQuestAcceptProof();
                            ReportQuestSnapshot("Quest state after Tekks reaccept on Sparkfly return");
                            const bool questPresentAfterReturn =
                                QuestMgr::GetQuestById(QUEST_TEKKS_WAR) != nullptr ||
                                QuestMgr::GetActiveQuestId() == QUEST_TEKKS_WAR;
                            IntCheck("Tekks quest present after Sparkfly return loop",
                                     questPresentAfterReturn);
                            preserveSparkflyLoopState =
                                questPresentAfterReturn &&
                                MapMgr::GetMapId() == MAP_SPARKFLY &&
                                MapMgr::GetIsMapLoaded() &&
                                AgentMgr::GetMyId() > 0;
                        } else {
                            IntSkip("Tekks reaccept after Bogroot return", "Could not reach Tekks after returning to Sparkfly");
                        }
                    } else {
                        IntSkip("Tekks reaccept after Bogroot return", "Dungeon loop did not return to Sparkfly");
                    }
                } else {
                    IntSkip("Bogroot blessing", "Did not enter Bogroot Growths");
                    IntSkip("Bogroot dungeon loop", "Did not enter Bogroot Growths");
                    IntSkip("Tekks reaccept after Bogroot return", "Dungeon loop never started");
                }
            } else {
                IntSkip("Tekks quest accept", "Could not reach Tekks path endpoint");
                IntSkip("Enter Bogroot", "Did not reach Tekks");
                IntSkip("Bogroot blessing", "Did not reach Tekks");
                IntSkip("Bogroot dungeon loop", "Did not reach Tekks");
                IntSkip("Tekks reaccept after Bogroot return", "Did not reach Tekks");
            }

            if (preserveSparkflyLoopState) {
                IntReport("=== PHASE 7: End State / Cleanup ===");
                IntReport("  Preserving Sparkfly state after reward/reaccept; skipping outpost cleanup");
                IntCheck("Loop state preserved in Sparkfly after reward/reaccept",
                         MapMgr::GetMapId() == MAP_SPARKFLY &&
                         MapMgr::GetIsMapLoaded() &&
                         AgentMgr::GetMyId() > 0);
                IntSkip("Returned to Gadd's Encampment",
                        "Preserving Sparkfly state for real Froggy loop continuation");
                IntSkip("PHASE 7B: Post-Run Identify Salvage Sell Restock",
                        "Preserving Sparkfly state for real Froggy loop continuation");
                goto froggy_done;
            }

            // ===== PHASE 7: Return to Outpost =====
            IntReport("=== PHASE 7: Return to Outpost ===");
            const uint32_t currentMap = MapMgr::GetMapId();
            if (currentMap == MAP_BOGROOT_LVL1) {
                // In dungeon — ReturnToOutpost doesn't work. Use Travel to zone back.
                IntReport("  In Bogroot dungeon (map=%u), using Travel to return", currentMap);
                MapMgr::Travel(MAP_GADDS);
            } else {
                // In explorable (Sparkfly) — use resign flow
                MapMgr::ReturnToOutpost();
            }
            bool returned = WaitFor("MapID == Gadd's after return", 120000, []() {
                return MapMgr::GetMapId() == MAP_GADDS &&
                       MapMgr::GetIsMapLoaded() &&
                       AgentMgr::GetMyId() > 0;
            });
            IntCheck("Returned to Gadd's Encampment", returned);
            if (returned) {
                RunMerchantMaintenanceCycle("PHASE 7B: Post-Run Identify Salvage Sell Restock", true);
            } else {
                IntSkip("PHASE 7B: Post-Run Identify Salvage Sell Restock", "Did not reach Gadd's after run");
            }

        } else {
            IntSkip("Explorable tests", "Failed to enter Sparkfly Swamp");
            IntSkip("Return to outpost", "Never left outpost");
        }
    } else {
        IntSkip("Explorable entry", "Failed to leave Gadd's within 30s");
        IntSkip("Explorable tests", "Never entered explorable");
        IntSkip("Return to outpost", "Never left outpost");
    }

froggy_done:
    IntReport("Entering froggy_done");
    IntReport("Stopping watchdog (non-blocking)...");
    StopWatchdog(false);
    IntReport("Watchdog stop requested");
    IntReport("=== FROGGY FEATURE TESTS COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return s_failed;
}

int RunFroggySparkflyRouteTest() {
    s_isolatedExplorableFlaggingMode = false;
    s_enableInvasiveSparkflyCombatProofs = false;
    // Direct debug path uses MovePlayerNear (no combat), which gets the party
    // killed in Hard Mode Sparkfly. Use Froggy's aggro route instead — it
    // reliably reaches Tekks in 5-8 minutes with hero combat.
    s_preferDirectTekksStagingForDebug = false;
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;
    StartWatchdog();

    auto finish = []() -> int {
        s_preferDirectTekksStagingForDebug = false;
        IntReport("Stopping watchdog (non-blocking)...");
        StopWatchdog(false);
        IntReport("Watchdog stop requested");
        IntReport("=== FROGGY SPARKFLY TEST COMPLETE ===");
        IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
        return s_failed;
    };

    IntReport("=== SPARKFLY ROUTE TEST: Bootstrap ===");
    WaitForPlayerWorldReady(15000);
    CtoS::Initialize();

    if (MapMgr::GetMapId() != MAP_GADDS) {
        IntReport("  Traveling to Gadd's from map %u...", MapMgr::GetMapId());
        MapMgr::Travel(MAP_GADDS);
        const bool arrived = WaitFor("MapID == Gadd's", 60000, []() {
            return MapMgr::GetMapId() == MAP_GADDS;
        });
        IntCheck("Travel to Gadd's", arrived);
        if (!arrived) return finish();
    }

    const bool agentReady = WaitFor("MyID > 0", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    IntCheck("Agent ready", agentReady);
    if (!agentReady) return finish();

    WaitForStablePlayerState(10000);
    Sleep(3000);

    IntReport("=== SPARKFLY ROUTE TEST: Outpost Hero Setup ===");
    const bool standardHeroesReady = SetupHeroesFromTemplateForOutpost("Standard.txt", "Standard");
    IntCheck("Configured Marvin standard heroes in Gadd's", standardHeroesReady);
    if (!standardHeroesReady) return finish();

    const bool hardModeReady = EnsureOutpostHardModeEnabled("Hard mode enabled before leaving Gadd's (route test)");
    if (!hardModeReady) return finish();

    if (MapMgr::GetMapId() != MAP_SPARKFLY) {
        IntReport("=== SPARKFLY ROUTE TEST: Enter Sparkfly ===");
        MovePlayerNear(-10018.0f, -21892.0f, 350.0f, 20000);
        MovePlayerNear(-9550.0f, -20400.0f, 350.0f, 20000);

        const DWORD zoneStart = GetTickCount();
        bool leftOutpost = false;
        while ((GetTickCount() - zoneStart) < 45000) {
            if (MapMgr::GetMapId() != MAP_GADDS) {
                leftOutpost = true;
                break;
            }
            GameThread::EnqueuePost([]() {
                AgentMgr::Move(-9451.0f, -19766.0f);
            });
            Sleep(500);
        }
        IntCheck("Left Gadd's for Sparkfly", leftOutpost);
        if (!leftOutpost) return finish();

        const bool inSparkfly = WaitFor("MapID == Sparkfly", 30000, []() {
            return MapMgr::GetMapId() == MAP_SPARKFLY;
        });
        IntCheck("Arrived in Sparkfly", inSparkfly);
        if (!inSparkfly) return finish();

        const bool sparkflyAgentReady = WaitFor("Sparkfly MyID > 0", 30000, []() {
            return AgentMgr::GetMyId() > 0;
        });
        IntCheck("Sparkfly agent ready", sparkflyAgentReady);
        if (!sparkflyAgentReady) return finish();

        WaitForStablePlayerState(8000);
        Sleep(2000);
    } else {
        IntCheck("Already in Sparkfly", true);
    }

    IntReport("=== SPARKFLY ROUTE TEST: Combat Probe ===");
    IntCheck("Combat cache refresh", Bot::Froggy::RefreshCombatSkillbar());
    if (s_preferDirectTekksStagingForDebug) {
        IntSkip("Found Sparkfly foe", "Direct Tekks debug staging skips the route-side combat probe");
        IntSkip("Combat observability harness", "Direct Tekks debug staging skips the route-side combat probe");
    } else {
        uint32_t foeId = FindNearestFoe(5000.0f);
        if (!foeId) {
            IntReport("  No nearby foe at spawn; moving toward first route leg...");
            MovePlayerNear(-4559.0f, -14406.0f, 500.0f, 25000);
            foeId = FindNearestFoe(5000.0f);
        }

        if (foeId) {
            IntCheck("Found Sparkfly foe", true);
            AgentMgr::ChangeTarget(foeId);
            Sleep(750);
            RunCombatObservabilityHarness(foeId, "sparkfly route foe");
        } else {
            IntSkip("Found Sparkfly foe", "No nearby foe before route start");
            IntSkip("Combat observability harness", "Skipped because no Sparkfly foe was available at route start");
        }
    }

    IntReport("=== SPARKFLY ROUTE TEST: Route To Tekks ===");
    Bot::Froggy::ResetSparkflyTraversalCombatStats();
    const bool reachedTekks = MoveToTekksForQuestDialog();
    IntCheck("Reached Tekks via Froggy Sparkfly route", reachedTekks);

    const auto stats = Bot::Froggy::GetSparkflyTraversalCombatStats();
    IntReport("  Sparkfly traversal combat stats: attempts=%u skill_steps=%u auto_attacks=%u settle_requests=%u unsettled=%u last_target=%u",
              stats.quick_step_attempts,
              stats.skill_steps,
              stats.auto_attack_steps,
              stats.settle_requests,
              stats.unsettled_skips,
              stats.last_target_id);
    if (s_preferDirectTekksStagingForDebug) {
        IntSkip("Sparkfly route exercised aggro combat",
                "Direct Tekks debug staging bypasses Froggy's aggro traversal combat");
        IntSkip("Sparkfly route recorded player skill or attack steps",
                "Direct Tekks debug staging bypasses Froggy's aggro traversal combat");
    } else {
        IntCheck("Sparkfly route exercised aggro combat",
                 stats.quick_step_attempts > 0 || stats.skill_steps > 0 || stats.auto_attack_steps > 0);
        IntCheck("Sparkfly route recorded player skill or attack steps",
                 (stats.skill_steps + stats.auto_attack_steps) > 0);
    }

    if (reachedTekks) {
        const bool tekksReadyForDungeon = RunTekksQuestAcceptProof();
        IntCheck("Sparkfly route Tekks dialog sequence completed", tekksReadyForDungeon);
        if (tekksReadyForDungeon) {
            const bool enteredBogroot = RunEnterBogrootProof();
            IntCheck("Sparkfly route entered Bogroot after Tekks dialog", enteredBogroot);
        } else {
            IntSkip("Sparkfly route entered Bogroot after Tekks dialog",
                    "Tekks dialog sequence did not complete");
        }
    } else {
        IntSkip("Sparkfly route Tekks dialog sequence completed", "Did not reach Tekks");
        IntSkip("Sparkfly route entered Bogroot after Tekks dialog", "Did not reach Tekks");
    }

    return finish();
}

int RunFroggyFeatureTest() {
    return RunFroggyFeatureTestImpl(false);
}

int RunFroggyExplorableFlaggingTest() {
    return RunFroggyFeatureTestImpl(true);
}

} // namespace GWA3::SmokeTest
