// Froggy checkpoint replay helpers. Included by FroggyHMWaypointTraversal.h.

static bool MoveFroggyWaypointForCheckpointReplay(const Waypoint& waypoint) {
    MoveFroggyRouteWaypoint(waypoint);
    return true;
}

static void LogQuestDoorCheckpointReplayWaypoint(const Waypoint* wps, int count, int waypointIndex) {
    LogFroggyWaypointState("quest-door-backtrack", wps, count, waypointIndex);
}

static void LogQuestDoorCheckpointDiagnostic(const Waypoint* wps, int waypointIndex) {
    auto* me = AgentMgr::GetMyAgent();
    const float myX = me ? me->x : 0.0f;
    const float myY = me ? me->y : 0.0f;
    const float distToWp = me ? AgentMgr::GetDistance(myX, myY,
                                                      wps[waypointIndex].x,
                                                      wps[waypointIndex].y) : -1.0f;
    LogBot("Quest Door Checkpoint diag: pos=(%.0f, %.0f) distToCheckpoint=%.0f alive=%d mapLoaded=%d",
           myX,
           myY,
           distToWp,
           me && me->hp > 0.0f ? 1 : 0,
           IsMapLoaded() ? 1 : 0);
}
