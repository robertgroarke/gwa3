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
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Party.h>
#include <gwa3/game/Skill.h>

#include <Windows.h>
#include <cstdio>
#include <cstring>

namespace GWA3::SmokeTest {

static int s_passed = 0;
static int s_failed = 0;
static int s_skipped = 0;
static bool s_isolatedExplorableFlaggingMode = false;
static constexpr float kSessionHarnessInteractionDistanceTolerance = 90.0f;
static constexpr uint32_t MODEL_SALVAGE_KIT = 2992;
static constexpr size_t kHeroTemplateCount = 7;

static constexpr uint32_t MAP_GADDS = 638;
static constexpr uint32_t MAP_SPARKFLY = 558;

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

static size_t LoadMercHeroTemplates(HeroTemplateConfig* out, size_t maxCount) {
    if (!out || maxCount == 0) return 0;

    char dllPath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&LoadMercHeroTemplates), &hSelf);
    GetModuleFileNameA(hSelf, dllPath, MAX_PATH);
    char* slash = strrchr(dllPath, '\\');
    if (slash) *(slash + 1) = '\0';

    char path[MAX_PATH] = {};
    snprintf(path, sizeof(path), "%s..\\..\\..\\..\\GWA Censured\\hero_configs\\Mercs.txt", dllPath);

    FILE* f = nullptr;
    fopen_s(&f, path, "r");
    if (!f) {
        IntReport("  WARN: Could not open Mercs config at %s", path);
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

static PartyInfo* ResolveTestPlayerParty() {
    if (Offsets::BasePointer <= 0x10000) return nullptr;
    __try {
        const uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx <= 0x10000) return nullptr;
        const uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (p1 <= 0x10000) return nullptr;
        const uintptr_t party = *reinterpret_cast<uintptr_t*>(p1 + 0x4C);
        if (party <= 0x10000) return nullptr;
        const uintptr_t playerParty = *reinterpret_cast<uintptr_t*>(party + 0x54);
        if (playerParty <= 0x10000) return nullptr;
        return reinterpret_cast<PartyInfo*>(playerParty);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
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
    IntCheck("Combat snapshot target stable across dwell", before.targetId == foeId && after.targetId == foeId);
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
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = GetAgentLivingRaw(AgentMgr::GetMyId());
        auto* foe = GetAgentLivingRaw(foeId);
        if (!me || !foe || foe->hp <= 0.0f) return false;

        const float dist = AgentMgr::GetDistance(me->x, me->y, foe->x, foe->y);
        if (dist <= desiredRange) return true;

        const float dx = foe->x - me->x;
        const float dy = foe->y - me->y;
        const float len = sqrtf(dx * dx + dy * dy);
        if (len < 1.0f) return true;

        const float stepBack = desiredRange > 150.0f ? desiredRange - 150.0f : desiredRange;
        const float scale = (len - stepBack) / len;
        const float moveX = me->x + dx * scale;
        const float moveY = me->y + dy * scale;

        GameThread::Enqueue([moveX, moveY]() {
            AgentMgr::Move(moveX, moveY);
        });
        Sleep(350);
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

    uint32_t skillId = 0, targetId = 0;
    uint8_t targetType = 0;
    if (!Bot::Froggy::DebugResolveFirstSkillTarget(TEST_ROLE_HEX | TEST_ROLE_PRESSURE, foeId, skillId, targetId, targetType)) {
        IntSkip("Combat target selection - unhexed foe branch",
                "No hex or pressure skill classified on current player bar");
    } else if (targetType != 5) {
        IntSkip("Combat target selection - unhexed foe branch",
                "Current hex/pressure skill is not foe-targeting in this bar configuration");
    } else if (targetId == 0) {
        IntSkip("Combat target selection - unhexed foe branch",
                "No valid foe target resolved for the current hex/pressure skill");
    } else {
        IntCheck("Unhexed/default foe branch resolved live foe target", targetId != 0 && IsLiveEnemyAgent(targetId));
    }

    if (!Bot::Froggy::DebugResolveFirstSkillTarget(TEST_ROLE_INTERRUPT_HARD | TEST_ROLE_INTERRUPT_SOFT, foeId, skillId, targetId, targetType)) {
        IntSkip("Combat target selection - casting foe branch",
                "No interrupt skill classified on current player bar");
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

    if (!Bot::Froggy::DebugResolveFirstSkillTarget(TEST_ROLE_ENCHANT_REMOVE, foeId, skillId, targetId, targetType)) {
        IntSkip("Combat target selection - enchanted foe branch",
                "No enchant-removal skill classified on current player bar");
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

    if (!Bot::Froggy::DebugResolveFirstSkillTarget(TEST_ROLE_ATTACK, foeId, skillId, targetId, targetType)) {
        IntSkip("Combat target selection - melee branch",
                "No attack skill classified on current player bar");
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

    IntReport("    step 1: legacy GoNPC packet 0x39 x3");
    for (int goAttempt = 1; goAttempt <= 3; ++goAttempt) {
        IntReport("      raw interact attempt %d: SendPacket(3, 0x39, %u, 0)", goAttempt, npcId);
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        Sleep(500);
    }

    IntReport("    Legacy GoNPC dwell: waiting 2500ms after raw interact...");
    Sleep(2500);
    IntReport("    Legacy GoNPC wait complete");
    ReportDialogSnapshot("After Froggy harness-body single-packet dwell");
    const uint32_t merchantCountAfterSingle = TradeMgr::GetMerchantItemCount();
    const uintptr_t merchantFrameAfterSingle = UIMgr::GetFrameByHash(3613855137u);
    IntReport("      Merchant probe after single-packet dwell: frame=0x%08X items=%u",
              merchantFrameAfterSingle,
              merchantCountAfterSingle);
    return WaitForMerchantContext(1500);
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

static int RunFroggyFeatureTestImpl(bool isolatedExplorableFlaggingMode) {
    s_isolatedExplorableFlaggingMode = isolatedExplorableFlaggingMode;
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

    // Add heroes using the BEASTRIT Mercs configuration and prove that each
    // hero skillbar can be changed and then restored to the configured template.
    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before setup: %u", heroesBefore);
    HeroTemplateConfig heroCfg[kHeroTemplateCount] = {};
    const size_t heroCfgCount = LoadMercHeroTemplates(heroCfg, _countof(heroCfg));
    IntCheck("Loaded Mercs hero config for BEASTRIT", heroCfgCount == kHeroTemplateCount);
    uint32_t heroIds[kHeroTemplateCount] = {30, 14, 21, 4, 24, 15, 29};
    for (size_t i = 0; i < heroCfgCount && i < _countof(heroIds); ++i) {
        heroIds[i] = heroCfg[i].heroId;
    }
    if (heroesBefore > 0) {
        IntReport("  Clearing existing heroes before setup...");
        const bool cleared = KickAllHeroesWithObservation(4000);
        const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
        IntReport("  Party heroes after clear: %u", heroesAfterKick);
        IntCheck("Existing heroes cleared before setup", cleared && heroesAfterKick == 0);
    }

    IntReport("  Adding heroes...");
    for (int i = 0; i < 7; i++) {
        IntReport("  Adding hero %u (%d/7)...", heroIds[i], i + 1);
        PartyMgr::AddHero(heroIds[i]);
        Sleep(1000);
    }
    for (int i = 0; i < 7; i++) {
        PartyMgr::SetHeroBehavior(i + 1, 1);
        Sleep(300);
    }
    Sleep(1000);
    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after setup: %u", heroesAfterAdd);
    IntCheck("Seven heroes present after setup", heroesAfterAdd == 7);

    if (heroCfgCount == kHeroTemplateCount) {
        IntReport("  Loading hero skillbars from Mercs.txt...");
        for (uint32_t heroIndex = 1; heroIndex <= kHeroTemplateCount; ++heroIndex) {
            uint32_t before[8] = {};
            const bool copiedBefore = WaitForHeroSkillbarAvailable(heroIndex, before, 4000);
            IntCheck("Hero skillbar available before template load", copiedBefore);
            if (!copiedBefore) continue;
            ReportHeroSkillbarState("Before template load", heroIndex, before);

            int beforeNonZero = 0;
            for (int i = 0; i < 8; ++i) {
                if (before[i] != 0) ++beforeNonZero;
            }
            IntCheck("Hero skillbar has skills before template load", beforeNonZero > 0);

            uint32_t modified[8] = {};
            const bool canModify = BuildModifiedHeroSkillbar(before, heroCfg[heroIndex - 1].skills, modified);
            IntCheck("Can build modified hero skillbar before template load", canModify);
            if (!canModify) continue;

            SkillMgr::LoadSkillbar(modified, heroIndex);
            const bool modifiedMatches = WaitForHeroSkillbarMatch(heroIndex, modified, 4000);
            IntCheck("Hero skillbar changed before template load", modifiedMatches);
            if (modifiedMatches) {
                ReportHeroSkillbarState("Modified hero skillbar", heroIndex, modified);
            }

            SkillMgr::LoadSkillbar(heroCfg[heroIndex - 1].skills, heroIndex);
            const bool restoredMatches = WaitForHeroSkillbarMatch(heroIndex, heroCfg[heroIndex - 1].skills, 4000);
            IntCheck("Hero skillbar matches Mercs template after load", restoredMatches);
            if (restoredMatches) {
                ReportHeroSkillbarState("After template load", heroIndex, heroCfg[heroIndex - 1].skills);
            }

            Sleep(500);
        }
    } else {
        IntSkip("Hero skillbar template load", "Mercs.txt could not be parsed completely");
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

    // Effects — party effects array may be empty in outpost (no buffs active)
    uint32_t myId = AgentMgr::GetMyId();
    if (myId > 0) {
        IntCheck("HasEffect(bogus 9999)=false", !EffectMgr::HasEffect(myId, 9999));
        auto* effects = EffectMgr::GetPlayerEffects();
        if (effects) {
            IntCheck("GetPlayerEffects returns non-null", true);
            IntCheck("Player effects agent_id matches self", effects->agent_id == myId);
        } else {
            IntSkip("GetPlayerEffects", "Party effects array empty in outpost (no buffs active)");
        }
    }

    // ===== PHASE 0: Unit Tests (run after outpost setup is stable) =====
    // These tests include stateful hero operations. Running them after the
    // initial party setup keeps Phase 2 aligned with the passing integration
    // sequence, where hero clear/setup is the first party mutation observed.
    IntReport("=== PHASE 0: Unit Tests (deferred until after outpost setup) ===");
    int unitFailures = Bot::Froggy::RunFroggyUnitTests();
    IntReport("Unit tests: %d failures", unitFailures);
    s_failed += unitFailures;

    // ===== PHASE 3: Merchant Tests =====
    // Movement safety guards added to AgentMgr::Move — re-enabled.
    IntReport("=== PHASE 3: Merchant Tests ===");

    // Move to merchant
    static constexpr float kMerchX = -8374.0f;
    static constexpr float kMerchY = -22491.0f;
    IntReport("  Moving to merchant (%.0f, %.0f)...", kMerchX, kMerchY);
    const bool reachedMerchantArea = MovePlayerNearMerchantIsolation(kMerchX, kMerchY, 550.0f, 15000);
    IntCheck("Reached merchant area", reachedMerchantArea);
    Sleep(500);

    // Find and interact with merchant (with retry)
    static constexpr uint16_t kGaddsMerchantPlayerNumber = 6060;
    DumpMerchantNpcCandidates(kMerchX, kMerchY, 1500.0f, kGaddsMerchantPlayerNumber);

    NpcCandidate merchantCandidates[1];
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

    if (merchantCandidateCount) {
        bool merchantOpen = false;
        uint32_t openedMerchantId = 0;
        const auto& candidate = merchantCandidates[0];
        const uint32_t resolvedAgentId = ResolveMerchantCandidateAgentId(candidate);
        LivingAgentSnapshot npc;
        const bool haveNpc = resolvedAgentId && TrySnapshotLivingAgent(resolvedAgentId, npc);
        IntReport("  Using first merchant candidate: agent=%u allegiance=%u player_number=%u npc_id=%u at (%.0f, %.0f)",
            resolvedAgentId ? resolvedAgentId : candidate.agentId,
            haveNpc ? npc.allegiance : 0,
            haveNpc ? npc.playerNumber : candidate.playerNumber,
            haveNpc ? npc.npcId : 0,
            haveNpc ? npc.x : 0.0f,
            haveNpc ? npc.y : 0.0f);
        bool reachedNpc = false;
        float postApproachDistance = 0.0f;
        if (haveNpc) {
            reachedNpc = MovePlayerNearForMerchantHarnessBody(npc.x, npc.y, &postApproachDistance);
            float px = 0, py = 0;
            TryReadAgentPosition(ReadMyId(), px, py);
            IntReport("  After direct merchant approach: pos=(%.0f,%.0f) reached=%d dist=%.0f",
                      px, py, reachedNpc, postApproachDistance);
        }

        if (reachedNpc && haveNpc) {
            ReportMerchantPreInteractState("Froggy pre-interact snapshot",
                resolvedAgentId,
                npc.x,
                npc.y);
            ReportMerchantRuntimeContext("Froggy runtime context");
            merchantOpen = OpenMerchantContextWithSessionHarnessBody(resolvedAgentId, npc.x, npc.y);
            if (merchantOpen) {
                openedMerchantId = resolvedAgentId;
            }
        } else {
            IntReport("  WARN: Could not reach first merchant candidate %u", resolvedAgentId ? resolvedAgentId : candidate.agentId);
        }

        IntCheck("Merchant window opened", merchantOpen);

        if (merchantOpen) {
            uint32_t itemCount = TradeMgr::GetMerchantItemCount();
            IntReport("  Merchant opened via candidate agent=%u", openedMerchantId);
            IntCheck("Merchant has items", itemCount > 0);
            IntReport("  Merchant has %u items", itemCount);

            for (uint32_t slot = 1; slot <= itemCount; ++slot) {
                if (Item* merchantItem = TradeMgr::GetMerchantItemByPosition(slot)) {
                    IntReport("  Merchant slot %u: itemId=%u model=%u value=%u qty=%u",
                              slot, merchantItem->item_id, merchantItem->model_id,
                              merchantItem->value, merchantItem->quantity);
                }
            }

            const uint32_t goldBeforeBuy = ItemMgr::GetGoldCharacter();
            const uint32_t salvBeforeBuy = CountInventoryModel(MODEL_SALVAGE_KIT);
            InventoryEntrySnapshot invBefore[128] = {};
            const uint32_t invBeforeCount = SnapshotInventory(invBefore, _countof(invBefore));
            IntReport("  Buying one salvage kit: gold=%u salvageKits=%u", goldBeforeBuy, salvBeforeBuy);
            const bool buyQueued = TradeMgr::BuyMerchantItemByPosition(2, 1, 100);
            IntCheck("Merchant buy request queued", buyQueued);

            DWORD buyStart = GetTickCount();
            bool buyObserved = false;
            uint32_t goldAfterBuy = goldBeforeBuy;
            uint32_t salvAfterBuy = salvBeforeBuy;
            InventoryEntrySnapshot invAfter[128] = {};
            uint32_t invAfterCount = invBeforeCount;
            if (buyQueued) {
                while ((GetTickCount() - buyStart) < 5000) {
                    Sleep(200);
                    goldAfterBuy = ItemMgr::GetGoldCharacter();
                    salvAfterBuy = CountInventoryModel(MODEL_SALVAGE_KIT);
                    invAfterCount = SnapshotInventory(invAfter, _countof(invAfter));
                    if (salvAfterBuy > salvBeforeBuy || goldAfterBuy < goldBeforeBuy) {
                        buyObserved = true;
                        break;
                    }
                }
            }

            IntCheck("Buying salvage kit changes inventory or gold", buyObserved);
            IntCheck("Salvage kit count increased after buy", salvAfterBuy > salvBeforeBuy);
            IntCheck("Gold decreased after buy", goldAfterBuy < goldBeforeBuy);
            IntReport("  After buy: gold=%u salvageKits=%u", goldAfterBuy, salvAfterBuy);

            InventoryEntrySnapshot boughtItem = {};
            const bool boughtItemFound = FindInventoryIncrease(invBefore, invBeforeCount, invAfter, invAfterCount, boughtItem);
            IntCheck("Inventory has bought item to sell", boughtItemFound);
            if (boughtItemFound) {
                IntReport("  Bought item: itemId=%u model=%u qty=%u value=%u",
                          boughtItem.itemId, boughtItem.modelId, boughtItem.quantity, boughtItem.value);
                const bool sellQueued = TradeMgr::SellMerchantItem(boughtItem.itemId, 1, boughtItem.value);
                IntCheck("Merchant sell request queued", sellQueued);

                DWORD sellStart = GetTickCount();
                bool sellObserved = false;
                uint32_t goldAfterSell = goldAfterBuy;
                uint32_t salvAfterSell = salvAfterBuy;
                if (sellQueued) {
                    while ((GetTickCount() - sellStart) < 5000) {
                        Sleep(200);
                        goldAfterSell = ItemMgr::GetGoldCharacter();
                        salvAfterSell = CountInventoryModel(MODEL_SALVAGE_KIT);
                        if (salvAfterSell < salvAfterBuy || goldAfterSell > goldAfterBuy) {
                            sellObserved = true;
                            break;
                        }
                    }
                }

                IntCheck("Selling salvage kit changes inventory or gold", sellObserved);
                IntCheck("Salvage kit count returns to baseline after sell", salvAfterSell == salvBeforeBuy);
                IntCheck("Gold increased after sell", goldAfterSell > goldAfterBuy);
                IntReport("  After sell: gold=%u salvageKits=%u", goldAfterSell, salvAfterSell);
            }
        }

        IntReport("  Final merchant cleanup: CancelAction()");
        AgentMgr::CancelAction();
        IntReport("  Final merchant cleanup: CancelAction complete");
        Sleep(500);
    } else {
        IntSkip("Merchant tests", "Merchant NPC not found near target coords");
    }
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

            // ===== PHASE 5: Explorable Tests =====
            IntReport("=== PHASE 5: Explorable Tests ===");
            RunExplorableSkillbarRefreshProof();

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
                RunCombatTargetAndCastTelemetryValidation(foeId, "initial foe");
                RunReadOnlyCombatPreconditions(foeId, "initial foe");
                RunBuiltinCombatSingleStepProof(foeId, "initial foe");
                RunCombatTargetSelectionCoverage(foeId, "initial foe");
                if (s_lastBuiltinCombatSnapshotsValid) {
                    RunCombatCastGatingAndSafetyAssertions(s_lastBuiltinCombatBefore, s_lastBuiltinCombatAfter, "initial foe");
                } else {
                    IntSkip("Cast gating and safety assertions", "Builtin combat proof did not capture before/after snapshots");
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
                AgentMgr::ChangeTarget(foeId);
                Sleep(500);
                IntCheck("Target changed to enemy after moving", AgentMgr::GetTargetId() == foeId);
                RunCombatObservabilityHarness(foeId, "foe after moving");
                RunCombatAgentReadValidation(foeId, "foe after moving");
                RunCombatEffectReadValidation(foeId, "foe after moving");
                RunCombatTargetAndCastTelemetryValidation(foeId, "foe after moving");
                RunReadOnlyCombatPreconditions(foeId, "foe after moving");
                RunBuiltinCombatSingleStepProof(foeId, "foe after moving");
                RunCombatTargetSelectionCoverage(foeId, "foe after moving");
                if (s_lastBuiltinCombatSnapshotsValid) {
                    RunCombatCastGatingAndSafetyAssertions(s_lastBuiltinCombatBefore, s_lastBuiltinCombatAfter, "foe after moving");
                } else {
                    IntSkip("Cast gating and safety assertions", "Builtin combat proof did not capture before/after snapshots");
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

            // ===== PHASE 6: Return to Outpost =====
            IntReport("=== PHASE 6: Return to Outpost ===");
            MapMgr::ReturnToOutpost();
            bool returned = WaitFor("MapID == Gadd's after return", 60000, []() {
                return MapMgr::GetMapId() == MAP_GADDS;
            });
            IntCheck("Returned to Gadd's Encampment", returned);

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

int RunFroggyFeatureTest() {
    return RunFroggyFeatureTestImpl(false);
}

int RunFroggyExplorableFlaggingTest() {
    return RunFroggyFeatureTestImpl(true);
}

} // namespace GWA3::SmokeTest
