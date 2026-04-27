#include <gwa3/dungeon/DungeonRuntime.h>

#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>

namespace GWA3::DungeonRuntime {

namespace {

template <typename Predicate>
bool WaitForPredicate(uint32_t timeoutMs, Predicate&& predicate, uint32_t pollMs = 250u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (predicate()) {
            return true;
        }
        Sleep(pollMs);
    }
    return predicate();
}

const char* ContextOrDefault(const char* context) {
    return context ? context : "Outpost";
}

const char* TransitionContextOrDefault(const char* context, const char* fallback) {
    return context ? context : fallback;
}

bool IsListedMap(uint32_t mapId, const uint32_t* mapIds, int mapCount) {
    if (!mapIds || mapCount <= 0) return false;
    for (int i = 0; i < mapCount; ++i) {
        if (mapIds[i] == mapId) return true;
    }
    return false;
}

const char* LogPrefixOrDefault(const char* logPrefix) {
    return logPrefix ? logPrefix : "DungeonRuntime";
}

} // namespace

bool IsDead() {
    auto* me = AgentMgr::GetMyAgent();
    return !me || me->hp <= 0.0f;
}

bool IsMapLoaded() {
    return MapMgr::GetIsMapLoaded();
}

void WaitMs(uint32_t ms) {
    Sleep(ms);
}

bool WaitForCondition(uint32_t timeoutMs, const std::function<bool()>& predicate, uint32_t pollMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (predicate && predicate()) {
            return true;
        }
        Sleep(pollMs);
    }
    return predicate && predicate();
}

void SuspendTransitionSensitiveHooks(const char* context) {
    AgentMgr::ResetMoveState(TransitionContextOrDefault(context, "Dungeon transition suspend"));
    CtoS::SuspendEngineHook();
    DialogMgr::Shutdown();
}

void ResumeTransitionSensitiveHooks(const char* context) {
    AgentMgr::ResetMoveState(TransitionContextOrDefault(context, "Dungeon transition resume"));
    CtoS::ResumeEngineHook();
    DialogMgr::Initialize();
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs) {
    return WaitForPredicate(timeoutMs, [mapId]() {
        return MapMgr::GetMapId() == mapId &&
               MapMgr::GetIsMapLoaded() &&
               AgentMgr::GetMyId() > 0u;
    });
}

bool WaitForTownRuntimeReady(uint32_t mapId, uint32_t timeoutMs) {
    return WaitForPredicate(timeoutMs, [mapId]() {
        if (MapMgr::GetMapId() != mapId ||
            !MapMgr::GetIsMapLoaded() ||
            AgentMgr::GetMyId() == 0u ||
            AgentMgr::GetMyAgent() == nullptr) {
            return false;
        }
        auto* inv = ItemMgr::GetInventory();
        if (!inv) return false;
        return inv->bags[1] != nullptr;
    });
}

bool EnsureOutpostReady(uint32_t outpostMapId, uint32_t timeoutMs, const char* context) {
    const uint32_t currentMapId = MapMgr::GetMapId();
    const uint32_t loadingState = MapMgr::GetLoadingState();
    if (currentMapId == outpostMapId) {
        const bool ready = WaitForMapReady(outpostMapId, timeoutMs);
        Log::Info("DungeonRuntime: %s outpost already selected map=%u ready=%d loading=%u",
                  ContextOrDefault(context),
                  outpostMapId,
                  ready ? 1 : 0,
                  loadingState);
        return ready;
    }

    Log::Info("DungeonRuntime: %s traveling to outpost map=%u from map=%u loading=%u",
              ContextOrDefault(context),
              outpostMapId,
              currentMapId,
              loadingState);
    if (!MapMgr::Travel(outpostMapId)) {
        Log::Warn("DungeonRuntime: %s travel request rejected target=%u currentMap=%u loading=%u",
                  ContextOrDefault(context),
                  outpostMapId,
                  currentMapId,
                  loadingState);
        return false;
    }

    const bool ready = WaitForMapReady(outpostMapId, timeoutMs);
    Log::Info("DungeonRuntime: %s outpost travel result ready=%d currentMap=%u loading=%u",
              ContextOrDefault(context),
              ready ? 1 : 0,
              MapMgr::GetMapId(),
              MapMgr::GetLoadingState());
    return ready;
}

bool PushUntilMapReady(
    uint32_t targetMapId,
    float pushX,
    float pushY,
    uint32_t transitionTimeoutMs,
    uint32_t loadTimeoutMs,
    uint32_t settleMs,
    const char* context) {
    const char* transitionContext = TransitionContextOrDefault(context, "Dungeon transition");
    SuspendTransitionSensitiveHooks(transitionContext);
    const bool transitioned = WaitForCondition(transitionTimeoutMs, [targetMapId, pushX, pushY]() {
        if (MapMgr::GetMapId() == targetMapId) {
            return true;
        }
        AgentMgr::Move(pushX, pushY);
        return false;
    }, 250u);
    ResumeTransitionSensitiveHooks(transitionContext);

    if (!transitioned) {
        return false;
    }
    if (settleMs > 0u) {
        WaitMs(settleMs);
    }
    return WaitForMapReady(targetMapId, loadTimeoutMs);
}

bool StageAndPushUntilMapReady(
    uint32_t targetMapId,
    float stageX,
    float stageY,
    float stageThreshold,
    float pushX,
    float pushY,
    MoveToPointFn move_to_point,
    uint32_t prePushDelayMs,
    uint32_t transitionTimeoutMs,
    uint32_t loadTimeoutMs,
    uint32_t settleMs,
    const char* context) {
    if (move_to_point) {
        (void)move_to_point(stageX, stageY, stageThreshold);
    } else {
        AgentMgr::Move(stageX, stageY);
    }
    AgentMgr::Move(pushX, pushY);
    if (prePushDelayMs > 0u) {
        WaitMs(prePushDelayMs);
    }
    return PushUntilMapReady(
        targetMapId,
        pushX,
        pushY,
        transitionTimeoutMs,
        loadTimeoutMs,
        settleMs,
        context);
}

bool ExecuteMapTransitionMove(
    float x,
    float y,
    uint32_t targetMapId,
    VoidMoveToPointFn moveToPoint,
    QueueMoveFn queueMove,
    GetMapIdFn getMapId,
    WaitMsFn waitMs,
    uint32_t timeoutMs,
    uint32_t pulseDelayMs,
    float settleThreshold) {
    if (moveToPoint == nullptr || queueMove == nullptr || getMapId == nullptr || waitMs == nullptr) {
        return false;
    }

    moveToPoint(x, y, settleThreshold);
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        queueMove(x, y);
        waitMs(pulseDelayMs);
        if (getMapId() == targetMapId) {
            return true;
        }
    }

    return getMapId() == targetMapId;
}

bool WaitForPostDungeonReturn(
    uint32_t expectedMapId,
    uint32_t transitionTimeoutMs,
    uint32_t loadTimeoutMs,
    const uint32_t* dungeonMapIds,
    int dungeonMapCount,
    const char* logPrefix) {
    const char* prefix = LogPrefixOrDefault(logPrefix);
    Log::Info("%s waiting for post-dungeon transition expectedMap=%u", prefix, expectedMapId);
    DWORD start = GetTickCount();
    DWORD lastLogAt = 0u;
    uint32_t lastMapId = 0xFFFFFFFFu;
    int lastLoaded = -1;
    uint32_t lastMyId = 0xFFFFFFFFu;
    bool sawUnload = false;
    bool leftDungeonState = false;

    while ((GetTickCount() - start) < transitionTimeoutMs) {
        const uint32_t mapId = MapMgr::GetMapId();
        const bool loaded = MapMgr::GetIsMapLoaded();
        const uint32_t myId = AgentMgr::GetMyId();
        const DWORD elapsed = GetTickCount() - start;

        if (mapId != lastMapId || static_cast<int>(loaded ? 1 : 0) != lastLoaded ||
            myId != lastMyId || (elapsed - lastLogAt) >= 5000u) {
            Log::Info("%s transition poll elapsed=%lu map=%u loaded=%d myId=%u sawUnload=%d",
                      prefix,
                      static_cast<unsigned long>(elapsed),
                      mapId,
                      loaded ? 1 : 0,
                      myId,
                      sawUnload ? 1 : 0);
            lastMapId = mapId;
            lastLoaded = loaded ? 1 : 0;
            lastMyId = myId;
            lastLogAt = elapsed;
        }

        if (mapId == expectedMapId && loaded && myId > 0u) {
            Log::Info("%s reached expected map=%u during transition wait", prefix, mapId);
            return true;
        }

        if (mapId == 0u || !loaded || myId == 0u) {
            sawUnload = true;
            leftDungeonState = true;
        } else if (!IsListedMap(mapId, dungeonMapIds, dungeonMapCount)) {
            leftDungeonState = true;
        }

        if (leftDungeonState) {
            break;
        }

        Sleep(250u);
    }

    Log::Info("%s transition state leftDungeonState=%d map=%u loaded=%d myId=%u",
              prefix,
              leftDungeonState ? 1 : 0,
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              AgentMgr::GetMyId());

    const DWORD loadStart = GetTickCount();
    lastLogAt = 0u;
    lastMapId = 0xFFFFFFFFu;
    lastLoaded = -1;
    lastMyId = 0xFFFFFFFFu;
    while ((GetTickCount() - loadStart) < loadTimeoutMs) {
        const uint32_t mapId = MapMgr::GetMapId();
        const bool loaded = MapMgr::GetIsMapLoaded();
        const uint32_t myId = AgentMgr::GetMyId();
        const DWORD elapsed = GetTickCount() - loadStart;

        if (mapId != lastMapId || static_cast<int>(loaded ? 1 : 0) != lastLoaded ||
            myId != lastMyId || (elapsed - lastLogAt) >= 5000u) {
            Log::Info("%s load poll elapsed=%lu map=%u loaded=%d myId=%u",
                      prefix,
                      static_cast<unsigned long>(elapsed),
                      mapId,
                      loaded ? 1 : 0,
                      myId);
            lastMapId = mapId;
            lastLoaded = loaded ? 1 : 0;
            lastMyId = myId;
            lastLogAt = elapsed;
        }

        if (mapId == expectedMapId && loaded && myId > 0u) {
            Log::Info("%s loaded=1 finalMap=%u myId=%u", prefix, mapId, myId);
            return true;
        }

        if (mapId != 0u && loaded && myId > 0u &&
            !IsListedMap(mapId, dungeonMapIds, dungeonMapCount) &&
            mapId != expectedMapId) {
            Log::Info("%s landed on unexpected loaded map=%u myId=%u", prefix, mapId, myId);
            return false;
        }

        Sleep(250u);
    }

    Log::Info("%s loaded=0 finalMap=%u myId=%u",
              prefix,
              MapMgr::GetMapId(),
              AgentMgr::GetMyId());
    return false;
}

} // namespace GWA3::DungeonRuntime
