bool TestMovement() {
    IntReport("=== Travel + Movement ===");

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
