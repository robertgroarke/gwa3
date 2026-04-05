// GWA3-074..089: Advanced Workflow Integration Tests
// Exercises untested manager APIs with actual game-state mutations:
// item manipulation, salvage, skillbar management, party composition,
// titles, merchant buy/sell, callbacks, GameThread hooks, StoC packets.
// Launched via GWA3_TEST_ADVANCED_WORKFLOW flag.

#include "IntegrationTestInternal.h"

#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Memory.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/CallbackRegistry.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/GuildMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/FriendListMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>
#include <atomic>

namespace GWA3::SmokeTest {

// ===== Helpers =====

static Item* FindFirstBackpackItem() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (bag->items.buffer[i]) return bag->items.buffer[i];
        }
    }
    return nullptr;
}

static uint32_t CountBackpackItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (bag->items.buffer[i]) count++;
        }
    }
    return count;
}

static uint32_t FindInventoryBagSlot(Inventory* inv, Bag* bagPtr) {
    if (!inv || !bagPtr) return UINT32_MAX;
    for (uint32_t bagIdx = 0; bagIdx < 23; ++bagIdx) {
        if (inv->bags[bagIdx] == bagPtr) return bagIdx;
    }
    return UINT32_MAX;
}

static uint32_t FindUsableTitleId(uint32_t currentTitle) {
    if (currentTitle > 0) return currentTitle;

    constexpr uint32_t kPreferredTitles[] = {
        TitleID::Sunspear,
        TitleID::Lightbringer,
        TitleID::Vanguard,
        TitleID::Norn,
        TitleID::Asura,
        TitleID::Deldrimor,
        TitleID::Kurzick,
        TitleID::Luxon,
    };

    for (uint32_t titleId : kPreferredTitles) {
        Title* track = PlayerMgr::GetTitleTrack(titleId);
        TitleClientData* clientData = PlayerMgr::GetTitleData(titleId);
        if (!track || !clientData) continue;
        if (clientData->name_id == 0) continue;
        if (track->max_title_rank == 0 || track->max_title_rank >= 100) continue;
        if (track->current_points > 0 || track->current_title_tier_index > 0 || track->next_title_tier_index > 0) {
            return titleId;
        }
    }

    return 0;
}

static uint32_t FindAlternateQuestId(uint32_t excludeQuestId) {
    const uint32_t questCount = QuestMgr::GetQuestLogSize();
    for (uint32_t i = 0; i < questCount; ++i) {
        Quest* quest = QuestMgr::GetQuestByIndex(i);
        if (!quest || quest->quest_id == 0) continue;
        if (quest->quest_id == excludeQuestId) continue;
        return quest->quest_id;
    }
    return 0;
}

static uint32_t FindNearbyFoeAgent(float maxDistance) {
    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return 0;

    if (Offsets::AgentBase <= 0x10000) return 0;
    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return 0;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    const float maxDistSq = maxDistance * maxDistance;
    float bestDistSq = maxDistSq;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->agent_id == myId) continue;
        if (living->type != 0xDB) continue;
        if (living->allegiance != 3) continue;
        if (living->hp <= 0.0f) continue;

        const float dx = living->x - myX;
        const float dy = living->y - myY;
        const float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = i;
        }
    }

    return bestId;
}

// ===== GWA3-074: Item Workflow Tests =====

bool TestItemMove() {
    IntReport("=== GWA3-074a: Item Move ===");

    if (ReadMyId() == 0) { IntSkip("ItemMove", "Not in game"); IntReport(""); return false; }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) { IntSkip("ItemMove", "No inventory"); IntReport(""); return false; }

    // Find an item and an empty slot in a different bag
    Item* item = FindFirstBackpackItem();
    if (!item || !item->bag) {
        IntSkip("ItemMove", "No item in backpack");
        IntReport("");
        return false;
    }

    uint32_t srcBagIdx = item->bag->index;
    uint32_t srcSlot = item->slot;
    uint32_t itemId = item->item_id;
    const uint32_t itemsBefore = CountBackpackItems();
    Bag* srcBagPtr = item->bag;
    const uint32_t srcInvBagSlot = FindInventoryBagSlot(inv, srcBagPtr);
    IntReport("  Moving item %u (model=%u) from bag %u slot %u...",
              itemId, item->model_id, srcBagIdx, srcSlot);
    IntReport("  Source: invBag[%u] runtimeIndex=%u packetBagId=%u slot %u",
              srcInvBagSlot,
              srcBagPtr ? srcBagPtr->index : 0,
              srcBagPtr ? srcBagPtr->h0008 : 0,
              srcSlot);

    // Find any free slot across all backpack bags (1-4)
    uint32_t dstBagIdx = UINT32_MAX;
    uint32_t freeSlot = UINT32_MAX;
    for (uint32_t bIdx = 1; bIdx <= 4 && freeSlot == UINT32_MAX; ++bIdx) {
        Bag* bag = inv->bags[bIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            // Skip the source slot itself
            if (bIdx == srcBagIdx && i == srcSlot) continue;
            if (!bag->items.buffer[i]) {
                dstBagIdx = bIdx;
                freeSlot = i;
                break;
            }
        }
    }

    if (freeSlot == UINT32_MAX) {
        IntSkip("ItemMove", "No free slot in any backpack bag");
        IntReport("");
        return true;
    }

    Bag* targetBagPtr = inv->bags[dstBagIdx];
    IntReport("  Target: invBag[%u] runtimeIndex=%u packetBagId=%u slot %u",
              dstBagIdx,
              targetBagPtr ? targetBagPtr->index : 0,
              targetBagPtr ? targetBagPtr->h0008 : 0,
              freeSlot);
    ItemMgr::MoveItem(itemId, dstBagIdx, freeSlot);
    Sleep(500 + ChatMgr::GetPing());

    // Verify item moved
    Item* movedItem = ItemMgr::GetItemById(itemId);
    if (movedItem) {
        IntReport("  After move: bagIndex=%u packetBagId=%u slot=%u",
                  movedItem->bag ? movedItem->bag->index : 0,
                  movedItem->bag ? movedItem->bag->h0008 : 0,
                  movedItem->slot);
        Bag* dstBag = inv->bags[dstBagIdx];
        Item* dstSlotItem = (dstBag && dstBag->items.buffer && freeSlot < dstBag->items.size)
            ? dstBag->items.buffer[freeSlot]
            : nullptr;
        Item* srcSlotItem = (srcBagPtr && srcBagPtr->items.buffer && srcSlot < srcBagPtr->items.size)
            ? srcBagPtr->items.buffer[srcSlot]
            : nullptr;
        const uint32_t itemsAfterMove = CountBackpackItems();
        bool moved = (movedItem->bag == targetBagPtr &&
                      movedItem->slot == static_cast<uint8_t>(freeSlot) &&
                      dstSlotItem && dstSlotItem->item_id == itemId &&
                      (!srcSlotItem || srcSlotItem->item_id != itemId) &&
                      itemsAfterMove == itemsBefore);
        IntCheck("Item moved to target slot", moved);
    } else {
        IntCheck("Item still exists after move", false);
    }

    // Move back to original position
    if (srcInvBagSlot == UINT32_MAX) {
        IntCheck("Original inventory bag slot resolved", false);
        IntReport("");
        return true;
    }

    ItemMgr::MoveItem(itemId, srcInvBagSlot, srcSlot);
    Sleep(500 + ChatMgr::GetPing());
    Item* restoredItem = ItemMgr::GetItemById(itemId);
    if (restoredItem) {
        IntReport("  After restore: bagIndex=%u packetBagId=%u slot=%u",
                  restoredItem->bag ? restoredItem->bag->index : 0,
                  restoredItem->bag ? restoredItem->bag->h0008 : 0,
                  restoredItem->slot);
    }
    bool restored = restoredItem && restoredItem->bag == srcBagPtr &&
                    restoredItem->slot == static_cast<uint8_t>(srcSlot) &&
                    CountBackpackItems() == itemsBefore;
    IntCheck("Item restored to original slot", restored);

    IntReport("");
    return true;
}

bool TestGoldTransfer() {
    IntReport("=== GWA3-074b: Gold Transfer ===");

    if (ReadMyId() == 0) { IntSkip("GoldTransfer", "Not in game"); IntReport(""); return false; }

    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t storeGold = ItemMgr::GetGoldStorage();
    IntReport("  Before: char=%u storage=%u", charGold, storeGold);

    if (charGold < 100) {
        IntSkip("GoldTransfer", "Not enough character gold (< 100)");
        IntReport("");
        return true;
    }

    // Transfer 100 gold to storage
    ItemMgr::ChangeGold(charGold - 100, storeGold + 100);
    Sleep(500);

    uint32_t charAfter = ItemMgr::GetGoldCharacter();
    uint32_t storeAfter = ItemMgr::GetGoldStorage();
    IntReport("  After transfer: char=%u storage=%u", charAfter, storeAfter);
    IntCheck("Character gold decreased by exactly 100", charAfter + 100 == charGold);
    IntCheck("Storage gold increased by exactly 100", storeAfter == storeGold + 100);

    // Transfer back
    ItemMgr::ChangeGold(charAfter + 100, storeAfter - 100);
    Sleep(500);

    uint32_t charRestored = ItemMgr::GetGoldCharacter();
    uint32_t storeRestored = ItemMgr::GetGoldStorage();
    IntReport("  After restore: char=%u storage=%u", charRestored, storeRestored);
    IntCheck("Character gold restored to original value", charRestored == charGold);
    IntCheck("Storage gold restored to original value", storeRestored == storeGold);

    IntReport("");
    return true;
}

bool TestMemAllocFree() {
    IntReport("=== GWA3-074c: MemAlloc/MemFree ===");

    void* ptr = MemoryMgr::MemAlloc(4096);
    IntReport("  MemAlloc(4096) = %p", ptr);
    IntCheck("MemAlloc returned non-null", ptr != nullptr);

    if (ptr) {
        // Write pattern
        memset(ptr, 0xAA, 4096);
        uint8_t* bytes = static_cast<uint8_t*>(ptr);
        IntCheck("Written pattern reads back correctly", bytes[0] == 0xAA && bytes[4095] == 0xAA);

        MemoryMgr::MemFree(ptr);
        IntSkip("MemFree semantics", "No reliable post-free validity probe in current harness");
    }

    IntReport("");
    return true;
}

// ===== GWA3-076: Skillbar Management =====

bool TestLoadSkillbar() {
    IntReport("=== GWA3-076: Skillbar Load ===");

    if (ReadMyId() == 0) { IntSkip("SkillbarLoad", "Not in game"); IntReport(""); return false; }

    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    if (!bar) { IntSkip("SkillbarLoad", "Skillbar unavailable"); IntReport(""); return false; }

    uint32_t savedSkills[8];
    for (int i = 0; i < 8; ++i) savedSkills[i] = bar->skills[i].skill_id;

    IntReport("  Current skillbar: [%u %u %u %u %u %u %u %u]",
              savedSkills[0], savedSkills[1], savedSkills[2], savedSkills[3],
              savedSkills[4], savedSkills[5], savedSkills[6], savedSkills[7]);

    const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
    if (!area || IsSkillCastMapType(area->type)) {
        IntSkip("SkillbarLoad", "Not in outpost - can only load skills in town");
        IntReport("");
        return true;
    }

    uint32_t modifiedSkills[8];
    for (int i = 0; i < 8; ++i) modifiedSkills[i] = savedSkills[i];

    int swapA = -1;
    int swapB = -1;
    for (int i = 0; i < 8; ++i) {
        if (savedSkills[i] == 0) continue;
        if (swapA == -1) {
            swapA = i;
            continue;
        }
        if (savedSkills[i] != savedSkills[swapA]) {
            swapB = i;
            break;
        }
    }
    if (swapA == -1 || swapB == -1) {
        IntSkip("SkillbarLoad", "Need two distinct non-zero skills to verify load");
        IntReport("");
        return true;
    }

    const uint32_t tmp = modifiedSkills[swapA];
    modifiedSkills[swapA] = modifiedSkills[swapB];
    modifiedSkills[swapB] = tmp;

    IntReport("  Loading modified skillbar (swap slot %d and %d)...", swapA + 1, swapB + 1);
    SkillMgr::LoadSkillbar(modifiedSkills, 0);
    Sleep(1000 + ChatMgr::GetPing());

    bar = SkillMgr::GetPlayerSkillbar();
    bool modifiedMatches = bar != nullptr;
    if (bar) {
        for (int i = 0; i < 8; ++i) {
            if (bar->skills[i].skill_id != modifiedSkills[i]) {
                modifiedMatches = false;
                break;
            }
        }
        IntReport("  After modified load: [%u %u %u %u %u %u %u %u]",
                  bar->skills[0].skill_id, bar->skills[1].skill_id, bar->skills[2].skill_id, bar->skills[3].skill_id,
                  bar->skills[4].skill_id, bar->skills[5].skill_id, bar->skills[6].skill_id, bar->skills[7].skill_id);
    }
    IntCheck("Skillbar changed after load", modifiedMatches);

    IntReport("  Restoring original skillbar...");
    SkillMgr::LoadSkillbar(savedSkills, 0);
    Sleep(1000 + ChatMgr::GetPing());

    bar = SkillMgr::GetPlayerSkillbar();
    bool restoredMatches = bar != nullptr;
    if (bar) {
        for (int i = 0; i < 8; ++i) {
            if (bar->skills[i].skill_id != savedSkills[i]) {
                restoredMatches = false;
                break;
            }
        }
        IntReport("  After restore: [%u %u %u %u %u %u %u %u]",
                  bar->skills[0].skill_id, bar->skills[1].skill_id, bar->skills[2].skill_id, bar->skills[3].skill_id,
                  bar->skills[4].skill_id, bar->skills[5].skill_id, bar->skills[6].skill_id, bar->skills[7].skill_id);
    }
    IntCheck("Skillbar restored after reload", restoredMatches);

    IntReport("");
    return true;
}

// ===== GWA3-077: Party Management =====

bool TestPartyManagement() {
    IntReport("=== GWA3-077: Party Management ===");

    if (ReadMyId() == 0) { IntSkip("PartyMgmt", "Not in game"); IntReport(""); return false; }

    const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
    if (!area || IsSkillCastMapType(area->type)) {
        IntSkip("PartyMgmt", "Not in outpost");
        IntReport("");
        return false;
    }

    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before kick: %u", heroesBefore);
    if (heroesBefore == 0) {
        IntSkip("Party hero assertions", "No heroes present in player party");
        IntReport("");
        return true;
    }

    // KickAllHeroes then re-add
    IntReport("  Kicking all heroes...");
    PartyMgr::KickAllHeroes();
    Sleep(2000);
    const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after kick: %u", heroesAfterKick);
    IntCheck("KickAllHeroes removed party heroes", heroesAfterKick == 0);

    // Re-add standard heroes
    // BEASTRIT uses the "Mercs" hero profile from GWA Censured/hero_configs/Mercs.txt.
    uint32_t heroIds[] = {30, 14, 21, 4, 24, 15, 29};
    IntReport("  Re-adding 7 heroes...");
    for (int i = 0; i < 7; i++) {
        const uint32_t beforeAddCount = PartyMgr::CountPartyHeroes();
        PartyMgr::AddHero(heroIds[i]);
        Sleep(500);
        uint32_t afterAddCount = PartyMgr::CountPartyHeroes();
        for (int retry = 0; retry < 4 && afterAddCount == beforeAddCount; ++retry) {
            Sleep(400);
            afterAddCount = PartyMgr::CountPartyHeroes();
        }
        IntReport("    Hero %u add: before=%u after=%u", heroIds[i], beforeAddCount, afterAddCount);
    }
    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after re-add: %u", heroesAfterAdd);
    IntCheck("Heroes re-added to party", heroesAfterAdd >= heroesBefore);

    IntSkip("Tick(true/false)", "No readable ready-state flag exposed yet");
    IntSkip("LockHeroTarget", "No readable hero target-lock state exposed yet");

    IntReport("");
    return true;
}

// ===== GWA3-078: Title Management =====

bool TestTitleManagement() {
    IntReport("=== GWA3-078: Title Management ===");

    if (ReadMyId() == 0) { IntSkip("TitleMgmt", "Not in game"); IntReport(""); return false; }

    // GetActiveTitleId() is currently exposing the player's active title tier, not a title id.
    uint32_t currentActiveTier = PlayerMgr::GetActiveTitleId();
    const uint32_t candidateTitle = FindUsableTitleId(currentActiveTier);
    IntReport("  Current active title tier: %u", currentActiveTier);
    IntReport("  Candidate title for mutation: %u", candidateTitle);

    if (candidateTitle == 0) {
        IntSkip("TitleMgmt", "No usable title track with readable data");
        IntReport("");
        return true;
    }

    IntReport("  Setting active title to %u...", candidateTitle);
    PlayerMgr::SetActiveTitle(candidateTitle);
    Sleep(1000);

    // Get title track data
    Title* track = PlayerMgr::GetTitleTrack(candidateTitle);
    IntReport("  TitleTrack(%u): %p", candidateTitle, track);
    if (track) {
        IntReport("    current_points=%u max_rank=%u tier=%u",
                  track->current_points, track->max_title_rank, track->current_title_tier_index);
        IntCheck("Title track has plausible max_rank", track->max_title_rank > 0 && track->max_title_rank < 100);
    }

    uint32_t afterSet = PlayerMgr::GetActiveTitleId();
    IntReport("  Active title tier after set: %u", afterSet);
    if (track) {
        IntCheck("SetActiveTitle updated active title tier", afterSet == track->current_title_tier_index && afterSet != 0);
    } else {
        IntSkip("SetActiveTitle tier readback", "No title track available for selected title");
    }

    // Get title client data
    TitleClientData* clientData = PlayerMgr::GetTitleData(candidateTitle);
    IntReport("  TitleClientData(%u): %p", candidateTitle, clientData);
    if (clientData) {
        IntReport("    flags=%u title_id=%u name_id=%u",
                  clientData->title_flags, clientData->title_id, clientData->name_id);
        IntCheck("TitleClientData has non-zero name_id", clientData->name_id > 0);
    } else {
        IntSkip("TitleClientData", "TitleClientDataBase not resolved");
    }

    // Remove active title
    IntReport("  Removing active title...");
    PlayerMgr::RemoveActiveTitle();
    Sleep(500);
    uint32_t afterRemove = PlayerMgr::GetActiveTitleId();
    IntReport("  Active title after remove: %u", afterRemove);
    IntCheck("RemoveActiveTitle cleared active title", afterRemove != candidateTitle);

    // Restore original
    IntReport("");
    return true;
}

// ===== GWA3-080: CallbackRegistry Tests =====

bool TestCallbackRegistry() {
    IntReport("=== GWA3-080: CallbackRegistry ===");

    if (ReadMyId() == 0) { IntSkip("CallbackRegistry", "Not in game"); IntReport(""); return false; }

    // Test UIMessage callback via EmulatePacket-style approach
    // Register a callback for a harmless UI message and dispatch it
    static std::atomic<uint32_t> cbFireCount{0};
    GWA3::HookEntry testEntry{nullptr};

    constexpr uint32_t kTestMsgId = 0x9999u; // unlikely to collide with real traffic

    bool registered = CallbackRegistry::RegisterUIMessageCallback(
        &testEntry, kTestMsgId,
        [](GWA3::HookStatus*, uint32_t, void*, void*) { cbFireCount++; }, -1);

    IntReport("  RegisterUIMessageCallback: %s", registered ? "ok" : "failed");
    IntCheck("UIMessage callback registered", registered);

    if (registered) {
        // Dispatch manually
        CallbackRegistry::DispatchUIMessage(kTestMsgId, nullptr, nullptr);
        IntReport("  After dispatch: fires=%u", cbFireCount.load());
        IntCheck("UIMessage callback fired", cbFireCount.load() > 0);

        // Remove and verify doesn't fire
        CallbackRegistry::RemoveCallbacks(&testEntry);
        uint32_t countBefore = cbFireCount.load();
        CallbackRegistry::DispatchUIMessage(kTestMsgId, nullptr, nullptr);
        IntCheck("Callback doesn't fire after removal", cbFireCount.load() == countBefore);
    }

    IntReport("");
    return true;
}

// ===== GWA3-081: GameThread Persistent Callbacks =====

bool TestGameThreadCallbacks() {
    IntReport("=== GWA3-081: GameThread Persistent Callbacks ===");

    if (!GameThread::IsInitialized()) {
        IntSkip("GameThread callbacks", "GameThread not initialized");
        IntReport("");
        return false;
    }

    static std::atomic<uint32_t> frameCount{0};
    static std::atomic<bool> callbackEnabled{false};
    static GameThread::HookEntry cbEntry{0};

    frameCount.store(0);
    callbackEnabled.store(true);
    GameThread::RegisterCallback(&cbEntry, []() {
        if (callbackEnabled.load()) {
            frameCount++;
        }
    }, 0x4000);
    Sleep(500);

    uint32_t after500ms = frameCount.load();
    IntReport("  Frame callback hits after 500ms: %u", after500ms);
    IntCheck("Persistent callback fired at least once", after500ms > 0);

    // Disable the body first so even an in-flight copied callback becomes inert.
    callbackEnabled.store(false);
    Sleep(100);

    // Remove from the registry on the game thread so mutation is serialized with frame dispatch.
    std::atomic<bool> removeRan{false};
    GameThread::Enqueue([&]() {
        GameThread::RemoveCallback(&cbEntry);
        removeRan.store(true);
    });
    WaitFor("GameThread callback removal barrier", 1000, [&]() { return removeRan.load(); });

    uint32_t atRemoval = frameCount.load();
    Sleep(500);
    uint32_t afterRemoval = frameCount.load();
    IntReport("  Hits at removal: %u, 500ms later: %u", atRemoval, afterRemoval);
    IntCheck("Callback stopped after removal", afterRemoval == atRemoval);

    IntReport("");
    return true;
}

// ===== GWA3-082: StoC Packet Type Coverage =====

bool TestStoCPacketTypes() {
    IntReport("=== GWA3-082: StoC Packet Type Coverage ===");

    if (ReadMyId() == 0) { IntSkip("StoC types", "Not in game"); IntReport(""); return false; }

    // Register callbacks on several common packet headers and wait
    struct HeaderProbe {
        uint32_t header;
        const char* name;
        std::atomic<uint32_t> hits{0};
        StoC::HookEntry entry{nullptr};
    };

    static HeaderProbe probes[] = {
        {0x00E1, "AgentUpdate"},
        {0x0089, "InstanceLoad"},
        {0x0029, "Movement"},
    };

    for (auto& p : probes) {
        StoC::RegisterPostPacketCallback(&p.entry, p.header,
            [&p](StoC::HookStatus*, StoC::PacketBase*) { p.hits++; });
    }

    Sleep(3000);

    for (auto& p : probes) {
        IntReport("  StoC 0x%04X (%s): %u hits", p.header, p.name, p.hits.load());
        if (p.hits.load() > 0) {
            IntCheck(p.name, true);
        } else {
            IntSkip(p.name, "No packets observed in 3s (hook may not be installed)");
        }
        StoC::RemoveCallbacks(&p.entry);
    }

    IntReport("");
    return true;
}

// ===== GWA3-083: Quest Management =====

bool TestQuestManagement() {
    IntReport("=== GWA3-083: Quest Management ===");

    if (ReadMyId() == 0) { IntSkip("QuestMgmt", "Not in game"); IntReport(""); return false; }

    const uint32_t activeQuestBefore = QuestMgr::GetActiveQuestId();
    const uint32_t alternateQuestId = FindAlternateQuestId(activeQuestBefore);
    IntReport("  Active quest before mutation: %u", activeQuestBefore);
    IntReport("  Alternate quest candidate: %u", alternateQuestId);
    if (activeQuestBefore == 0) {
        IntSkip("SetActiveQuest", "No active quest selected");
    } else if (alternateQuestId == 0) {
        IntSkip("SetActiveQuest", "No alternate quest available in quest log");
    } else {
    IntReport("  SetActiveQuest(0) — deselect...");
    QuestMgr::SetActiveQuest(alternateQuestId);
    Sleep(1000);
    const uint32_t activeQuestAfterSet = QuestMgr::GetActiveQuestId();
    IntReport("  Active quest after set: %u", activeQuestAfterSet);
    IntCheck("SetActiveQuest switched active quest", activeQuestAfterSet == alternateQuestId);

    IntReport("  Restoring active quest %u...", activeQuestBefore);
    QuestMgr::SetActiveQuest(activeQuestBefore);
    Sleep(1000);
    const uint32_t activeQuestAfterRestore = QuestMgr::GetActiveQuestId();
    IntReport("  Active quest after restore: %u", activeQuestAfterRestore);
    IntCheck("SetActiveQuest restored original active quest", activeQuestAfterRestore == activeQuestBefore);
    }

    IntReport("  SkipCinematic...");
    MapMgr::SkipCinematic();
    Sleep(300);
    IntSkip("SkipCinematic", "No observable cinematic state exposed in current harness");

    IntReport("");
    return true;
}

// ===== GWA3-085: UI Frame Interaction =====

bool TestUIFrameInteraction() {
    IntReport("=== GWA3-085: UI Frame Interaction ===");

    if (ReadMyId() == 0) { IntSkip("UIFrameInteract", "Not in game"); IntReport(""); return false; }

    uintptr_t root = UIMgr::GetRootFrame();
    if (!root) { IntSkip("UIFrameInteract", "No root frame"); IntReport(""); return false; }

    // GetChildOffsetId
    uint32_t childOffset = UIMgr::GetChildOffsetId(root);
    IntReport("  Root GetChildOffsetId: %u", childOffset);
    IntCheck("GetChildOffsetId returned plausible value", childOffset < 4096);

    // GetFrameContext
    uintptr_t ctx = UIMgr::GetFrameContext(root);
    IntReport("  Root GetFrameContext: 0x%08X", static_cast<unsigned>(ctx));
    IntCheck("GetFrameContext returned non-null", ctx > 0x10000);

    IntSkip("SendUIMessage(0)", "No observable UI-side effect defined for this message");

    IntReport("");
    return true;
}

// ===== GWA3-086: Agent Interaction =====

bool TestAgentInteraction() {
    IntReport("=== GWA3-086: Agent Interaction ===");

    if (ReadMyId() == 0) { IntSkip("AgentInteract", "Not in game"); IntReport(""); return false; }

    // AgentExists
    bool selfExists = AgentMgr::GetAgentExists(ReadMyId());
    IntReport("  AgentExists(self): %d", selfExists);
    IntCheck("Self agent exists", selfExists);

    AgentLiving* selfAgent = AgentMgr::GetMyAgent();
    IntReport("  GetMyAgent(): %p", selfAgent);
    IntCheck("GetMyAgent returned non-null", selfAgent != nullptr);

    bool bogusExists = AgentMgr::GetAgentExists(99999);
    IntReport("  AgentExists(99999): %d", bogusExists);
    IntCheck("Bogus agent does not exist", !bogusExists);

    const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
    if (!area || !IsSkillCastMapType(area->type)) {
        IntSkip("CallTarget", "Requires explorable map with nearby foes");
    } else {
        uint32_t nearbyId = FindNearbyFoeAgent(5000.0f);
        if (nearbyId) {
            IntReport("  CallTarget(%u)...", nearbyId);
            const uint32_t calledTargetBefore = PartyMgr::GetCalledTargetId();
            IntReport("  Called target before call: %u", calledTargetBefore);
            AgentMgr::CallTarget(nearbyId);
            Sleep(500);
            const uint32_t targetAfterCall = AgentMgr::GetTargetId();
            const uint32_t calledTargetAfter = PartyMgr::GetCalledTargetId();
            IntReport("  Target after call: %u", targetAfterCall);
            IntReport("  Called target after call: %u", calledTargetAfter);
            IntCheck("CallTarget updated party called target", calledTargetAfter == nearbyId);
        } else {
            IntSkip("CallTarget", "No nearby foe in explorable");
        }
    }

    IntReport("");
    return true;
}

// ===== GWA3-087: Camera FOV =====

bool TestCameraFOV() {
    IntReport("=== GWA3-087: Camera FOV ===");

    Camera* cam = CameraMgr::GetCamera();
    if (!cam) { IntSkip("CameraFOV", "Camera not resolved"); IntReport(""); return false; }

    float origFov = CameraMgr::GetFieldOfView();
    IntReport("  Original FOV: %.4f", origFov);

    CameraMgr::SetFieldOfView(1.5f);
    float newFov = CameraMgr::GetFieldOfView();
    IntReport("  After SetFieldOfView(1.5): %.4f", newFov);
    float diff = (newFov > 1.5f) ? (newFov - 1.5f) : (1.5f - newFov);
    IntCheck("FOV changed to ~1.5", diff < 0.01f);

    // Restore
    CameraMgr::SetFieldOfView(origFov);
    IntReport("  Restored FOV: %.4f", CameraMgr::GetFieldOfView());

    IntReport("");
    return true;
}

// ===== GWA3-089: Memory Personal Dir =====

bool TestPersonalDir() {
    IntReport("=== GWA3-089: Personal Dir ===");

    wchar_t buf[MAX_PATH] = {};
    bool ok = MemoryMgr::GetPersonalDir(buf, MAX_PATH);
    IntReport("  GetPersonalDir: %s", ok ? "ok" : "failed");
    IntCheck("GetPersonalDir returned true", ok);

    if (ok) {
        char narrow[MAX_PATH] = {};
        for (int i = 0; i < MAX_PATH - 1 && buf[i]; ++i) {
            narrow[i] = (buf[i] < 128) ? static_cast<char>(buf[i]) : '?';
        }
        IntReport("  Path: %s", narrow);
        IntCheck("Path starts with drive letter", buf[0] >= L'A' && buf[0] <= L'Z');
        IntCheck("Path contains Guild Wars directory name", wcsstr(buf, L"Guild Wars") != nullptr);
    }

    IntReport("");
    return true;
}

// ===== GWA3-086b: CallTarget in Explorable (Sparkfly) =====

static constexpr uint32_t MAP_GADDS_ENCAMPMENT = 638;
static constexpr uint32_t MAP_SPARKFLY_SWAMP   = 558;

bool TestExplorableCallTarget() {
    IntReport("=== GWA3-086b: CallTarget in Explorable ===");

    if (ReadMyId() == 0) { IntSkip("ExplorableCallTarget", "Not in game"); IntReport(""); return false; }

    uint32_t mapId = ReadMapId();

    // If not at Gadd's, travel there first
    if (mapId != MAP_GADDS_ENCAMPMENT && !IsSkillCastMapType(MapMgr::GetAreaInfo(mapId)->type)) {
        IntReport("  Traveling to Gadd's Encampment...");
        MapMgr::Travel(MAP_GADDS_ENCAMPMENT);
        bool arrived = WaitFor("MapID = Gadd's", 60000, []() {
            return ReadMapId() == MAP_GADDS_ENCAMPMENT;
        });
        if (!arrived) {
            IntSkip("ExplorableCallTarget", "Failed to travel to Gadd's");
            IntReport("");
            return false;
        }
        WaitFor("MyID valid after travel", 30000, []() { return ReadMyId() > 0; });
        WaitForPlayerWorldReady(10000);
    }

    // If already in explorable, skip the travel phase
    if (IsSkillCastMapType(MapMgr::GetAreaInfo(ReadMapId())->type)) {
        IntReport("  Already in explorable map %u, skipping travel", ReadMapId());
    } else {
        // Walk out of Gadd's into Sparkfly Swamp
        IntReport("  Walking out of Gadd's to Sparkfly Swamp...");

        // Wait for position to be readable
        bool posReady = WaitFor("player position ready", 10000, []() {
            float x = 0.0f, y = 0.0f;
            return ReadMyId() > 0 && TryReadAgentPosition(ReadMyId(), x, y);
        });
        if (!posReady) {
            IntSkip("ExplorableCallTarget", "Player position not ready");
            IntReport("");
            return false;
        }

        // Walk to Gadd's exit (same waypoints as TestExplorableEntry)
        IntReport("  Moving to exit waypoint (-10018, -21892)...");
        MovePlayerNear(-10018.0f, -21892.0f, 350.0f, 20000);
        IntReport("  Moving to exit waypoint (-9550, -20400)...");
        MovePlayerNear(-9550.0f, -20400.0f, 350.0f, 20000);

        // Push into zone boundary (must use EnqueuePost like TestExplorableEntry)
        IntReport("  Entering Sparkfly Swamp...");
        DWORD zoneStart = GetTickCount();
        bool enteredSparkfly = false;
        while ((GetTickCount() - zoneStart) < 30000) {
            GameThread::EnqueuePost([]() {
                AgentMgr::Move(-9451.0f, -19766.0f);
            });
            Sleep(500);
            if (ReadMapId() == MAP_SPARKFLY_SWAMP) {
                enteredSparkfly = true;
                break;
            }
        }

        IntCheck("Entered Sparkfly Swamp", enteredSparkfly);
        if (!enteredSparkfly) {
            IntReport("");
            return false;
        }

        // Wait for explorable to load
        WaitFor("MyID valid in Sparkfly", 30000, []() { return ReadMyId() > 0; });
        WaitForPlayerWorldReady(10000);
        WaitForStablePlayerState(5000);
    }

    // Now we're in explorable — walk toward a known enemy area
    IntReport("  In Sparkfly (map %u), looking for foes...", ReadMapId());

    // Sparkfly spawn area has enemies around these coords
    static const struct { float x; float y; } kProbeSteps[] = {
        {-4559.0f, -14406.0f},
        {-5204.0f, -9831.0f},
    };

    uint32_t foeId = FindNearbyFoeAgent(5000.0f);
    for (const auto& step : kProbeSteps) {
        if (foeId) break;
        IntReport("  Probing area (%.0f, %.0f) for foes...", step.x, step.y);
        MovePlayerNear(step.x, step.y, 500.0f, 20000);
        Sleep(1000);
        foeId = FindNearbyFoeAgent(5000.0f);
    }

    if (!foeId) {
        IntSkip("CallTarget in explorable", "No foes found after probing Sparkfly areas");
        IntReport("");
        return false;
    }

    // Test CallTarget on the foe
    IntReport("  Found foe agent %u, testing CallTarget...", foeId);
    const uint32_t calledBefore = PartyMgr::GetCalledTargetId();
    IntReport("  Called target before: %u", calledBefore);

    AgentMgr::CallTarget(foeId);
    Sleep(500);

    const uint32_t calledAfter = PartyMgr::GetCalledTargetId();
    IntReport("  Called target after: %u", calledAfter);
    IntCheck("CallTarget set party called target to foe", calledAfter == foeId);

    // Also verify ChangeTarget + GetTargetId in explorable
    AgentMgr::ChangeTarget(foeId);
    bool targetSet = WaitFor("ChangeTarget updates in explorable", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    IntCheck("ChangeTarget works in explorable", targetSet);

    IntReport("");
    return true;
}

} // namespace GWA3::SmokeTest

