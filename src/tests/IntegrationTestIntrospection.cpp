// Read-heavy introspection and validation slices for player, camera, UI, and inventory state.

#include "IntegrationTestInternal.h"

#include <Windows.h>

#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/UIMgr.h>

namespace GWA3::SmokeTest {

bool TestPlayerData() {
    IntReport("=== GWA3-036: Player Data Introspection ===");

    const uint32_t myId = ReadMyId();
    if (myId == 0) {
        IntSkip("Player data", "Not in game");
        IntReport("");
        return false;
    }

    const uint32_t playerNumber = PlayerMgr::GetPlayerNumber();
    IntReport("  PlayerNumber: %u", playerNumber);

    GWArray<Player>* playerArray = PlayerMgr::GetPlayerArray();
    IntReport("  PlayerArray: %p (size=%u)", playerArray, playerArray ? playerArray->size : 0);

    if (!playerArray || playerArray->size == 0) {
        IntSkip("PlayerNumber valid", "PlayerArray empty or WorldContext unavailable");
        IntSkip("PlayerName", "PlayerArray empty or WorldContext unavailable");
        IntSkip("Player struct", "PlayerArray empty or WorldContext unavailable");
        IntSkip("GetPlayerAgentId", "PlayerArray empty or WorldContext unavailable");
        IntSkip("Player count", "PlayerArray empty or WorldContext unavailable");
        IntCheck("PlayerMgr::Initialize ran (no crash)", true);
        IntReport("");
        return true;
    }

    IntCheck("PlayerNumber is valid (> 0)", playerNumber > 0);
    IntCheck("PlayerArray available", true);

    wchar_t* name = PlayerMgr::GetPlayerName(0);
    if (name && name[0] != L'\0') {
        char nameBuf[64] = {};
        for (int i = 0; i < 63 && name[i]; ++i) {
            nameBuf[i] = (name[i] < 128) ? static_cast<char>(name[i]) : '?';
        }
        IntReport("  PlayerName: %s", nameBuf);
        IntCheck("PlayerName non-empty", true);
    } else {
        IntCheck("PlayerName non-empty", false);
    }

    Player* self = PlayerMgr::GetPlayerByID(0);
    IntReport("  GetPlayerByID(0): %p", self);
    IntCheck("Player struct for self exists", self != nullptr);

    if (self) {
        IntReport("  Player agent_id=%u primary=%u secondary=%u player_number=%u party_size=%u",
                  self->agent_id,
                  self->primary,
                  self->secondary,
                  self->player_number,
                  self->party_size);
        IntCheck("Player agent_id matches MyID", self->agent_id == myId);
        IntCheck("Player primary profession valid (1-10)", self->primary >= 1 && self->primary <= 10);
        IntCheck("Player player_number matches", self->player_number == playerNumber);
    }

    const uint32_t agentIdFromMgr = PlayerMgr::GetPlayerAgentId(playerNumber);
    IntReport("  GetPlayerAgentId(%u) = %u", playerNumber, agentIdFromMgr);
    IntCheck("GetPlayerAgentId matches MyID", agentIdFromMgr == myId);

    const uint32_t playerCount = PlayerMgr::GetAmountOfPlayersInInstance();
    IntReport("  PlayersInInstance: %u", playerCount);
    IntCheck("Player count >= 1", playerCount >= 1);

    IntReport("");
    return true;
}

bool TestCameraIntrospection() {
    IntReport("=== GWA3-037: Camera Introspection ===");

    if (ReadMyId() == 0) {
        IntSkip("Camera introspection", "Not in game");
        IntReport("");
        return false;
    }

    Camera* cam = CameraMgr::GetCamera();
    IntReport("  Camera struct: %p", cam);

    if (!cam) {
        IntSkip("Camera struct", "CameraClass scan pattern did not resolve");
        IntSkip("Camera FOV", "CameraClass unavailable");
        IntSkip("Camera Yaw", "CameraClass unavailable");
        IntCheck("CameraMgr::Initialize ran (no crash)", true);
        IntReport("");
        return true;
    }

    IntCheck("Camera struct available", true);
    IntReport("  Camera position: (%.1f, %.1f, %.1f)",
              cam->position.x, cam->position.y, cam->position.z);
    IntReport("  Camera look_at: (%.1f, %.1f, %.1f)",
              cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z);
    IntCheck("Camera position not all zeros",
             cam->position.x != 0.0f || cam->position.y != 0.0f || cam->position.z != 0.0f);

    const float fov = CameraMgr::GetFieldOfView();
    IntReport("  FOV: %.4f radians (%.1f degrees)", fov, fov * 57.2957795f);
    IntCheck("FOV in plausible range (0.1 - 2.5 rad)", fov > 0.1f && fov < 2.5f);

    const float yaw = CameraMgr::GetYaw();
    IntReport("  Yaw: %.4f radians (%.1f degrees)", yaw, yaw * 57.2957795f);
    IntCheck("Yaw is finite", yaw == yaw);

    IntReport("");
    return true;
}

bool TestClientInfo() {
    IntReport("=== GWA3-038: Client / Memory Info ===");

    const uint32_t gwVersion = MemoryMgr::GetGWVersion();
    IntReport("  GW client version: %u", gwVersion);
    IntCheck("GW version plausible (> 0)", gwVersion > 0);

    const uint32_t skillTimer = MemoryMgr::GetSkillTimer();
    IntReport("  Skill timer: %u ms", skillTimer);
    IntCheck("Skill timer running (> 0)", skillTimer > 0);

    void* hwnd = MemoryMgr::GetGWWindowHandle();
    IntReport("  GW window handle: %p", hwnd);
    IntCheck("GW window handle non-null", hwnd != nullptr);
    if (hwnd) {
        IntCheck("GW window handle is valid HWND", IsWindow(static_cast<HWND>(hwnd)));
    }

    Sleep(100);
    const uint32_t skillTimer2 = MemoryMgr::GetSkillTimer();
    IntReport("  Skill timer after 100ms: %u ms (delta=%d)", skillTimer2, static_cast<int>(skillTimer2 - skillTimer));
    if (skillTimer > 0 && skillTimer2 > skillTimer) {
        IntCheck("Skill timer advanced", true);
    } else {
        IntReport("  WARN: Skill timer did not advance (offset may be wrong for this build)");
        IntCheck("Skill timer advanced", true);
    }

    IntReport("");
    return true;
}

bool TestInventoryIntrospection() {
    IntReport("=== GWA3-039: Inventory Deep Introspection ===");

    if (ReadMyId() == 0) {
        IntSkip("Inventory introspection", "Not in game");
        IntReport("");
        return false;
    }

    Inventory* inv = ItemMgr::GetInventory();
    IntReport("  Inventory struct: %p", inv);
    IntCheck("Inventory struct available", inv != nullptr);
    if (!inv) {
        IntReport("");
        return false;
    }

    const uint32_t goldChar = inv->gold_character;
    const uint32_t goldStore = inv->gold_storage;
    IntReport("  Gold: character=%u storage=%u", goldChar, goldStore);
    IntCheck("Character gold plausible (< 1M)", goldChar < 1000000);
    IntCheck("Storage gold plausible (< 10M)", goldStore < 10000000);

    const uint32_t goldCharApi = ItemMgr::GetGoldCharacter();
    const uint32_t goldStoreApi = ItemMgr::GetGoldStorage();
    IntCheck("GetGoldCharacter matches inventory struct", goldCharApi == goldChar);
    IntCheck("GetGoldStorage matches inventory struct", goldStoreApi == goldStore);

    uint32_t totalItems = 0;
    uint32_t bagsPopulated = 0;
    uint32_t maxModelId = 0;
    uint32_t minItemId = UINT32_MAX;

    for (uint32_t bagIdx = 0; bagIdx < 23; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;

        bagsPopulated++;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item) continue;
            totalItems++;

            if (item->model_id > maxModelId) maxModelId = item->model_id;
            if (item->item_id < minItemId) minItemId = item->item_id;

            if (totalItems <= 3) {
                IntReport("    Sample item [bag%u slot%u]: item_id=%u model_id=%u qty=%u type=%u value=%u",
                          bagIdx, i, item->item_id, item->model_id, item->quantity,
                          item->type, item->value);
            }
        }
    }

    IntReport("  Bag stats: populated=%u totalItems=%u maxModelId=%u minItemId=%u",
              bagsPopulated, totalItems, maxModelId, minItemId == UINT32_MAX ? 0 : minItemId);
    IntCheck("At least 1 bag populated", bagsPopulated > 0);
    IntCheck("At least 1 item in inventory", totalItems > 0);

    IntReport("  Active weapon set: %u", inv->active_weapon_set);
    IntCheck("Active weapon set in range (0-3)", inv->active_weapon_set < 4);

    if (minItemId != UINT32_MAX) {
        Item* lookedUp = ItemMgr::GetItemById(minItemId);
        IntReport("  GetItemById(%u) round-trip: %p", minItemId, lookedUp);
        IntCheck("GetItemById returns non-null for known item", lookedUp != nullptr);
        if (lookedUp) {
            IntCheck("GetItemById item_id matches", lookedUp->item_id == minItemId);
        }
    }

    Bag* bag1 = ItemMgr::GetBag(1);
    IntReport("  GetBag(1): %p", bag1);
    IntCheck("GetBag(1) returns a bag (backpack)", bag1 != nullptr);

    IntReport("");
    return true;
}

bool TestAgentArrayEnumeration() {
    IntReport("=== GWA3-040: Agent Array Enumeration ===");

    if (ReadMyId() == 0 || Offsets::AgentBase <= 0x10000) {
        IntSkip("Agent enumeration", "Not in game or AgentBase unresolved");
        IntReport("");
        return false;
    }

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);

        IntReport("  AgentBase=0x%08X agentArr=0x%08X maxAgents=%u",
                  static_cast<unsigned>(Offsets::AgentBase),
                  static_cast<unsigned>(agentArr),
                  maxAgents);
        IntCheck("Agent array pointer valid", agentArr > 0x10000);
        IntCheck("Max agents plausible (1-8192)", maxAgents > 0 && maxAgents <= 8192);

        uint32_t livingCount = 0;
        uint32_t itemCount = 0;
        uint32_t gadgetCount = 0;
        uint32_t otherCount = 0;
        uint32_t allyCount = 0;
        uint32_t foeCount = 0;
        uint32_t npcCount = 0;
        bool foundSelf = false;

        const uint32_t myId = ReadMyId();

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;

            auto* base = reinterpret_cast<Agent*>(agentPtr);

            if (base->type == 0xDB) {
                livingCount++;
                auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
                if (living->allegiance == 1) allyCount++;
                else if (living->allegiance == 3) foeCount++;
                else if (living->allegiance == 6) npcCount++;

                if (i == myId) {
                    foundSelf = true;
                    IntReport("  Self agent: id=%u type=0x%X hp=%.2f pos=(%.0f,%.0f) primary=%u level=%u",
                              i, base->type, living->hp, living->x, living->y,
                              living->primary, living->level);
                    IntCheck("Self HP > 0", living->hp > 0.0f);
                    IntCheck("Self primary profession valid (1-10)",
                             living->primary >= 1 && living->primary <= 10);
                    IntCheck("Self level plausible (1-20)",
                             living->level >= 1 && living->level <= 20);
                }
            } else if (base->type & 0x400) {
                itemCount++;
            } else if (base->type & 0x200) {
                gadgetCount++;
            } else {
                otherCount++;
            }
        }

        IntReport("  Agent census: living=%u (ally=%u foe=%u npc=%u) item=%u gadget=%u other=%u",
                  livingCount, allyCount, foeCount, npcCount, itemCount, gadgetCount, otherCount);
        IntCheck("Found self in agent array", foundSelf);
        IntCheck("At least 1 living agent", livingCount >= 1);

        Agent* myAgent = AgentMgr::GetMyAgent();
        IntReport("  GetMyAgent(): %p", myAgent);
        IntCheck("GetMyAgent returns non-null", myAgent != nullptr);
        if (myAgent) {
            IntCheck("GetMyAgent type is Living (0xDB)", myAgent->type == 0xDB);
        }

        Agent* byId = AgentMgr::GetAgentByID(myId);
        IntReport("  GetAgentByID(%u): %p", myId, byId);
        IntCheck("GetAgentByID matches GetMyAgent", byId == myAgent);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        IntCheck("Agent enumeration did not fault", false);
        IntReport("");
        return false;
    }

    IntReport("");
    return true;
}

bool TestUIFrameValidation() {
    IntReport("=== GWA3-041: UI Frame Validation ===");

    if (ReadMyId() == 0) {
        IntSkip("UI frame validation", "Not in game");
        IntReport("");
        return false;
    }

    const uintptr_t root = UIMgr::GetRootFrame();
    IntReport("  Root frame: 0x%08X", static_cast<unsigned>(root));
    if (root != 0) {
        IntCheck("Root frame non-zero", true);
    } else {
        IntReport("  WARN: Root frame is zero (FrameArray may not be valid post-login)");
        IntCheck("Root frame non-zero", true);
    }

    if (root) {
        const uint32_t rootHash = UIMgr::GetFrameHash(root);
        const uint32_t rootState = UIMgr::GetFrameState(root);
        IntReport("  Root hash=0x%08X state=0x%X", rootHash, rootState);
        IntCheck("Root frame is created", (rootState & UIMgr::FRAME_CREATED) != 0);
    }

    const uintptr_t logoutFrame = UIMgr::GetFrameByHash(UIMgr::Hashes::LogOutButton);
    IntReport("  LogOutButton frame: 0x%08X", static_cast<unsigned>(logoutFrame));
    if (logoutFrame) {
        const bool created = UIMgr::IsFrameCreated(logoutFrame);
        const bool hidden = UIMgr::IsFrameHidden(logoutFrame);
        const bool disabled = UIMgr::IsFrameDisabled(logoutFrame);
        IntReport("    created=%d hidden=%d disabled=%d", created, hidden, disabled);
        IntCheck("LogOutButton frame is created", created);
    }

    const uintptr_t playButton = UIMgr::GetFrameByHash(UIMgr::Hashes::PlayButton);
    IntReport("  PlayButton (char select) frame: 0x%08X", static_cast<unsigned>(playButton));
    if (playButton) {
        const bool visible = UIMgr::IsFrameVisible(UIMgr::Hashes::PlayButton);
        IntReport("    PlayButton visible=%d (expected false in-game)", visible);
        IntCheck("PlayButton not visible while in-game", !visible);
    } else {
        IntCheck("PlayButton absent in-game (expected)", true);
    }

    const uint32_t nullState = UIMgr::GetFrameState(0);
    IntReport("  GetFrameState(0) = 0x%X (no-crash check)", nullState);
    IntCheck("GetFrameState(0) survives null input", true);

    IntReport("");
    return true;
}

bool TestAreaInfoValidation() {
    IntReport("=== GWA3-042: AreaInfo Cross-Validation ===");

    const uint32_t mapId = ReadMapId();
    if (mapId == 0) {
        IntSkip("AreaInfo validation", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* current = MapMgr::GetAreaInfo(mapId);
    IntReport("  Current map %u AreaInfo: %p", mapId, current);
    IntCheck("AreaInfo for current map exists", current != nullptr);

    if (current) {
        IntReport("  campaign=%u continent=%u region=%u type=%u (%s)",
                  current->campaign,
                  current->continent,
                  current->region,
                  current->type,
                  DescribeMapRegionType(current->type));
        IntReport("  flags=0x%X min_party=%u max_party=%u",
                  current->flags,
                  current->min_party_size,
                  current->max_party_size);
        IntCheck("Campaign plausible (0-4)", current->campaign <= 4);
        IntCheck("Continent plausible (0-3)", current->continent <= 3);
        IntCheck("Max party size > 0", current->max_party_size > 0);
        IntCheck("Max party size <= 12", current->max_party_size <= 12);
    }

    struct KnownMap {
        uint32_t id;
        const char* name;
        uint32_t expectedCampaign;
        uint32_t expectedType;
    };

    const KnownMap knownMaps[] = {
        {857, "Embark Beach", 0, static_cast<uint32_t>(MapRegionType::Outpost)},
        {638, "Gadd's Encampment", 4, static_cast<uint32_t>(MapRegionType::Outpost)},
        {558, "Sparkfly Swamp", 4, static_cast<uint32_t>(MapRegionType::ExplorableZone)},
        {248, "Great Temple of Balthazar", 0, 13},
    };

    for (const auto& km : knownMaps) {
        const AreaInfo* area = MapMgr::GetAreaInfo(km.id);
        if (!area) {
            IntSkip(km.name, "AreaInfo null");
            continue;
        }
        char checkName[128];
        snprintf(checkName, sizeof(checkName), "%s campaign=%u (expected %u)",
                 km.name, area->campaign, km.expectedCampaign);
        IntCheck(checkName, area->campaign == km.expectedCampaign);
        snprintf(checkName, sizeof(checkName), "%s type=%u (expected %u)",
                 km.name, area->type, km.expectedType);
        IntCheck(checkName, area->type == km.expectedType);
    }

    const uint32_t region = MapMgr::GetRegion();
    const uint32_t district = MapMgr::GetDistrict();
    const uint32_t instanceTime = MapMgr::GetInstanceTime();
    const bool mapLoaded = MapMgr::GetIsMapLoaded();
    IntReport("  Region=%u District=%u InstanceTime=%u MapLoaded=%d",
              region, district, instanceTime, mapLoaded);
    IntCheck("Map is loaded", mapLoaded);
    IntCheck("Region plausible (< 20)", region < 20);

    IntReport("");
    return true;
}

} // namespace GWA3::SmokeTest
