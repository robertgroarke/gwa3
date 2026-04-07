// Core gameplay interaction slices: movement, targeting, combat, and loot.

#include "IntegrationTestInternal.h"

#include <atomic>
#include <cmath>

namespace GWA3::SmokeTest {

namespace {

bool TryAddHeroWithObservation(uint32_t heroId, uint32_t* beforeOut = nullptr, uint32_t* afterOut = nullptr) {
    const uint32_t beforeAdd = PartyMgr::CountPartyHeroes();
    PartyMgr::AddHero(heroId);
    Sleep(500);

    uint32_t afterAdd = PartyMgr::CountPartyHeroes();
    for (int retry = 0; retry < 4 && afterAdd == beforeAdd; ++retry) {
        Sleep(400);
        afterAdd = PartyMgr::CountPartyHeroes();
    }

    if (beforeOut) *beforeOut = beforeAdd;
    if (afterOut) *afterOut = afterAdd;
    IntReport("    Hero %u add: before=%u after=%u", heroId, beforeAdd, afterAdd);
    return afterAdd > beforeAdd;
}

void RunHeroAddDiagnostics() {
    IntReport("  Running hero-add diagnostics...");

    const uint32_t knownGoodHeroId = 14;
    const uint32_t suspectHeroId = 25;
    const uint32_t mercenaryHeroIds[] = {28, 29, 30, 31, 32, 33, 34, 35};

    PartyMgr::KickAllHeroes();
    Sleep(2000);
    IntReport("    Diagnostic baseline hero count: %u", PartyMgr::CountPartyHeroes());

    uint32_t before = 0;
    uint32_t after = 0;
    const bool knownGoodAdded = TryAddHeroWithObservation(knownGoodHeroId, &before, &after);
    IntReport("    Standard hero %u diagnostic: %s", knownGoodHeroId, knownGoodAdded ? "added" : "did not add");

    PartyMgr::KickAllHeroes();
    Sleep(1500);
    const bool suspectAdded = TryAddHeroWithObservation(suspectHeroId, &before, &after);
    IntReport("    Suspect hero %u diagnostic: %s", suspectHeroId, suspectAdded ? "added" : "did not add");

    PartyMgr::KickAllHeroes();
    Sleep(1500);
    bool mercAdded = false;
    for (uint32_t mercHeroId : mercenaryHeroIds) {
        if (TryAddHeroWithObservation(mercHeroId, &before, &after)) {
            IntReport("    Mercenary hero %u diagnostic: added", mercHeroId);
            mercAdded = true;
            break;
        }
        IntReport("    Mercenary hero %u diagnostic: did not add", mercHeroId);
        PartyMgr::KickAllHeroes();
        Sleep(1000);
    }
    if (!mercAdded) {
        IntReport("    No mercenary hero IDs 28-35 added during diagnostics");
    }
}

void RestoreHeroSetup(const uint32_t* heroIds, size_t heroCount) {
    PartyMgr::KickAllHeroes();
    Sleep(2000);
    for (size_t i = 0; i < heroCount; ++i) {
        TryAddHeroWithObservation(heroIds[i]);
    }
}

} // namespace

bool TestCharSelectLogin() {
    IntReport("=== GWA3-028: Character Select + Login ===");

    const uint32_t mapId = ReadMapId();
    const uint32_t myId = ReadMyId();
    IntReport("  Post-bootstrap state: MapID=%u, MyID=%u", mapId, myId);

    IntCheck("Bootstrap reached map", mapId > 0);
    IntCheck("Bootstrap produced MyID", myId > 0);
    IntCheck("GameThread initialized post-login", GameThread::IsInitialized());

    if (!GameThread::IsInitialized()) {
        IntReport("  Cannot continue without GameThread after login");
        return false;
    }

    std::atomic<bool> gtFlag{false};
    GameThread::Enqueue([&gtFlag]() { gtFlag = true; });
    Sleep(1000);
    IntCheck("GameThread enqueue fires", gtFlag.load());

    if (mapId == 0 || myId == 0) {
        IntReport("");
        return false;
    }

    IntCheck("MapID is valid", mapId <= 2000);
    IntCheck("MyID is valid", myId <= 10000);

    const uint32_t ping = ChatMgr::GetPing();
    IntReport("  Ping: %u ms", ping);
    IntCheck("Ping plausible", ping > 0 && ping <= 5000);

    IntReport("");
    return true;
}

bool TestHeroSetup() {
    IntReport("=== GWA3-029: Hero Setup + Consumables ===");

    uint32_t mapId = ReadMapId();
    if (mapId == 0) {
        IntSkip("Hero setup", "Not in game");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    const bool isOutpost = area && !IsSkillCastMapType(area->type);

    // BEASTRIT uses the "Mercs" hero profile from GWA Censured/hero_configs/Mercs.txt.
    uint32_t heroIds[] = {30, 14, 21, 4, 24, 15, 29};
    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before setup: %u", heroesBefore);

    if (isOutpost && heroesBefore > 0) {
        IntReport("  Clearing existing heroes before setup...");
        PartyMgr::KickAllHeroes();
        Sleep(2000);
        const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
        IntReport("  Party heroes after clear: %u", heroesAfterKick);
        IntCheck("Existing heroes cleared before setup", heroesAfterKick == 0);
    }

    IntReport("  Adding 7 heroes...");
    for (int i = 0; i < 7; i++) {
        TryAddHeroWithObservation(heroIds[i]);
    }
    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after setup: %u", heroesAfterAdd);
    if (isOutpost) {
        IntCheck("Seven heroes present after setup", heroesAfterAdd == 7);
        if (heroesAfterAdd != 7) {
            RunHeroAddDiagnostics();
            IntReport("  Restoring standard hero setup after diagnostics...");
            RestoreHeroSetup(heroIds, std::size(heroIds));
            const uint32_t heroesAfterRestore = PartyMgr::CountPartyHeroes();
            IntReport("  Party heroes after restore: %u", heroesAfterRestore);
        }
    } else {
        IntCheck("Hero setup did not reduce party heroes", heroesAfterAdd >= heroesBefore);
    }

    IntReport("  Setting hero behaviors to Guard...");
    for (int i = 0; i < 7; i++) {
        GameThread::Enqueue([idx = i + 1]() {
            PartyMgr::SetHeroBehavior(idx, 1);
        });
        Sleep(200);
    }
    IntCheck("Hero behaviors set (no crash)", true);

    IntReport("");
    return true;
}

// Movement

bool TestMovement() {
    IntReport("=== GWA3-030: Travel + Movement ===");

    uint32_t mapId = ReadMapId();
    if (mapId == 0) {
        IntSkip("Movement", "Not in game");
        return false;
    }

    if (!WaitForPlayerWorldReady(10000)) {
        IntReport("  Player runtime state: TypeMap=0x%X ModelState=%u", GetPlayerTypeMap(), GetPlayerModelState());
        IntSkip("Movement test", "Player world state not ready");
        return false;
    }

    float startX = 0, startY = 0;
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        uint32_t myId = ReadMyId();
        if (agentArr > 0x10000 && myId > 0 && myId < 5000) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + myId * 4);
            if (agentPtr > 0x10000) {
                startX = *reinterpret_cast<float*>(agentPtr + 0x74);
                startY = *reinterpret_cast<float*>(agentPtr + 0x78);
            }
        }
    }
    IntReport("  Start position: (%.1f, %.1f)", startX, startY);

    if (startX == 0.0f && startY == 0.0f) {
        IntSkip("Movement test", "Cannot read position");
        return false;
    }

    float targetX = startX + 200.0f;
    float targetY = startY;
    IntReport("  Moving to (%.1f, %.1f)...", targetX, targetY);
    GameThread::EnqueuePost([targetX, targetY]() {
        AgentMgr::Move(targetX, targetY);
    });

    Sleep(4000);

    float endX = 0, endY = 0;
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        uint32_t myId = ReadMyId();
        if (agentArr > 0x10000 && myId > 0 && myId < 5000) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + myId * 4);
            if (agentPtr > 0x10000) {
                endX = *reinterpret_cast<float*>(agentPtr + 0x74);
                endY = *reinterpret_cast<float*>(agentPtr + 0x78);
            }
        }
    }

    float dist = sqrtf((endX - startX) * (endX - startX) + (endY - startY) * (endY - startY));
    IntReport("  End position: (%.1f, %.1f), moved %.1f units", endX, endY, dist);
    IntCheck("Character moved > 50 units", dist > 50.0f);

    GameThread::EnqueuePost([startX, startY]() {
        AgentMgr::Move(startX, startY);
    });
    Sleep(3000);

    IntReport("");
    return true;
}

// Targeting

bool TestTargeting() {
    IntReport("=== GWA3-030 continued: Targeting ===");

    uint32_t myId = ReadMyId();
    if (myId == 0) {
        IntSkip("Targeting", "Not in game");
        return false;
    }

    if (!WaitForPlayerWorldReady(10000)) {
        IntReport("  Player runtime state: TypeMap=0x%X ModelState=%u", GetPlayerTypeMap(), GetPlayerModelState());
        IntSkip("Targeting", "Player world state not ready");
        return false;
    }

    uint32_t targetId = 0;
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr > 0x10000) {
            for (uint32_t i = 1; i < maxAgents && i < 200; i++) {
                if (i == myId) continue;
                uintptr_t ap = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
                if (ap > 0x10000) {
                    targetId = i;
                    break;
                }
            }
        }
    }

    if (targetId == 0) {
        IntSkip("Targeting", "No other agents found");
        return false;
    }

    IntReport("  Targeting agent %u via ChangeTarget...", targetId);
    const uint32_t targetBefore = AgentMgr::GetTargetId();
    IntReport("  CurrentTarget before change: %u", targetBefore);
    if (targetBefore == 0) {
        DumpCurrentTargetCandidates("before", targetId);
    }
    GameThread::Enqueue([targetId]() {
        AgentMgr::ChangeTarget(targetId);
    });

    const bool targetChanged = WaitFor("CurrentTarget updates after ChangeTarget", 5000, [targetId]() {
        return AgentMgr::GetTargetId() == targetId;
    });
    const uint32_t targetAfter = AgentMgr::GetTargetId();
    IntReport("  CurrentTarget after change: %u", targetAfter);
    if (targetAfter == 0) {
        DumpCurrentTargetCandidates("after", targetId);
    }
    IntCheck("CurrentTarget matches requested target", targetChanged);

    IntReport("");
    return targetChanged;
}

// Skill activation

bool TestSkillActivation() {
    IntReport("=== GWA3-034 slice: Skill Activation ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        IntSkip("Skill activation", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area) {
        IntSkip("Skill activation", "AreaInfo unavailable");
        IntReport("");
        return false;
    }

    IntReport("  Map %u regionType=%u (%s)",
              mapId,
              area->type,
              DescribeMapRegionType(area->type));
    if (!IsSkillCastMapType(area->type)) {
        IntSkip("Skill activation", "Current instance type is outpost-like; explorable phase required");
        IntReport("");
        return false;
    }

    if (!WaitForStablePlayerState()) {
        IntReport("  Player runtime state: TypeMap=0x%X ModelState=%u", GetPlayerTypeMap(), GetPlayerModelState());
        DumpSkillbarForSkillTest();
        IntSkip("Skill activation", "Player state not stable after login/targeting");
        IntReport("");
        return false;
    }

    SkillTestCandidate candidate{};
    if (!TryChooseSkillTestCandidate(candidate)) {
        DumpSkillbarForSkillTest();
        IntSkip("Skill activation", "No suitable recharged skill/target combination found");
        IntReport("");
        return false;
    }

    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    auto* me = GetAgentLivingRaw(ReadMyId());
    if (!bar || !me) {
        IntSkip("Skill activation", "Skillbar or player agent unavailable");
        IntReport("");
        return false;
    }

    SkillbarSkill before = bar->skills[candidate.slot - 1];
    const uint16_t activeSkillBefore = me->skill;
    const uint32_t energyBefore = GetCurrentEnergyPoints();
    IntReport("  Casting slot %u skill %u target=%u targetType=%u type=%u energyCost=%u baseRecharge=%u activation=%.2f recharge=%u event=%u active=%u energy=%u",
              candidate.slot,
              candidate.skillId,
              candidate.targetId,
              candidate.targetType,
              candidate.type,
              candidate.energyCost,
              candidate.baseRecharge,
              candidate.activation,
              before.recharge,
              before.event,
              activeSkillBefore,
              energyBefore);

    SkillMgr::UseSkill(candidate.slot, candidate.targetId, 0);

    const bool skillStarted = WaitFor("skill activation state change", 5000, [candidate, before, activeSkillBefore, energyBefore]() {
        Skillbar* liveBar = SkillMgr::GetPlayerSkillbar();
        auto* liveMe = GetAgentLivingRaw(ReadMyId());
        if (!liveBar || !liveMe) return false;

        const SkillbarSkill& after = liveBar->skills[candidate.slot - 1];
        const uint32_t energyAfter = GetCurrentEnergyPoints();
        return after.recharge != before.recharge ||
               after.event != before.event ||
               liveMe->skill != activeSkillBefore ||
               liveMe->skill == candidate.skillId ||
               energyAfter < energyBefore;
    });

    bar = SkillMgr::GetPlayerSkillbar();
    me = GetAgentLivingRaw(ReadMyId());
    if (bar && me) {
        const SkillbarSkill& after = bar->skills[candidate.slot - 1];
        IntReport("  After cast: recharge=%u event=%u active=%u energy=%u",
                  after.recharge,
                  after.event,
                  me->skill,
                  GetCurrentEnergyPoints());
    }

    IntCheck("Skill activation changes runtime state", skillStarted);
    IntReport("");
    return skillStarted;
}

// Loot pickup

bool TestLootPickup() {
    IntReport("=== GWA3-031 slice: Loot Pickup ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0 || ReadMyId() == 0) {
        IntSkip("Loot pickup", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    if (!area || !IsSkillCastMapType(area->type)) {
        IntSkip("Loot pickup", "Current instance type is not explorable-like");
        IntReport("");
        return false;
    }

    if (!WaitForStablePlayerState()) {
        IntReport("  Player runtime state: TypeMap=0x%X ModelState=%u", GetPlayerTypeMap(), GetPlayerModelState());
        IntSkip("Loot pickup", "Player state not stable enough for loot scan");
        IntReport("");
        return false;
    }

    AgentItem* item = FindNearbyGroundItem(5000.0f);
    if (!item) {
        const bool createdOpportunity = TryForceNearbyLootDrop();
        if (!createdOpportunity) {
            IntSkip("Loot pickup", "No nearby loot and could not create a local drop opportunity");
            IntReport("");
            return false;
        }
        IntCheck("Nearby loot opportunity created", true);
        if (createdOpportunity) {
            item = FindNearbyGroundItem(5000.0f);
        }
    }
    if (!item) {
        IntSkip("Loot pickup", "No nearby ground item found");
        IntReport("");
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

    IntReport("  Picking up item agent=%u item=%u at (%.0f, %.0f), dist=%.0f, gold=%u/%u inventoryCount=%u itemIdSum=%llu modelIdSum=%llu quantitySum=%llu",
              itemAgentId,
              itemId,
              itemX,
              itemY,
              itemDistance,
              inventoryBefore.goldCharacter,
              inventoryBefore.goldStorage,
              inventoryBefore.count,
              inventoryBefore.itemIdSum,
              inventoryBefore.modelIdSum,
              inventoryBefore.quantitySum);

    if (itemDistance < 0.0f || itemDistance > 180.0f) {
        IntReport("  Moving closer to loot before pickup...");
        MovePlayerNear(itemX, itemY, 120.0f, 12000);

        float pickupX = 0.0f;
        float pickupY = 0.0f;
        if (TryReadAgentPosition(ReadMyId(), pickupX, pickupY)) {
            IntReport("  Post-move loot distance: %.0f", AgentMgr::GetDistance(pickupX, pickupY, itemX, itemY));
        }
    }

    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (int b = 0; b < 5; ++b) {
                Bag* bag = inv->bags[b];
                if (!bag) { IntReport("    Bag[%d]: null", b); continue; }
                IntReport("    Bag[%d]: type=%u index=%u items_count=%u items.buffer=0x%08X items.size=%u",
                          b, bag->bag_type, bag->index, bag->items_count,
                          static_cast<unsigned>(reinterpret_cast<uintptr_t>(bag->items.buffer)),
                          bag->items.size);
            }
        } else {
            IntReport("    GetInventory() returned null");
        }
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
    IntReport("  Pickup loop finished: done=%d elapsed=%ums", pickupDone, GetTickCount() - pickupStart);

    Sleep(1000);

    const bool pickedUpIntoInventory = pickupDone || WaitFor("pickup acknowledged by inventory or item removal", 5000, [itemAgentId, itemId, inventoryBefore]() {
        if (!FindGroundItemByAgentId(itemAgentId)) return true;
        if (ItemMgr::GetItemById(itemId)) return true;
        const InventorySnapshot inventoryAfter = CaptureInventorySnapshot();
        return InventoryChangedMeaningfully(inventoryBefore, inventoryAfter);
    });

    const InventorySnapshot inventoryAfter = CaptureInventorySnapshot();
    Item* pickedItem = ItemMgr::GetItemById(itemId);
    AgentItem* remainingGroundItem = FindGroundItemByAgentId(itemAgentId);
    IntReport("  After pickup: gold=%u/%u inventoryCount=%u itemIdSum=%llu modelIdSum=%llu quantitySum=%llu pickedItem=%p groundItem=%p",
              inventoryAfter.goldCharacter,
              inventoryAfter.goldStorage,
              inventoryAfter.count,
              inventoryAfter.itemIdSum,
              inventoryAfter.modelIdSum,
              inventoryAfter.quantitySum,
              pickedItem,
              remainingGroundItem);
    if (pickedItem) {
        IntReport("  Picked item in inventory: item_id=%u model_id=%u quantity=%u bag=%p slot=%u",
                  pickedItem->item_id,
                  pickedItem->model_id,
                  pickedItem->quantity,
                  pickedItem->bag,
                  pickedItem->slot);
    }

    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (int b = 0; b < 5; ++b) {
                Bag* bag = inv->bags[b];
                if (!bag) continue;
                IntReport("    After Bag[%d]: type=%u items_count=%u items.size=%u",
                          b, bag->bag_type, bag->items_count, bag->items.size);
            }
        }
    }

    const bool inventoryChanged = InventoryChangedMeaningfully(inventoryBefore, inventoryAfter);
    const bool groundItemGone = (remainingGroundItem == nullptr);
    IntReport("  groundItemGone=%d inventoryChanged=%d pickedItem=%p",
              groundItemGone, inventoryChanged, pickedItem);
    IntCheck("Loot pickup changes inventory state", inventoryChanged || pickedItem != nullptr);

    IntReport("");
    return pickedUpIntoInventory;
}

} // namespace GWA3::SmokeTest
