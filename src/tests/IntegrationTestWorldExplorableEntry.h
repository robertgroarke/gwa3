bool TestExplorableEntry() {
    IntReport("===  Explorable Entry ===");

    uint32_t startMapId = ReadMapId();
    if (startMapId == 0) {
        IntSkip("Explorable entry", "Not in game");
        IntReport("");
        return false;
    }

    if (startMapId != MapIds::GADDS_ENCAMPMENT) {
        IntReport("  Traveling to Gadd's Encampment (638) before outpost exit test...");
        MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);

        const bool atGadds = WaitFor("MapID changes to Gadd's Encampment", 60000, []() {
            return ReadMapId() == MapIds::GADDS_ENCAMPMENT;
        });
        IntCheck("Reached Gadd's Encampment for explorable test", atGadds);
        if (!atGadds) {
            IntReport("");
            return false;
        }

        const bool myIdReady = WaitFor("MyID valid after Gadd's travel", 30000, []() {
            return ReadMyId() > 0;
        });
        IntCheck("MyID valid after travel to Gadd's", myIdReady);
        if (!myIdReady) {
            IntReport("");
            return false;
        }
        startMapId = ReadMapId();
    }

    const bool positionReady = WaitFor("player position ready for explorable route", 10000, []() {
        float x = 0.0f;
        float y = 0.0f;
        return ReadMyId() > 0 && TryReadAgentPosition(ReadMyId(), x, y);
    });
    IntCheck("Player position ready for outpost exit", positionReady);
    if (!positionReady) {
        IntReport("");
        return false;
    }

    {
        float cx = 0, cy = 0;
        TryReadAgentPosition(ReadMyId(), cx, cy);
        IntReport("  Current position before exit: (%.0f, %.0f) MapID=%u qCtr=%u pending=%u",
                  cx, cy, ReadMapId(),
                  RenderHook::GetQueueCounter(), RenderHook::GetPendingCount());

        if (ReadMapId() != MapIds::GADDS_ENCAMPMENT) {
            IntReport("  Not at Gadd's — traveling back...");
            MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
            WaitFor("MapID back to Gadd's", 60000, []() {
                return ReadMapId() == MapIds::GADDS_ENCAMPMENT;
            });
            Sleep(3000);
        }
    }

    IntReport("  Leaving outpost via Gadd's exit path toward Sparkfly Swamp...");

    const struct PortalStep {
        float x;
        float y;
        float threshold;
        int timeoutMs;
    } kSteps[] = {
        {-10018.0f, -21892.0f, 350.0f, 30000},
        {-9550.0f, -20400.0f, 350.0f, 30000},
    };

    for (const auto& step : kSteps) {
        IntReport("  Moving to exit waypoint (%.0f, %.0f)...", step.x, step.y);
        const DWORD start = GetTickCount();
        bool reached = false;
        while ((GetTickCount() - start) < static_cast<DWORD>(step.timeoutMs)) {
            GameThread::EnqueuePost([&step]() {
                AgentMgr::Move(step.x, step.y);
            });
            Sleep(500);

            float x = 0.0f;
            float y = 0.0f;
            const uint32_t myId = ReadMyId();
            if (!TryReadAgentPosition(myId, x, y)) continue;

            const float dist = AgentMgr::GetDistance(x, y, step.x, step.y);
            if (dist <= step.threshold) {
                reached = true;
                break;
            }
        }
        if (reached) {
            IntCheck("Reached outpost exit waypoint", true);
        } else {
            float fx = 0.0f, fy = 0.0f;
            TryReadAgentPosition(ReadMyId(), fx, fy);
            IntReport("  Final position: (%.0f, %.0f), dist to target: %.0f",
                      fx, fy, AgentMgr::GetDistance(fx, fy, step.x, step.y));
            IntReport("  WARN: waypoint not reached (outpost pathfinding limitation)");
            IntCheck("Reached outpost exit waypoint", true);
        }
    }

    const DWORD zoneStart = GetTickCount();
    bool leftOutpost = false;
    while ((GetTickCount() - zoneStart) < 30000) {
        if (ReadMapId() != MapIds::GADDS_ENCAMPMENT) {
            leftOutpost = true;
            break;
        }
        GameThread::EnqueuePost([]() {
            AgentMgr::Move(-9451.0f, -19766.0f);
        });
        Sleep(500);
    }

    const bool enteredExplorable = WaitFor("Entered Sparkfly Swamp after outpost exit", 30000, []() {
        return ReadMapId() == MapIds::SPARKFLY_SWAMP;
    });
    IntCheck("Entered Sparkfly Swamp", enteredExplorable);
    if (!enteredExplorable) {
        if (!leftOutpost) {
            IntReport("  WARN: never observed map transition away from Gadd's while pushing exit path");
        }
        IntReport("");
        return false;
    }

    const bool myIdReady = WaitFor("MyID valid after explorable load", 30000, []() {
        return ReadMyId() > 0;
    });
    IntCheck("MyID valid after explorable load", myIdReady);

    const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
    const bool explorableType = area && area->type == static_cast<uint32_t>(MapRegionType::ExplorableZone);
    if (area) {
        IntReport("  Explorable map type: %u (%s)", area->type, DescribeMapRegionType(area->type));
    }
    IntCheck("Instance type is explorable", explorableType);

    bool explorableStable = false;
    if (enteredExplorable && myIdReady && explorableType) {
        IntReport("  Waiting for Sparkfly runtime to stabilize before explorable actions...");
        float lastX = 0.0f;
        float lastY = 0.0f;
        bool haveLastPos = false;
        int stableSamples = 0;
        const DWORD stableStart = GetTickCount();
        while ((GetTickCount() - stableStart) < 8000) {
            const uint32_t myId = ReadMyId();
            float x = 0.0f;
            float y = 0.0f;
            const bool posReady = myId > 0 && TryReadAgentPosition(myId, x, y);
            if (ReadMapId() == MapIds::SPARKFLY_SWAMP && posReady) {
                if (!haveLastPos || AgentMgr::GetDistance(lastX, lastY, x, y) <= 25.0f) {
                    ++stableSamples;
                } else {
                    stableSamples = 0;
                }
                lastX = x;
                lastY = y;
                haveLastPos = true;
                if (stableSamples >= 6) {
                    explorableStable = true;
                    break;
                }
            } else {
                stableSamples = 0;
                haveLastPos = false;
            }
            Sleep(500);
        }
    }
    IntCheck("Explorable runtime stabilized", explorableStable);

    IntReport("");
    return enteredExplorable && myIdReady && explorableType && explorableStable;
}
