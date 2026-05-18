#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/TravelMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/core/Memory.h>
#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/packets/CtoS.h>

#include <algorithm>
#include <cwchar>

namespace {

bool g_test_hard_mode_enabled = false;
uint32_t g_test_party_hero_ids[16] = {};
uint32_t g_test_party_hero_count = 0u;
uint32_t g_test_party_hero_behaviors[16] = {};
GWA3::HeroPartyMember g_test_party_hero_members[16] = {};
GWA3::PartyInfo g_test_party_info = {};
wchar_t g_test_player_name[64] = {};

constexpr uint32_t kTestHeroAgentBase = 0x7000u;

} // namespace

namespace GWA3::TestStubs::AgentMgr {
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
}

namespace GWA3::MapMgr {

void SetHardMode(bool enabled) {
    g_test_hard_mode_enabled = enabled;
}

} // namespace GWA3::MapMgr

namespace GWA3::TestStubs::MapMgr {

void SetHardModeEnabled(bool enabled) {
    g_test_hard_mode_enabled = enabled;
}

bool HardModeEnabled() {
    return g_test_hard_mode_enabled;
}

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::PartyMgr {

void AddHero(uint32_t heroId) {
    if (heroId == 0u || g_test_party_hero_count >= 16u) {
        return;
    }
    const uint32_t slot = g_test_party_hero_count;
    const uint32_t agentId = kTestHeroAgentBase + slot + 1u;
    g_test_party_hero_ids[slot] = heroId;
    g_test_party_hero_behaviors[slot] = 0u;
    g_test_party_hero_members[slot] = {};
    g_test_party_hero_members[slot].agent_id = agentId;
    g_test_party_hero_members[slot].owner_player_id = 1u;
    g_test_party_hero_members[slot].hero_id = heroId;
    g_test_party_hero_members[slot].level = 20u;
    ++g_test_party_hero_count;

    if (auto* existing = GWA3::AgentMgr::GetAgentByID(agentId)) {
        auto* living = static_cast<GWA3::AgentLiving*>(existing);
        living->type = 0xDBu;
        living->allegiance = 1u;
        living->level = 20u;
        living->hp = 1.0f;
        living->primary = 1u;
        living->secondary = 0u;
    } else {
        GWA3::TestStubs::AgentMgr::AddNpc(agentId, 0.0f, 0.0f, 1u, 1.0f);
        if (auto* added = GWA3::AgentMgr::GetAgentByID(agentId)) {
            auto* living = static_cast<GWA3::AgentLiving*>(added);
            living->level = 20u;
            living->primary = 1u;
        }
    }
}

void KickHero(uint32_t heroId) {
    for (uint32_t i = 0u; i < g_test_party_hero_count; ++i) {
        if (g_test_party_hero_ids[i] != heroId) {
            continue;
        }
        for (uint32_t j = i + 1u; j < g_test_party_hero_count; ++j) {
            g_test_party_hero_ids[j - 1u] = g_test_party_hero_ids[j];
            g_test_party_hero_behaviors[j - 1u] = g_test_party_hero_behaviors[j];
            g_test_party_hero_members[j - 1u] = g_test_party_hero_members[j];
        }
        g_test_party_hero_ids[g_test_party_hero_count - 1u] = 0u;
        g_test_party_hero_behaviors[g_test_party_hero_count - 1u] = 0u;
        g_test_party_hero_members[g_test_party_hero_count - 1u] = {};
        if (g_test_party_hero_count > 0u) {
            --g_test_party_hero_count;
        }
        break;
    }
}

void KickAllHeroes() {
    for (uint32_t i = 0u; i < 16u; ++i) {
        g_test_party_hero_ids[i] = 0u;
        g_test_party_hero_behaviors[i] = 0u;
        g_test_party_hero_members[i] = {};
    }
    g_test_party_info = {};
    g_test_party_hero_count = 0u;
}

void SetHeroBehavior(uint32_t heroIndex, uint32_t behavior) {
    if (heroIndex == 0u || heroIndex > 16u) {
        return;
    }
    g_test_party_hero_behaviors[heroIndex - 1u] = behavior;
}

uint32_t CountPartyHeroes() {
    return g_test_party_hero_count;
}

bool GetIsHardMode() {
    return g_test_hard_mode_enabled;
}

uintptr_t ResolvePartyContext() {
    return 0u;
}

PartyInfo* ResolvePlayerParty() {
    g_test_party_info.heroes.buffer = g_test_party_hero_members;
    g_test_party_info.heroes.size = g_test_party_hero_count;
    g_test_party_info.heroes.capacity = 16u;
    return &g_test_party_info;
}

size_t GetPartyHeroIds(uint32_t* out, size_t maxCount) {
    if (!out || maxCount == 0u) {
        return 0u;
    }

    const size_t count = std::min<std::size_t>(g_test_party_hero_count, maxCount);
    for (size_t i = 0; i < count; ++i) {
        out[i] = g_test_party_hero_ids[i];
    }
    return count;
}

void DebugDumpPartyState(const char*) {
}

} // namespace GWA3::PartyMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetHeroes() {
    GWA3::PartyMgr::KickAllHeroes();
}

uint32_t HeroIdAt(uint32_t index) {
    return index < 16u ? g_test_party_hero_ids[index] : 0u;
}

uint32_t HeroBehaviorAt(uint32_t index) {
    return index < 16u ? g_test_party_hero_behaviors[index] : 0u;
}

} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::PlayerMgr {

wchar_t* GetPlayerName(uint32_t playerId) {
    return playerId == 0u ? g_test_player_name : nullptr;
}

} // namespace GWA3::PlayerMgr

namespace GWA3::Offsets {

uintptr_t MyID = 0u;

uintptr_t ResolveWorldContext() {
    return 0u;
}

} // namespace GWA3::Offsets

namespace GWA3::ChatMgr {

uint32_t GetPing() {
    return 0u;
}

} // namespace GWA3::ChatMgr

namespace GWA3::UIMgr {

bool ButtonClickByHash(uint32_t) {
    return true;
}

} // namespace GWA3::UIMgr

namespace GWA3::CtoS {

bool Initialize() {
    return true;
}

bool SendPacketBotshub(uint32_t size, uint32_t header, ...) {
    SendPacket(size, header, 0u, 0u);
    return true;
}

void MapTravel(uint32_t mapId, uint32_t region, uint32_t district, uint32_t language) {
    (void)mapId;
    (void)region;
    (void)district;
    (void)language;
}

} // namespace GWA3::CtoS

namespace GWA3::SkillMgr {

void SetRestrictedMapPlayerUseSkillOverride(bool) {
}

} // namespace GWA3::SkillMgr

namespace GWA3::MaintenanceMgr {

bool IsRareSkin(uint32_t) {
    return false;
}

bool NeedsMaintenance(const Config&) {
    return false;
}

void DepositGold(uint32_t) {
}

uint32_t CountFreeSlots() {
    return DungeonInventory::CountFreeSlots();
}

uint32_t CountItemByModel(uint32_t modelId) {
    return DungeonInventory::CountItemByModel(modelId);
}

uint32_t CountItemByModelInStorage(uint32_t) {
    return 0u;
}

bool HasCharacterConsetSet(uint32_t) {
    return false;
}

bool NeedsCharacterConsetRestock(const Config&) {
    return false;
}

uint32_t WithdrawMissingConsetsFromStorage(uint32_t) {
    return 0u;
}

uint32_t SellEmergencyItemsForFreeSlots(uint32_t minFreeSlots) {
    DungeonInventory::EmergencyFreeSlotOptions options;
    options.allow_green_items = true;
    return DungeonInventory::EnsureEmergencyFreeSlots(minFreeSlots, options);
}

void OpenXunlaiChest(float, float) {
}

uint32_t IdentifyAndSalvageGoldItems() {
    return 0u;
}

uint32_t IdentifyAndSalvageGoldItems(const Config&) {
    return 0u;
}

void PerformMaintenance(const Config&) {
}

} // namespace GWA3::MaintenanceMgr

namespace GWA3::AgentMgr {

bool IsCombatCommandSafe() {
    return true;
}

void InteractNPCEx(uint32_t agentId, NpcInteractMode) {
    InteractNPC(agentId);
}

} // namespace GWA3::AgentMgr

namespace GWA3::MapMgr {

uint32_t GetRegion() {
    return 0u;
}

uint32_t GetDistrict() {
    return 0u;
}

bool IsTravelSettling(uint32_t) {
    return false;
}

} // namespace GWA3::MapMgr

namespace GWA3::PlayerMgr {

Title* GetTitleTrack(uint32_t) {
    return nullptr;
}

} // namespace GWA3::PlayerMgr

namespace GWA3::Memory {

Patch::Patch()
    : address(0u),
      patchedBytes(nullptr),
      originalBytes(nullptr),
      size(0u),
      enabled(false),
      staged(false) {
}

Patch::~Patch() = default;

Patch::Patch(Patch&& other) noexcept
    : address(other.address),
      patchedBytes(other.patchedBytes),
      originalBytes(other.originalBytes),
      size(other.size),
      enabled(other.enabled),
      staged(other.staged) {
    other.address = 0u;
    other.patchedBytes = nullptr;
    other.originalBytes = nullptr;
    other.size = 0u;
    other.enabled = false;
    other.staged = false;
}

Patch& Patch::operator=(Patch&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    address = other.address;
    patchedBytes = other.patchedBytes;
    originalBytes = other.originalBytes;
    size = other.size;
    enabled = other.enabled;
    staged = other.staged;
    other.address = 0u;
    other.patchedBytes = nullptr;
    other.originalBytes = nullptr;
    other.size = 0u;
    other.enabled = false;
    other.staged = false;
    return *this;
}

bool Patch::SetPatch(uintptr_t addr, const uint8_t*, uint32_t len) {
    address = addr;
    size = len;
    staged = true;
    return true;
}

bool Patch::SetRedirect(uintptr_t addr, uintptr_t) {
    address = addr;
    size = 5u;
    staged = true;
    return true;
}

bool Patch::Enable() {
    enabled = true;
    return true;
}

bool Patch::Disable() {
    enabled = false;
    return true;
}

bool Patch::Toggle() {
    enabled = !enabled;
    return enabled;
}

Patch& GetCameraUnlockPatch() {
    static Patch patch;
    return patch;
}

Patch& GetLevelDataBypassPatch() {
    static Patch patch;
    return patch;
}

Patch& GetMapPortBypassPatch() {
    static Patch patch;
    return patch;
}

} // namespace GWA3::Memory

namespace GWA3::Offsets {

uintptr_t ValidateAsyncDecodeStr = 0u;
uintptr_t ValidateAsyncDecodeStrGwca = 0u;

uint32_t ResolveGameContext() {
    return 0u;
}

} // namespace GWA3::Offsets

namespace GWA3::TestStubs::PlayerMgr {

void SetPlayerName(const wchar_t* name) {
    g_test_player_name[0] = L'\0';
    if (!name) {
        return;
    }
    wcsncpy_s(g_test_player_name, name, _TRUNCATE);
}

} // namespace GWA3::TestStubs::PlayerMgr
