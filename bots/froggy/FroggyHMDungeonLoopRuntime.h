// Froggy Bogroot dungeon-loop runtime. Used by both the bot state machine and
// debug/test entrypoints so live Froggy follows the same route behavior that
// integration runs validate.

void ResetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry = {};
    ResetOpenedChestTracker("dungeon-loop-start");
}

DungeonLoopTelemetry GetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    return s_dungeonLoopTelemetry;
}

static bool RunDungeonLoopFromCurrentMap() {
    ResetDungeonLoopTelemetry();

    // Level 1 can occasionally bounce back to Sparkfly at the quest-door
    // checkpoint before the route reaches level 2. Treat that as a refreshable
    // entry failure for a few passes instead of terminating the loop
    // immediately on the second return-to-Sparkfly observation.
    constexpr int kMaxSparkflyRefreshRetriesBeforeLvl2 = 3;
    int refreshRetries = 0;
    while (true) {
        const uint32_t mapId = MapMgr::GetMapId();
        Log::Info("Froggy: Bogroot loop iteration map=%u refreshRetries=%d enteredLvl2=%d finalMap=%u",
                  mapId,
                  refreshRetries,
                  s_dungeonLoopTelemetry.entered_lvl2 ? 1 : 0,
                  s_dungeonLoopTelemetry.final_map_id);
        if (mapId == MapIds::BOGROOT_GROWTHS_LVL1) {
            s_dungeonLoopTelemetry.started_in_lvl1 = true;
            FollowWaypoints(BOGROOT_LVL1, BOGROOT_LVL1_COUNT, true);
            Log::Info("Froggy: Bogroot loop after lvl1 map=%u lastWp=%u(%s) returnedToSparkfly=%d",
                      MapMgr::GetMapId(),
                      s_dungeonLoopTelemetry.last_waypoint_index,
                      s_dungeonLoopTelemetry.last_waypoint_label,
                      s_dungeonLoopTelemetry.returned_to_sparkfly ? 1 : 0);
        } else if (mapId == MapIds::BOGROOT_GROWTHS_LVL2) {
            s_dungeonLoopTelemetry.started_in_lvl2 = true;
        } else if (mapId == MapIds::SPARKFLY_SWAMP &&
                   !s_dungeonLoopTelemetry.entered_lvl2 &&
                   refreshRetries < kMaxSparkflyRefreshRetriesBeforeLvl2) {
            ++refreshRetries;
            Log::Info("Froggy: Bogroot loop refreshing via Sparkfly retry=%d", refreshRetries);
            if (!PrepareTekksDungeonEntry()) {
                Log::Info("Froggy: Bogroot loop refresh aborted because PrepareTekksDungeonEntry failed");
                (void)RecordTekksQuestEntryFailureAndMaybeResetDialog("bogroot-loop-refresh");
                s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
                s_dungeonLoopTelemetry.returned_to_sparkfly =
                    s_dungeonLoopTelemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
                return false;
            }
            ResetTekksQuestEntryFailures("bogroot-loop-refresh-prepared");
            if (!EnterBogrootFromSparkfly()) {
                Log::Info("Froggy: Bogroot loop refresh aborted because EnterBogrootFromSparkfly failed");
                s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
                s_dungeonLoopTelemetry.returned_to_sparkfly =
                    s_dungeonLoopTelemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
                return false;
            }
            continue;
        } else if (mapId == MapIds::SPARKFLY_SWAMP && !s_dungeonLoopTelemetry.entered_lvl2) {
            s_dungeonLoopTelemetry.final_map_id = mapId;
            s_dungeonLoopTelemetry.returned_to_sparkfly = true;
            Log::Info("Froggy: Bogroot loop exhausted Sparkfly refresh retries before level 2 retryLimit=%d",
                      kMaxSparkflyRefreshRetriesBeforeLvl2);
            return false;
        } else if (mapId == MapIds::SPARKFLY_SWAMP && s_dungeonLoopTelemetry.entered_lvl2) {
            s_dungeonLoopTelemetry.final_map_id = mapId;
            s_dungeonLoopTelemetry.returned_to_sparkfly = true;
            Log::Info("Froggy: Bogroot loop completed with Sparkfly return after level 2");
            return true;
        } else {
            s_dungeonLoopTelemetry.final_map_id = mapId;
            Log::Info("Froggy: Bogroot loop exiting on unsupported map=%u", mapId);
            return false;
        }

        if (MapMgr::GetMapId() == MapIds::BOGROOT_GROWTHS_LVL2) {
            s_dungeonLoopTelemetry.started_in_lvl2 = true;
            const bool lvl2Ready = WaitForBogrootLvl2SpawnReady(10000);
            auto* me = AgentMgr::GetMyAgent();
            Log::Info("Froggy: Bogroot lvl2 route gate ready=%d player=(%.0f, %.0f) nearestLvl2Wp=%d",
                      lvl2Ready ? 1 : 0,
                      me ? me->x : 0.0f,
                      me ? me->y : 0.0f,
                      GetNearestWaypointIndex(BOGROOT_LVL2, BOGROOT_LVL2_COUNT));
            WaitForLocalPositionSettle(1500, 24.0f);
            FollowWaypoints(BOGROOT_LVL2, BOGROOT_LVL2_COUNT, true);
            Log::Info("Froggy: Bogroot loop after lvl2 map=%u bossStarted=%d bossCompleted=%d",
                      MapMgr::GetMapId(),
                      s_dungeonLoopTelemetry.boss_started ? 1 : 0,
                      s_dungeonLoopTelemetry.boss_completed ? 1 : 0);
        }

        if (MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
            if (MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT &&
                s_dungeonLoopTelemetry.entered_lvl2 &&
                s_dungeonLoopTelemetry.boss_completed) {
                Log::Info("Froggy: Bogroot loop completed with Gadd's return after level 2");
                s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
                return true;
            }
            Log::Info("Froggy: Bogroot loop terminating because map=%u (expected Sparkfly for successful return)",
                      MapMgr::GetMapId());
            break;
        }
    }

    s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        s_dungeonLoopTelemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
    return s_dungeonLoopTelemetry.returned_to_sparkfly && s_dungeonLoopTelemetry.entered_lvl2;
}
