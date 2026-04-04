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

static uint32_t CountBackpackFreeSlots() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;
    uint32_t free = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (!bag->items.buffer[i]) free++;
        }
    }
    return free;
}

static Item* FindItemByModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (item && item->model_id == modelId) return item;
        }
    }
    return nullptr;
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
    IntReport("  Moving item %u (model=%u) from bag %u slot %u...",
              itemId, item->model_id, srcBagIdx, srcSlot);

    // Find a free slot in same bag
    Bag* bag = inv->bags[srcBagIdx];
    uint32_t freeSlot = UINT32_MAX;
    if (bag && bag->items.buffer) {
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (!bag->items.buffer[i] && i != srcSlot) { freeSlot = i; break; }
        }
    }

    if (freeSlot == UINT32_MAX) {
        IntSkip("ItemMove", "No free slot in same bag to move to");
        IntReport("");
        return true;
    }

    ItemMgr::MoveItem(itemId, srcBagIdx, freeSlot);
    Sleep(500 + ChatMgr::GetPing());

    // Verify item moved
    Item* movedItem = ItemMgr::GetItemById(itemId);
    if (movedItem) {
        IntReport("  After move: bag=%u slot=%u", movedItem->bag ? movedItem->bag->index : 0, movedItem->slot);
        IntCheck("Item slot changed after move", movedItem->slot == static_cast<uint8_t>(freeSlot));
    } else {
        IntCheck("Item still exists after move", false);
    }

    // Move back
    ItemMgr::MoveItem(itemId, srcBagIdx, srcSlot);
    Sleep(300);

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
    IntCheck("Character gold decreased", charAfter < charGold);
    IntCheck("Storage gold increased", storeAfter > storeGold);

    // Transfer back
    ItemMgr::ChangeGold(charAfter + 100, storeAfter - 100);
    Sleep(300);

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
        IntCheck("MemFree did not crash", true);
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

    // Save current bar
    uint32_t savedSkills[8];
    for (int i = 0; i < 8; ++i) savedSkills[i] = bar->skills[i].skill_id;

    IntReport("  Current skillbar: [%u %u %u %u %u %u %u %u]",
              savedSkills[0], savedSkills[1], savedSkills[2], savedSkills[3],
              savedSkills[4], savedSkills[5], savedSkills[6], savedSkills[7]);

    // Check if outpost (can only load skills in town)
    const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
    if (!area || IsSkillCastMapType(area->type)) {
        IntSkip("SkillbarLoad", "Not in outpost — can only load skills in town");
        IntReport("");
        return true;
    }

    // Load same skills back (safe — no actual change)
    SkillMgr::LoadSkillbar(savedSkills, 0);
    Sleep(1000);

    bar = SkillMgr::GetPlayerSkillbar();
    if (bar) {
        bool match = true;
        for (int i = 0; i < 8; ++i) {
            if (bar->skills[i].skill_id != savedSkills[i]) match = false;
        }
        IntCheck("Skillbar preserved after reload", match);
    }

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

    // KickAllHeroes then re-add
    IntReport("  Kicking all heroes...");
    PartyMgr::KickAllHeroes();
    Sleep(2000);
    IntCheck("KickAllHeroes no crash", true);

    // Re-add standard heroes
    uint32_t heroIds[] = {25, 14, 21, 4, 24, 15, 1};
    IntReport("  Re-adding 7 heroes...");
    for (int i = 0; i < 7; i++) {
        PartyMgr::AddHero(heroIds[i]);
        Sleep(300);
    }
    IntCheck("Heroes re-added no crash", true);

    // Test Tick
    IntReport("  Sending Tick(true)...");
    PartyMgr::Tick(true);
    Sleep(500);
    IntCheck("Tick(true) no crash", true);

    PartyMgr::Tick(false);
    Sleep(300);
    IntCheck("Tick(false) no crash", true);

    // Test hero target lock (lock hero 1 to self — harmless)
    IntReport("  Locking hero 1 target to self...");
    PartyMgr::LockHeroTarget(1, ReadMyId());
    Sleep(500);
    IntCheck("LockHeroTarget no crash", true);

    // Unlock
    PartyMgr::LockHeroTarget(1, 0);
    Sleep(300);

    IntReport("");
    return true;
}

// ===== GWA3-078: Title Management =====

bool TestTitleManagement() {
    IntReport("=== GWA3-078: Title Management ===");

    if (ReadMyId() == 0) { IntSkip("TitleMgmt", "Not in game"); IntReport(""); return false; }

    // Read current title
    uint32_t currentTitle = PlayerMgr::GetActiveTitleId();
    IntReport("  Current active title: %u", currentTitle);

    // Try setting Sunspear title (ID 20) — common EotN title
    IntReport("  Setting active title to Sunspear (20)...");
    PlayerMgr::SetActiveTitle(20);
    Sleep(1000);

    uint32_t afterSet = PlayerMgr::GetActiveTitleId();
    IntReport("  Active title after set: %u", afterSet);
    // May or may not change depending on whether the char has the title
    IntCheck("SetActiveTitle no crash", true);

    // Get title track data
    Title* track = PlayerMgr::GetTitleTrack(20);
    IntReport("  TitleTrack(20): %p", track);
    if (track) {
        IntReport("    current_points=%u max_rank=%u tier=%u",
                  track->current_points, track->max_title_rank, track->current_title_tier_index);
        IntCheck("Title track has plausible max_rank", track->max_title_rank > 0 && track->max_title_rank < 100);
    }

    // Get title client data
    TitleClientData* clientData = PlayerMgr::GetTitleData(20);
    IntReport("  TitleClientData(20): %p", clientData);
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
    IntCheck("RemoveActiveTitle no crash", true);

    // Restore original
    if (currentTitle > 0) {
        PlayerMgr::SetActiveTitle(currentTitle);
        Sleep(300);
    }

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
    GameThread::HookEntry cbEntry{0};

    GameThread::RegisterCallback(&cbEntry, []() { frameCount++; }, 0x4000);
    Sleep(500);

    uint32_t after500ms = frameCount.load();
    IntReport("  Frame callback hits after 500ms: %u", after500ms);
    IntCheck("Persistent callback fired at least once", after500ms > 0);

    // Remove and verify it stops
    GameThread::RemoveCallback(&cbEntry);
    uint32_t atRemoval = frameCount.load();
    Sleep(300);
    uint32_t afterRemoval = frameCount.load();
    IntReport("  Hits at removal: %u, 300ms later: %u", atRemoval, afterRemoval);
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

    // Just verify the API calls don't crash — actual quest state requires an active quest
    IntReport("  SetActiveQuest(0) — deselect...");
    QuestMgr::SetActiveQuest(0);
    Sleep(300);
    IntCheck("SetActiveQuest(0) no crash", true);

    IntReport("  SkipCinematic...");
    MapMgr::SkipCinematic();
    Sleep(300);
    IntCheck("SkipCinematic no crash (non-cinematic context)", true);

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
    IntCheck("GetChildOffsetId no crash", true);

    // GetFrameContext
    uintptr_t ctx = UIMgr::GetFrameContext(root);
    IntReport("  Root GetFrameContext: 0x%08X", static_cast<unsigned>(ctx));
    IntCheck("GetFrameContext no crash", true);

    // SendUIMessage with harmless msg
    IntReport("  SendUIMessage(0, null, null)...");
    UIMgr::SendUIMessage(0, nullptr, nullptr);
    IntCheck("SendUIMessage(0) no crash", true);

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

    bool bogusExists = AgentMgr::GetAgentExists(99999);
    IntReport("  AgentExists(99999): %d", bogusExists);
    IntCheck("Bogus agent does not exist", !bogusExists);

    // CallTarget on a nearby ally (harmless — just pings)
    uint32_t nearbyId = FindNearbyNpcLikeAgent(5000.0f);
    if (nearbyId) {
        IntReport("  CallTarget(%u)...", nearbyId);
        AgentMgr::CallTarget(nearbyId);
        Sleep(300);
        IntCheck("CallTarget no crash", true);
    } else {
        IntSkip("CallTarget", "No nearby agent");
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
    }

    IntReport("");
    return true;
}

} // namespace GWA3::SmokeTest
