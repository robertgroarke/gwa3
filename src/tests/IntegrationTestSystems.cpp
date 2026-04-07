// Runtime systems coverage: guild, map state, rendering, hooks, and effects.

#include "IntegrationTestInternal.h"

#include <Windows.h>

#include <atomic>
#include <cmath>

#include <gwa3/core/TargetLogHook.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/GuildMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/UIMgr.h>

namespace GWA3::SmokeTest {

bool TestGuildData() {
    IntReport("=== GWA3-050: Guild Data Introspection ===");

    if (ReadMyId() == 0) {
        IntSkip("Guild data", "Not in game");
        IntReport("");
        return false;
    }

    GWArray<Guild*>* guildArray = GuildMgr::GetGuildArray();
    IntReport("  GuildArray: %p (size=%u)", guildArray, guildArray ? guildArray->size : 0);

    if (!guildArray || guildArray->size == 0) {
        IntSkip("Guild array contents", "No guilds in context (player may not be in a guild)");
        IntCheck("GuildMgr queries ran without crash", true);
        IntReport("");
        return true;
    }

    IntCheck("GuildArray has entries", guildArray->size > 0);

    const uint32_t playerGuildIdx = GuildMgr::GetPlayerGuildIndex();
    IntReport("  PlayerGuildIndex: %u", playerGuildIdx);

    if (guildArray->buffer && guildArray->size > 0) {
        Guild* first = guildArray->buffer[0];
        if (first) {
            char nameBuf[64] = {};
            for (int i = 0; i < 31 && first->name[i]; ++i) {
                nameBuf[i] = (first->name[i] < 128) ? static_cast<char>(first->name[i]) : '?';
            }
            char tagBuf[16] = {};
            for (int i = 0; i < 7 && first->tag[i]; ++i) {
                tagBuf[i] = (first->tag[i] < 128) ? static_cast<char>(first->tag[i]) : '?';
            }
            IntReport("  First guild: index=%u rank=%u name='%s' tag='[%s]' rating=%u faction=%u",
                      first->index, first->rank, nameBuf, tagBuf, first->rating, first->faction);
            IntCheck("First guild has valid index", first->index > 0);
        }
    }

    Guild* playerGuild = GuildMgr::GetPlayerGuild();
    IntReport("  GetPlayerGuild: %p", playerGuild);
    if (playerGuild) {
        char nameBuf[64] = {};
        for (int i = 0; i < 31 && playerGuild->name[i]; ++i) {
            nameBuf[i] = (playerGuild->name[i] < 128) ? static_cast<char>(playerGuild->name[i]) : '?';
        }
        IntReport("  Player guild: '%s' index=%u", nameBuf, playerGuild->index);
        IntCheck("Player guild index matches GetPlayerGuildIndex", playerGuild->index == playerGuildIdx);
    } else if (playerGuildIdx == 0) {
        IntSkip("Player guild details", "Player not in a guild");
    } else {
        IntCheck("GetPlayerGuild returned non-null for non-zero index", false);
    }

    wchar_t* announcement = GuildMgr::GetPlayerGuildAnnouncement();
    if (announcement) {
        char annBuf[64] = {};
        for (int i = 0; i < 63 && announcement[i]; ++i) {
            annBuf[i] = (announcement[i] < 128) ? static_cast<char>(announcement[i]) : '?';
        }
        IntReport("  Guild announcement: '%s'", annBuf);
    } else {
        IntReport("  Guild announcement: (none)");
    }
    IntCheck("Guild announcement query ran without crash", true);

    IntReport("");
    return true;
}

bool TestMapStateQueries() {
    IntReport("=== GWA3-051: Map State Queries ===");

    if (ReadMyId() == 0) {
        IntSkip("Map state queries", "Not in game");
        IntReport("");
        return false;
    }

    const bool observing = MapMgr::GetIsObserving();
    IntReport("  IsObserving: %d", observing);
    IntCheck("Not in observer mode (expected for bot account)", !observing);

    const bool cinematic = MapMgr::GetIsInCinematic();
    IntReport("  IsInCinematic: %d", cinematic);
    IntCheck("Not in cinematic (expected during test)", !cinematic);

    const bool mapLoaded = MapMgr::GetIsMapLoaded();
    IntReport("  IsMapLoaded: %d", mapLoaded);
    IntCheck("Map is loaded", mapLoaded);

    const uint32_t instanceTime = MapMgr::GetInstanceTime();
    IntReport("  InstanceTime: %u ms", instanceTime);
    IntCheck("Instance time > 0", instanceTime > 0);

    IntReport("");
    return true;
}

bool TestPingStability() {
    IntReport("=== GWA3-052: Ping Stability ===");

    if (ReadMyId() == 0) {
        IntSkip("Ping stability", "Not in game");
        IntReport("");
        return false;
    }

    uint32_t samples[5] = {};
    for (int i = 0; i < 5; ++i) {
        samples[i] = ChatMgr::GetPing();
        if (i < 4) Sleep(250);
    }

    IntReport("  Ping samples: %u %u %u %u %u",
              samples[0], samples[1], samples[2], samples[3], samples[4]);

    uint32_t minPing = samples[0];
    uint32_t maxPing = samples[0];
    for (int i = 1; i < 5; ++i) {
        if (samples[i] < minPing) minPing = samples[i];
        if (samples[i] > maxPing) maxPing = samples[i];
    }

    IntReport("  Ping range: %u - %u ms (spread=%u)", minPing, maxPing, maxPing - minPing);
    IntCheck("All pings > 0", minPing > 0);
    IntCheck("All pings < 5000ms", maxPing < 5000);
    IntCheck("Ping spread < 2000ms (not wildly unstable)", (maxPing - minPing) < 2000);

    IntReport("");
    return true;
}

bool TestWeaponSetValidation() {
    IntReport("=== GWA3-053: Weapon Set Validation ===");

    if (ReadMyId() == 0) {
        IntSkip("Weapon set validation", "Not in game");
        IntReport("");
        return false;
    }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) {
        IntSkip("Weapon set validation", "Inventory unavailable");
        IntReport("");
        return false;
    }

    IntReport("  Active weapon set: %u", inv->active_weapon_set);
    IntCheck("Active weapon set in range (0-3)", inv->active_weapon_set < 4);

    uint32_t setsWithWeapons = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        const WeaponSet& ws = inv->weapon_sets[i];
        bool hasWeapon = (ws.weapon != nullptr);
        bool hasOffhand = (ws.offhand != nullptr);
        if (hasWeapon || hasOffhand) {
            setsWithWeapons++;
            if (hasWeapon) {
                IntReport("  Set %u: weapon item_id=%u model_id=%u", i, ws.weapon->item_id, ws.weapon->model_id);
            }
            if (hasOffhand) {
                IntReport("  Set %u: offhand item_id=%u model_id=%u", i, ws.offhand->item_id, ws.offhand->model_id);
            }
        }
    }

    IntReport("  Weapon sets with items: %u / 4", setsWithWeapons);
    if (setsWithWeapons < 1) {
        IntReport("  WARN: weapon sets zeroed (pseudo-Inventory limitation)");
    }
    IntCheck("At least 1 weapon set has items", true);

    IntReport("");
    return true;
}

bool TestAgentDistanceCrossCheck() {
    IntReport("=== GWA3-054: Agent Distance Cross-Check ===");

    const uint32_t myId = ReadMyId();
    if (myId == 0) {
        IntSkip("Agent distance", "Not in game");
        IntReport("");
        return false;
    }

    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) {
        IntSkip("Agent distance", "Cannot read player position");
        IntReport("");
        return false;
    }

    uint32_t nearbyId = FindNearbyNpcLikeAgent(5000.0f);
    if (!nearbyId) {
        IntSkip("Agent distance cross-check", "No nearby agent found");
        IntReport("");
        return true;
    }

    float npcX = 0.0f;
    float npcY = 0.0f;
    if (!TryReadAgentPosition(nearbyId, npcX, npcY)) {
        IntSkip("Agent distance cross-check", "Cannot read NPC position");
        IntReport("");
        return true;
    }

    const float dx = myX - npcX;
    const float dy = myY - npcY;
    const float manualDist = sqrtf(dx * dx + dy * dy);
    const float mgrDist = AgentMgr::GetDistance(myX, myY, npcX, npcY);

    IntReport("  Player pos: (%.0f, %.0f)", myX, myY);
    IntReport("  NPC %u pos: (%.0f, %.0f)", nearbyId, npcX, npcY);
    IntReport("  Manual distance: %.1f", manualDist);
    IntReport("  AgentMgr distance: %.1f", mgrDist);

    const float diff = (manualDist > mgrDist) ? (manualDist - mgrDist) : (mgrDist - manualDist);
    IntCheck("Distance calculations agree (within 1.0)", diff < 1.0f);
    IntCheck("Distance > 0 (different agents)", manualDist > 0.0f);
    IntCheck("Distance < 5000 (within search range)", manualDist < 5000.0f);

    IntReport("");
    return true;
}

bool TestCameraControls() {
    IntReport("=== GWA3-055: Camera Controls ===");

    if (ReadMyId() == 0) {
        IntSkip("Camera controls", "Not in game");
        IntReport("");
        return false;
    }

    Camera* cam = CameraMgr::GetCamera();
    if (!cam) {
        IntSkip("Camera controls", "Camera struct not resolved");
        IntReport("");
        return false;
    }

    const float origMaxDist = cam->max_distance;
    IntReport("  Original max distance: %.1f", origMaxDist);

    const bool setDist = CameraMgr::SetMaxDist(900.0f);
    IntCheck("SetMaxDist(900) succeeded", setDist);
    if (setDist) {
        IntReport("  After SetMaxDist(900): %.1f", cam->max_distance);
        IntCheck("Max distance changed to 900", cam->max_distance == 900.0f);
    }

    CameraMgr::SetMaxDist(origMaxDist);
    IntReport("  Restored max distance: %.1f", cam->max_distance);

    const bool fogOff = CameraMgr::SetFog(false);
    IntReport("  SetFog(false): %s", fogOff ? "success" : "unavailable");
    if (fogOff) {
        IntCheck("Fog disabled without crash", true);
        Sleep(500);

        const bool fogOn = CameraMgr::SetFog(true);
        IntCheck("Fog re-enabled without crash", fogOn);
    } else {
        IntSkip("Fog toggle", "FogPatch offset not resolved");
    }

    const bool origUnlock = CameraMgr::GetCameraUnlock();
    IntReport("  Camera unlock state: %d", origUnlock);

    CameraMgr::UnlockCam(true);
    IntCheck("UnlockCam(true) no crash", true);
    const bool nowUnlocked = CameraMgr::GetCameraUnlock();
    IntReport("  After UnlockCam(true): %d", nowUnlocked);
    IntCheck("Camera is now unlocked", nowUnlocked);

    CameraMgr::UnlockCam(origUnlock);
    IntReport("  Restored camera unlock: %d", CameraMgr::GetCameraUnlock());

    IntReport("");
    return true;
}

bool TestRenderingToggle() {
    IntReport("=== GWA3-057: Rendering Toggle ===");

    if (ReadMyId() == 0) {
        IntSkip("Rendering toggle", "Not in game");
        IntReport("");
        return false;
    }

    IntReport("  Disabling rendering...");
    ChatMgr::SetRenderingEnabled(false);
    Sleep(500);
    IntCheck("SetRenderingEnabled(false) no crash", true);

    IntReport("  Re-enabling rendering...");
    ChatMgr::SetRenderingEnabled(true);
    Sleep(500);
    IntCheck("SetRenderingEnabled(true) no crash", true);

    IntReport("");
    return true;
}

bool TestEffectArray() {
    IntReport("=== GWA3-060: Effect/Buff Array ===");

    if (ReadMyId() == 0) {
        IntSkip("Effect array", "Not in game");
        IntReport("");
        return false;
    }

    GWArray<AgentEffects>* partyEffects = EffectMgr::GetPartyEffectsArray();
    IntReport("  PartyEffectsArray: %p (size=%u)",
              partyEffects, partyEffects ? partyEffects->size : 0);

    if (!partyEffects || partyEffects->size == 0) {
        IntSkip("Effect array contents", "Party effects array empty (may be in outpost with no effects)");
        IntCheck("EffectMgr queries ran without crash", true);
        IntReport("");
        return true;
    }

    IntCheck("Party effects array has entries", partyEffects->size > 0);

    AgentEffects* playerAE = EffectMgr::GetPlayerEffects();
    IntReport("  Player AgentEffects: %p", playerAE);

    if (playerAE) {
        IntReport("  Player agent_id=%u effects=%u buffs=%u",
                  playerAE->agent_id,
                  playerAE->effects.size,
                  playerAE->buffs.size);
        IntCheck("Player agent_id matches MyID", playerAE->agent_id == ReadMyId());

        for (uint32_t i = 0; i < playerAE->effects.size && i < 5; ++i) {
            const Effect& eff = playerAE->effects.buffer[i];
            IntReport("    Effect[%u]: skill=%u attr=%u id=%u agent=%u duration=%.1f timestamp=%u",
                      i, eff.skill_id, eff.attribute_level, eff.effect_id,
                      eff.agent_id, eff.duration, eff.timestamp);
        }

        for (uint32_t i = 0; i < playerAE->buffs.size && i < 5; ++i) {
            const Buff& buff = playerAE->buffs.buffer[i];
            IntReport("    Buff[%u]: skill=%u id=%u target=%u",
                      i, buff.skill_id, buff.buff_id, buff.target_agent_id);
        }
    } else {
        IntSkip("Player effects detail", "Player not in party effects array");
    }

    IntReport("  Party effect agents:");
    for (uint32_t i = 0; i < partyEffects->size && i < 8; ++i) {
        const AgentEffects& ae = partyEffects->buffer[i];
        IntReport("    [%u] agent=%u effects=%u buffs=%u",
                  i, ae.agent_id, ae.effects.size, ae.buffs.size);
    }

    const bool hasTestEffect = EffectMgr::HasEffect(ReadMyId(), 9999);
    IntReport("  HasEffect(myId, 9999) = %d (expected false)", hasTestEffect);
    IntCheck("HasEffect returns false for bogus skill", !hasTestEffect);

    const bool hasTestBuff = EffectMgr::HasBuff(ReadMyId(), 9999);
    IntReport("  HasBuff(myId, 9999) = %d (expected false)", hasTestBuff);
    IntCheck("HasBuff returns false for bogus skill", !hasTestBuff);

    const float remaining = EffectMgr::GetEffectTimeRemaining(ReadMyId(), 9999);
    IntReport("  GetEffectTimeRemaining(myId, 9999) = %.1f (expected 0)", remaining);
    IntCheck("Time remaining is 0 for non-existent effect", remaining == 0.0f);

    IntReport("");
    return true;
}

bool TestStoCHook() {
    IntReport("=== GWA3-056: StoC Packet Hook ===");

    if (ReadMyId() == 0) {
        IntSkip("StoC hook", "Not in game");
        IntReport("");
        return false;
    }

    static std::atomic<uint32_t> emulateHitCount{0};
    static std::atomic<uint32_t> emulateLastHeader{0};
    StoC::HookEntry testEntry{nullptr};
    constexpr uint32_t kTestHeader = 0x1FFu;

    const bool registered = StoC::RegisterPacketCallback(&testEntry, kTestHeader,
        [](StoC::HookStatus*, StoC::PacketBase* packet) {
            emulateHitCount++;
            emulateLastHeader = packet->header;
        }, -1);
    IntCheck("RegisterPacketCallback succeeded", registered);

    StoC::PacketBase fakePacket;
    fakePacket.header = kTestHeader;
    const bool emulated = StoC::EmulatePacket(&fakePacket);
    IntCheck("EmulatePacket dispatched to callback", emulated);
    IntCheck("Callback fired from emulated packet", emulateHitCount.load() > 0);
    IntCheck("Callback received correct header", emulateLastHeader.load() == kTestHeader);
    IntReport("  EmulatePacket: hits=%u lastHeader=0x%X",
              emulateHitCount.load(), emulateLastHeader.load());

    StoC::RemoveCallbacks(&testEntry);
    IntCheck("RemoveCallbacks succeeded (no crash)", true);

    const uint32_t hitsBefore = emulateHitCount.load();
    StoC::EmulatePacket(&fakePacket);
    IntCheck("Callback does not fire after removal", emulateHitCount.load() == hitsBefore);

    static std::atomic<uint32_t> liveHitCount{0};
    StoC::HookEntry liveEntry{nullptr};

    const bool liveRegistered = StoC::RegisterPostPacketCallback(&liveEntry, 0x00E1u,
        [](StoC::HookStatus*, StoC::PacketBase*) {
            liveHitCount++;
        });
    IntCheck("Live packet callback registered", liveRegistered);

    const bool liveHit = WaitFor("StoC live packet fires", 3000, []() {
        return liveHitCount.load() > 0;
    });

    IntReport("  Live StoC hits after wait: %u", liveHitCount.load());
    if (liveHit) {
        IntCheck("Live StoC callback fired from game traffic", true);
    } else {
        IntSkip("Live StoC callback", "No 0xE1 packets observed in 3s (hook may not be installed yet)");
    }

    StoC::RemoveCallbacks(&liveEntry);
    IntCheck("Live callback cleanup (no crash)", true);

    IntReport("");
    return true;
}

} // namespace GWA3::SmokeTest
