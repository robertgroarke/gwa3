// Froggy dungeon progression state handler. Included by FroggyHM.cpp.

BotState HandleDungeon(BotConfig& cfg) {
    uint32_t mapId = MapMgr::GetMapId();

    // Refresh combat cache on explorable entry rather than relying on town-setup state.
    RefreshCombatSkillbar();

    if (mapId == MapIds::SPARKFLY_SWAMP) {
        LogBot("State: Sparkfly Swamp - preparing Bogroot entry");

        // Renew consets at Sparkfly entry.
        UseConsumables(cfg);

        if (!MoveToTekksFromSparkflyCurrentSide()) {
            LogBot("Sparkfly Tekks approach failed; staying in Sparkfly for retry");
            Log::Warn("Froggy: skipping ACTION_CANCEL after Sparkfly approach failure; retry will issue fresh movement");
            WaitMs(1000);
            return BotState::InDungeon;
        }

        if (!PrepareTekksDungeonEntry()) {
            LogBot("Tekks dungeon entry preparation failed; staying in Sparkfly for retry");
            (void)RecordTekksQuestEntryFailureAndMaybeResetDialog("sparkfly-state");
            Log::Warn("Froggy: skipping ACTION_CANCEL after Tekks prep failure; retry will refresh dialog/movement");
            WaitMs(1000);
            return BotState::InDungeon;
        }
        ResetTekksQuestEntryFailures("tekks-entry-prepared");

        if (!EnterBogrootFromSparkfly()) {
            LogBot("Sparkfly dungeon portal transition failed; staying in Sparkfly for retry");
            Log::Warn("Froggy: skipping ACTION_CANCEL after portal transition failure; retry will issue fresh movement");
            WaitMs(1000);
            return BotState::InDungeon;
        }
    }

    mapId = MapMgr::GetMapId();

    if (mapId == MapIds::BOGROOT_GROWTHS_LVL1 || mapId == MapIds::BOGROOT_GROWTHS_LVL2) {
        s_runCount++;
        s_runStartTime = GetTickCount();

        const bool completed = RunDungeonLoopFromCurrentMap();
        const DungeonLoopTelemetry telemetry = GetDungeonLoopTelemetry();
        if (completed) {
            DWORD runTime = GetTickCount() - s_runStartTime;
            if (runTime < s_bestRunTime) s_bestRunTime = runTime;
            LogBot("Run #%u complete in %u ms (best: %u ms finalMap=%u)",
                   s_runCount,
                   runTime,
                   s_bestRunTime,
                   telemetry.final_map_id);

            if (telemetry.final_map_id == MapIds::SPARKFLY_SWAMP) {
                MaintenanceMgr::Config maintenanceCfg = MakeFroggyMaintenanceConfig(
                    cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT);
                const auto sparkflyDecision = ResolvePostSparkflyRunDecision(
                    MaintenanceMgr::NeedsMaintenance(maintenanceCfg));
                if (sparkflyDecision.maintenance_deferred) {
                    LogBot("Maintenance needed after run; deferring town maintenance and preserving Sparkfly loop");
                }
                LogBot("Run returned to Sparkfly; re-entering Bogroot without town reset");
                return sparkflyDecision.next_state;
            }

            if (telemetry.final_map_id == (cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT)) {
                return BotState::InTown;
            }
        }

        if (telemetry.final_map_id == MapIds::SPARKFLY_SWAMP) {
            LogBot("Bogroot loop returned to Sparkfly before completion; retrying entry");
            return BotState::InDungeon;
        }

        LogBot("Bogroot loop did not complete cleanly (finalMap=%u); retrying dungeon state",
               telemetry.final_map_id);
        WaitMs(1000);
        return BotState::InDungeon;
    }

    // If we ended up back in outpost (wipe/resign), restart
    if (mapId == (cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT)) {
        s_failCount++;
        LogBot("Run failed (returned to outpost), restarting");
        return BotState::InTown;
    }

    return BotState::InDungeon;
}
