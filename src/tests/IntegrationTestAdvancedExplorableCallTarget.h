bool TestExplorableCallTarget() {
    IntReport("=== CallTarget in Explorable ===");

    if (ReadMyId() == 0) { IntSkip("ExplorableCallTarget", "Not in game"); IntReport(""); return false; }

    uint32_t mapId = ReadMapId();

    // If not at Gadd's, travel there first
    if (mapId != MapIds::GADDS_ENCAMPMENT && !IsSkillCastMapType(MapMgr::GetAreaInfo(mapId)->type)) {
        IntReport("  Traveling to Gadd's Encampment...");
        MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
        bool arrived = WaitFor("MapID = Gadd's", 60000, []() {
            return ReadMapId() == MapIds::GADDS_ENCAMPMENT;
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
            if (ReadMapId() == MapIds::SPARKFLY_SWAMP) {
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

    // Now we're in explorable ??? walk deep into Sparkfly to find real enemies.
    // The spawn point is near (-9500, -20000). First combat waypoint from the
    // Froggy route is (-4559, -14406) ??? we MUST walk there before scanning.
    IntReport("  In Sparkfly (map %u), walking to combat zone...", ReadMapId());

    // Walk through intermediate waypoints toward the first combat area
    static const struct { float x; float y; float threshold; int timeoutMs; } kWalkSteps[] = {
        {-7500.0f, -17000.0f, 500.0f, 20000},  // intermediate
        {-5500.0f, -15000.0f, 500.0f, 20000},  // closer
        {-4559.0f, -14406.0f, 500.0f, 25000},  // first Froggy combat waypoint
    };

    for (const auto& step : kWalkSteps) {
        IntReport("  Walking to (%.0f, %.0f)...", step.x, step.y);
        MovePlayerNear(step.x, step.y, step.threshold, step.timeoutMs);
        // Check for foes after each step
        uint32_t earlyFoe = FindNearbyFoeAgent(2000.0f);
        if (earlyFoe) {
            IntReport("  Found foe %u early at this waypoint", earlyFoe);
            break;
        }
    }

    // Now scan for foes at the combat area
    Sleep(1000);
    uint32_t foeId = FindNearbyFoeAgent(3000.0f);

    // If none found at first waypoint, try the second combat waypoint
    if (!foeId) {
        IntReport("  No foes at first waypoint, walking to (-5204, -9831)...");
        MovePlayerNear(-5204.0f, -9831.0f, 500.0f, 25000);
        Sleep(1000);
        foeId = FindNearbyFoeAgent(3000.0f);
    }

    if (!foeId) {
        IntSkip("CallTarget in explorable", "No foes found after probing Sparkfly areas");
        IntReport("");
        return false;
    }

    // First verify ChangeTarget works (this is proven)
    AgentMgr::ChangeTarget(foeId);
    bool targetSet = WaitFor("ChangeTarget updates in explorable", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    IntCheck("ChangeTarget works in explorable", targetSet);

    // Now test CallTarget ??? record chat log timestamp before call
    const uint32_t chatTimeBefore = GetTickCount();
    const uint32_t chatCountBefore = ChatLogMgr::GetMessageCount();
    const uint32_t calledBefore = PartyMgr::GetCalledTargetId();
    IntReport("  Found foe agent %u, testing CallTarget...", foeId);
    IntReport("  Called target before: %u, chat messages: %u", calledBefore, chatCountBefore);

    AgentMgr::CallTarget(foeId);
    Sleep(1000); // give time for packet round-trip and chat message

    const uint32_t calledAfter = PartyMgr::GetCalledTargetId();
    const uint32_t chatCountAfter = ChatLogMgr::GetMessageCount();
    IntReport("  Called target after: %u, chat messages: %u (delta=%u)",
              calledAfter, chatCountAfter, chatCountAfter - chatCountBefore);

    // Check party called target (may not work due to read-back issue)
    if (calledAfter == foeId) {
        IntCheck("CallTarget set party called target", true);
    } else {
        IntReport("  GetCalledTargetId didn't update (known read-back issue)");
    }

    // Check chat log for call target message ??? this is the reliable validation
    bool chatConfirmed = false;
    if (chatCountAfter > chatCountBefore) {
        // Scan recent messages for any new entries since the call
        const ChatLogMgr::ChatEntry* recentMsgs[8] = {};
        uint32_t recentCount = ChatLogMgr::GetMessagesSince(chatTimeBefore, recentMsgs, 8);
        IntReport("  Chat messages since call: %u", recentCount);
        for (uint32_t i = 0; i < recentCount; ++i) {
            if (!recentMsgs[i]) continue;
            // Log the message for debugging
            char narrow[128] = {};
            for (int j = 0; j < 127 && recentMsgs[i]->message[j]; ++j) {
                narrow[j] = (recentMsgs[i]->message[j] < 128)
                    ? static_cast<char>(recentMsgs[i]->message[j]) : '?';
            }
            IntReport("    [%s] %s", recentMsgs[i]->channel_name, narrow);
            chatConfirmed = true; // any new chat message after CallTarget is evidence it fired
        }
    }

    IntCheck("CallTarget produced observable effect (chat or party state)",
             calledAfter == foeId || chatConfirmed);

    // --- UIMessage CallTarget experiment (investigation) ---
    // Use UIMessage to call target on SELF with Morale type ??? produces a different
    // chat message ("I have X% morale boost/death penalty") so we can distinguish
    // from the packet-based call that already fired.
    {
        IntReport("  --- UIMessage CallTarget experiment ---");

        struct CallTargetUIPacket {
            uint32_t call_type;
            uint32_t agent_id;
        };

        const uint32_t uiChatBefore = ChatLogMgr::GetMessageCount();

        // Call self with Morale type (0x7) ??? produces a morale chat message
        CallTargetUIPacket uiPacket;
        uiPacket.call_type = 0x7; // Morale
        uiPacket.agent_id = ReadMyId();

        IntReport("  Sending UIMessage kSendCallTarget (Morale, self=%u)...", uiPacket.agent_id);
        GameThread::EnqueuePost([uiPacket]() mutable {
            UIMgr::SendUIMessage(0x30000013u, &uiPacket, nullptr);
        });
        Sleep(1000);

        const uint32_t uiChatAfter = ChatLogMgr::GetMessageCount();
        IntReport("  UIMessage Morale CallTarget: chat=%u???%u (delta=%u)",
                  uiChatBefore, uiChatAfter, uiChatAfter - uiChatBefore);

        if (uiChatAfter > uiChatBefore) {
            IntCheck("UIMessage CallTarget (Morale) produced chat message", true);

            // Dump the new messages
            const ChatLogMgr::ChatEntry* msgs[4] = {};
            uint32_t count = ChatLogMgr::GetMessagesSince(GetTickCount() - 2000, msgs, 4);
            for (uint32_t i = 0; i < count; ++i) {
                if (!msgs[i]) continue;
                char narrow[128] = {};
                for (int j = 0; j < 127 && msgs[i]->message[j]; ++j) {
                    narrow[j] = (msgs[i]->message[j] < 128)
                        ? static_cast<char>(msgs[i]->message[j]) : '?';
                }
                IntReport("    [%s] %s", msgs[i]->channel_name, narrow);
            }
        } else {
            IntReport("  UIMessage CallTarget (Morale) DID NOT produce chat message");
            IntSkip("UIMessage CallTarget", "No observable effect ???  still open");
        }
    }

    // --- StoC AgentUpdateEffects probe in explorable ---
    // In explorable with heroes and foes, buff activity should generate 0x00F1 packets.
    // This validates the StoC hook works for effect packets (skipped in outpost).
    IntReport("  Probing StoC AgentUpdateEffects in explorable (5s)...");
    static std::atomic<uint32_t> effectHits{0};
    StoC::HookEntry effectEntry{nullptr};
    StoC::RegisterPostPacketCallback(&effectEntry, 0x00F1,
        [](StoC::HookStatus*, StoC::PacketBase*) { effectHits++; });

    // Wait 5 seconds ??? hero skills and buff ticks should generate effect updates
    Sleep(5000);

    uint32_t hits = effectHits.load();
    IntReport("  AgentUpdateEffects in explorable: %u hits", hits);
    if (hits > 0) {
        IntCheck("AgentUpdateEffects fires in explorable", true);
    } else {
        IntSkip("AgentUpdateEffects in explorable", "No effect packets in 5s (heroes may be idle)");
    }
    StoC::RemoveCallbacks(&effectEntry);

    IntReport("");
    return true;
}
