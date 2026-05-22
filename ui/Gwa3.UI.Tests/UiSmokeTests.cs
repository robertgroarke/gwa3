using Gwa3.UI.Core.Models;
using Gwa3.UI.Core.Profiles;
using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;
using Gwa3.UI.Supervisor.Services;
using Xunit;

using CoreLaunchMode = Gwa3.UI.Core.Models.LaunchMode;
using SupervisorLaunchMode = Gwa3.UI.Supervisor.Models.LaunchMode;

namespace Gwa3.UI.Tests;

public sealed class UiSmokeTests
{
    [Fact]
    public async Task DefaultProfileResolvesLaneSafeLaunchPlan()
    {
        var (profile, profilePath, repoRoot) = await LoadDefaultProfileAsync();

        Verify(profile.InventoryPolicy.UpgradeSalvageRules.Any(rule =>
            rule.Enabled &&
            rule.SalvageIndex == 2 &&
            rule.ItemTypes.Contains("staff", StringComparer.OrdinalIgnoreCase) &&
            rule.ItemTypes.Contains("wand", StringComparer.OrdinalIgnoreCase) &&
            string.Equals(rule.ModifierPattern, "22500140828", StringComparison.OrdinalIgnoreCase)),
            "Default profile should expose the GWA2 HCT20 staff/wand inscription salvage matrix row.");
        Verify(profile.InventoryPolicy.UpgradeSalvageRules.Any(rule =>
            rule.Enabled &&
            rule.SalvageIndex == 1 &&
            rule.ItemTypes.Contains("runesAndInsignias", StringComparer.OrdinalIgnoreCase) &&
            string.Equals(rule.ModifierPattern, "C202EA27", StringComparison.OrdinalIgnoreCase)),
            "Default profile should expose the GWA2 Superior Vigor rune salvage matrix row.");

        var plan = CreateDryRunLaunchPlan(profile, profilePath, repoRoot);
        var validation = new LaunchPlanValidator().Validate(plan);
        Verify(validation.IsValid, string.Join(Environment.NewLine, validation.Errors));
        Assert.Equal(2, plan.AccountIndex);
        Assert.Equal("trade-helper", plan.LaneTag, ignoreCase: true);
        Assert.Equal("launch_sample_three_via_gwlauncher.au3", Path.GetFileName(plan.LauncherScriptPath), ignoreCase: true);
        Verify(!plan.LauncherScriptPath.Contains("sample-five", StringComparison.OrdinalIgnoreCase), "SAMPLE THREE launch plan must never resolve the SAMPLE FIVE launcher script.");

        var mismatchedPlan = plan with
        {
            AccountIndex = 4,
            LaneTag = "sample-five",
            DllName = "gwa3_sample_five.dll",
            LauncherScriptPath = Path.Combine(Path.GetDirectoryName(plan.LauncherScriptPath) ?? "", "launch_sample_five_via_gwlauncher.au3"),
            Bridge = plan.Bridge with
            {
                PipeName = @"\\.\pipe\gwa3_llm_sample-five",
                LaneTag = "sample-five",
                ExpectedDllName = "gwa3_sample_five.dll"
            }
        };
        var mismatchedValidation = new LaunchPlanValidator().Validate(mismatchedPlan);
        Verify(!mismatchedValidation.IsValid, "Validator should block a SAMPLE THREE profile from launching the SAMPLE FIVE lane.");
        Verify(mismatchedValidation.Errors.Any(error => error.Contains("launch_sample_three_via_gwlauncher.au3", StringComparison.OrdinalIgnoreCase)), "Validator should call out the expected SAMPLE THREE launcher script.");
    }

    [Fact]
    public async Task SupervisorDryRunAndCommandsStayLaneSafe()
    {
        var (profile, profilePath, repoRoot) = await LoadDefaultProfileAsync();
        var plan = CreateDryRunLaunchPlan(profile, profilePath, repoRoot);
        var processRunner = new ProcessRunner();
        var statusSink = new InMemorySessionStatusSink();
        var supervisor = new SessionSupervisor(
            new LaunchPlanValidator(),
            new LauncherService(processRunner),
            new ProcessHealthGate(new WindowsProcessProbe()),
            new InjectorService(processRunner),
            new BridgeService(processRunner),
            statusSink);

        var result = await supervisor.StartAsync(plan);
        Verify(result.Succeeded, result.Message);
        Verify(result.DryRun, "Supervisor smoke test must not perform live launch or injection.");
        Assert.Equal(42_000, result.GuildWarsProcessId);
        Verify(statusSink.Events.Any(evt => evt.Stage == SessionStage.Running), "Dry-run session should publish Running stage.");

        var bridgePlan = plan with { Mode = SupervisorLaunchMode.Llm, Bridge = plan.Bridge with { Profile = "qwen-safe" } };
        var bridgeCommand = new BridgeService(processRunner).BuildCommand(bridgePlan);
        Verify(bridgeCommand.Arguments.Contains("--profile"), "Bridge command must pass the configured LLM launch profile.");
        Verify(bridgeCommand.Arguments.Contains("qwen-safe"), "Bridge command must include the configured LLM launch profile value.");
        Verify(bridgeCommand.Arguments.Contains("--llm-url"), "Bridge command must pass the LLM endpoint with the Python bridge's --llm-url flag.");
        Verify(!bridgeCommand.Arguments.Contains("--endpoint"), "Bridge command must not use unsupported --endpoint flag.");
        Verify(bridgeCommand.Arguments.Contains("--llm-hourly-token-cap"), "Bridge command must pass the configured token cap.");
        Verify(!bridgeCommand.Arguments.Contains("--allow-remote-llm"), "Default local bridge profile must not opt into remote LLM usage.");

        var liveBridgeRunner = new RecordingProcessRunner();
        var liveBridgePlan = bridgePlan with { DryRun = false };
        var liveBridge = await new BridgeService(liveBridgeRunner).StartAsync(liveBridgePlan);
        Verify(liveBridge.Succeeded, liveBridge.Message);
        Assert.Equal(1, liveBridgeRunner.StartCount);
        Assert.Equal(0, liveBridgeRunner.RunCount);

        var injectorCommand = new InjectorService(processRunner).BuildCommand(plan, result.GuildWarsProcessId!.Value);
        Verify(injectorCommand.Arguments.Contains("--pid"), "Injector command must include explicit PID argument.");
        Verify(!injectorCommand.Arguments.Contains("Gw.exe", StringComparer.OrdinalIgnoreCase), "Injector command must not target a guessed process name.");
    }

    [Fact]
    public async Task SessionSupervisorTerminatesOwnedClientAfterStartupFailures()
    {
        await VerifyOwnedClientCleanupAsync(
            SessionStage.FailedHealthGate,
            12_345,
            new HealthGateResult
            {
                Succeeded = false,
                ProcessId = 12_345,
                Message = "health failed"
            },
            new InjectionResult { Succeeded = true, Message = "not reached" },
            BridgeResult.Disabled());

        await VerifyOwnedClientCleanupAsync(
            SessionStage.FailedInjector,
            23_456,
            new HealthGateResult
            {
                Succeeded = true,
                ProcessId = 23_456,
                Message = "healthy"
            },
            InjectionResult.Failed("injector failed"),
            BridgeResult.Disabled());

        await VerifyOwnedClientCleanupAsync(
            SessionStage.FailedBridge,
            23_456,
            new HealthGateResult
            {
                Succeeded = true,
                ProcessId = 23_456,
                Message = "healthy"
            },
            new InjectionResult { Succeeded = true, Message = "injected" },
            BridgeResult.Failed("bridge failed"));
    }

    [Fact]
    public void LogRoutingAndSnapshotStoreRoundTrip()
    {
        Verify(LogStreamRouter.ShouldShowInLaunchSummary("Launch", "Live launch started."), "Launch events should appear in the launch summary.");
        Verify(LogStreamRouter.ShouldShowInLaunchSummary("Supervisor", "Injecting: injector.exe"), "Supervisor events should appear in the launch summary.");
        Verify(LogStreamRouter.ShouldShowInLaunchSummary("Launcher", "GWLAUNCHER_PID=13588"), "Launcher PID lines should appear in the launch summary.");
        Verify(LogStreamRouter.ShouldShowInLaunchSummary("Launcher", "LAUNCH_VERIFIED=1"), "Launcher verification lines should appear in the launch summary.");
        Verify(!LogStreamRouter.ShouldShowInLaunchSummary("DLL", "CtoS: EnqueueBotshubCommand size=16"), "Raw DLL runtime lines should not appear in the launch summary.");
        Verify(!LogStreamRouter.ShouldShowInLaunchSummary("Bot", "[BOT] State transition: Traveling -> InDungeon"), "Raw bot runtime lines should not appear in the launch summary.");
        Verify(LogStreamRouter.ShouldShowInRuntimeTail("DLL"), "DLL lines should appear in the runtime tail.");
        Verify(LogStreamRouter.ShouldShowInRuntimeTail("Bot"), "Bot lines should appear in the runtime tail.");
        Verify(LogStreamRouter.ShouldShowInRuntimeTail("Launcher"), "Launcher lines should appear in the runtime tail.");
        Verify(!LogStreamRouter.ShouldShowInRuntimeTail("Profile"), "Profile bookkeeping should not appear in the runtime tail.");

        var snapshotDirectory = Path.Combine(Path.GetTempPath(), "gwa3-ui-tests", Guid.NewGuid().ToString("N"));
        var snapshotPath = Path.Combine(snapshotDirectory, "ui_live_status.json");
        var snapshotStore = UiLiveStatusSnapshotStore.FromSnapshotPath(snapshotPath);
        var snapshotTimestamp = DateTimeOffset.Parse("2026-05-15T21:03:44-07:00");
        snapshotStore.WriteLatest(new UiLiveStatusSnapshot
        {
            Timestamp = snapshotTimestamp,
            EventName = "runtime-progress",
            Profile = "Froggy HM - SAMPLE THREE",
            Character = "GWA3 SAMPLE THREE",
            Lane = "codex-ui",
            Bot = "froggy-hm",
            Status = "InDungeon",
            BotPhase = "InDungeon",
            Pid = "40392",
            Health = "alive",
            LastEvent = "Bogroot post wp=2(3)",
            RunCount = "3",
            SuccessCount = "2",
            FailureCount = "1",
            FailureRatio = "33.3%",
            Gold = "char 68,328 / storage 791,813",
            Heartbeat = "map 616 gameThreadResponsive=1",
            RuntimeStartedAt = snapshotTimestamp.AddMinutes(-92),
            CurrentRunStartedAt = snapshotTimestamp.AddMinutes(-21),
            TotalRuntime = "1:32:00",
            CurrentRunTime = "00:21:09",
            BestRunTime = "00:28:09",
            AverageRunTime = "00:28:26",
            DungeonLevel = "Bogroot Growths level 2",
            CompletionPercent = 74,
            CompletionText = "74%",
            ProgressDetail = "Bogroot post wp 2/3",
            DeldrimorPoints = "1,200",
            AsuraPoints = "3,400",
            Lockpicks = "7",
            ChestsOpened = "5",
            BotshubIpcStatus = "updated 5/15 21:03:44",
            BotshubRunStats = "runs 3 / success 2 / fail 1",
            BotshubInventorySummary = "free slots 9, salvage kits 2",
            BotshubMaintenanceSummary = "maintenance: salvage, sell",
            RuntimeCurrentStep = "InDungeon",
            RuntimeMap = "map 616",
            RuntimeHealth = "alive hp 0.94",
            RuntimePosition = "pos (1200,-350), dist 540",
            RuntimeTarget = "nearest 31, nearby 4",
            RuntimeActionQueue = "queued idx 558",
            Detail = "Bogroot route telemetry"
        });
        var rehydratedSnapshot = Required(snapshotStore.TryReadLatest(), "UI live status snapshot should round-trip through the typed store.");
        Assert.Equal("runtime-progress", rehydratedSnapshot.EventName);
        Assert.Equal("40392", rehydratedSnapshot.Pid);
        Assert.Equal(74, rehydratedSnapshot.CompletionPercent);
        Assert.Equal("74%", rehydratedSnapshot.CompletionText);
        Verify(rehydratedSnapshot.BotshubInventorySummary.Contains("free slots", StringComparison.OrdinalIgnoreCase), "Snapshot store should preserve Botshub inventory monitoring.");
        Verify(rehydratedSnapshot.RuntimeActionQueue.Contains("idx 558", StringComparison.OrdinalIgnoreCase), "Snapshot store should preserve Runtime action queue monitoring.");
        Directory.Delete(snapshotDirectory, recursive: true);
    }

    [Fact]
    public void BotshubWebIpcStatusParserReadsRuntimeState()
    {
        var botshubStatus = BotshubWebIpcStatusParser.TryParse("""
        {
          "character": "GWA3 SAMPLE THREE",
          "script": "Froggy_HM_v1.6",
          "pid": 35980,
          "timestamp": 1774869889,
          "uptime_seconds": 331,
          "bot_running": true,
          "map_id": 857,
          "gold": { "character": 68328, "storage": 791813 },
          "state": "maintenance",
          "settings": {
            "add_heroes": true,
            "consets": false,
            "buy_consets": true,
            "hero_config": "Standard"
          },
          "stats": {
            "run_count": 3,
            "success_count": 2,
            "fail_count": 1,
            "success_ratio": "66.7%",
            "best_run_time": "00:28:09",
            "avg_run_time": "00:28:26",
            "current_run_time": "00:21:09",
            "total_time": "01:31:00",
            "time_per_run": "00:30:20",
            "experience": 1200,
            "chests": 4,
            "gold_items": 6
          },
          "titles": { "deldrimor": 1200, "asura": 3400 },
          "loot": { "lockpicks": 2, "ectos": 1, "gold_items": 6 },
          "materials": { "dust": 30, "iron": 12 },
          "inventory": { "free_slots": 9, "salvage_kits": 2, "ident_kits": 1 },
          "maintenance": { "salvage": true, "sell": true, "craft_consets": false },
          "log": ["Start pressed", "Crafting 25 Grails at Eyja..."]
        }
        """);
        botshubStatus = Required(botshubStatus, "Botshub WebIPC status JSON should parse.");
        Assert.Equal("GWA3 SAMPLE THREE", botshubStatus.Character);
        Verify(botshubStatus.BotRunning == true, "Botshub parser should extract bot_running.");
        Assert.Equal(68328, botshubStatus.CharacterGold);
        Assert.Equal(791813, botshubStatus.StorageGold);
        Assert.Equal(3, botshubStatus.RunCount);
        Assert.Equal(2, botshubStatus.SuccessCount);
        Assert.Equal(1, botshubStatus.FailCount);
        Assert.Equal("66.7%", botshubStatus.SuccessRatio);
        Assert.Equal(4, botshubStatus.Chests);
        Assert.Equal(6, botshubStatus.GoldItems);
        Verify((botshubStatus.SettingsSummary ?? "").Contains("add heroes", StringComparison.OrdinalIgnoreCase), "Botshub parser should summarize enabled settings.");
        Verify((botshubStatus.TitleSummary ?? "").Contains("deldrimor", StringComparison.OrdinalIgnoreCase), "Botshub parser should summarize title counters.");
        Verify((botshubStatus.LootSummary ?? "").Contains("lockpicks", StringComparison.OrdinalIgnoreCase), "Botshub parser should summarize loot counters.");
        Verify((botshubStatus.MaterialSummary ?? "").Contains("dust", StringComparison.OrdinalIgnoreCase), "Botshub parser should summarize material counters.");
        Verify((botshubStatus.InventorySummary ?? "").Contains("free slots", StringComparison.OrdinalIgnoreCase), "Botshub parser should summarize inventory counters.");
        Verify((botshubStatus.MaintenanceSummary ?? "").Contains("maintenance", StringComparison.OrdinalIgnoreCase), "Botshub parser should summarize maintenance status.");
        Verify((botshubStatus.LastLogLine ?? "").Contains("Grails", StringComparison.OrdinalIgnoreCase), "Botshub parser should expose the last ring-buffer log.");
    }

    [Fact]
    public void FroggyProgressParserReadsDungeonTelemetry()
    {
        var sparkflyProgress = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-15 18:31:54] [INFO] Froggy: Sparkfly post wp=8(9) map=558 loaded=1 alive=1"), "Sparkfly route telemetry should parse.");
        Verify(sparkflyProgress.DungeonLevel.Contains("Sparkfly", StringComparison.OrdinalIgnoreCase), "Sparkfly level should be identified.");
        Verify(sparkflyProgress.CompletionPercent is > 10 and <= 15, "Sparkfly final waypoint should land near the entry progress boundary.");

        var level1Progress = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-15 19:21:58] [INFO] Froggy: Bogroot pre wp=22(20) map=615 loaded=1 alive=1"), "Bogroot level 1 telemetry should parse.");
        Verify(level1Progress.DungeonLevel.Contains("level 1", StringComparison.OrdinalIgnoreCase), "Level 1 should be identified from map 615.");
        Verify(level1Progress.CompletionPercent is > 55 and < 70, "Late level 1 waypoint should show mid-to-late run progress.");

        var level2Progress = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-15 19:24:35] [INFO] Froggy: Bogroot post wp=2(3) map=616 loaded=1 alive=1"), "Bogroot level 2 telemetry should parse.");
        Verify(level2Progress.DungeonLevel.Contains("level 2", StringComparison.OrdinalIgnoreCase), "Level 2 should be identified from map 616.");
        Verify(level2Progress.CompletionPercent is >= 70 and < 80, "Early level 2 waypoint should sit just past the level 2 boundary.");

        var level2Heartbeat = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-15 19:24:28] [INFO] [WATCHDOG] runtime alive map=616 gameThreadResponsive=1"), "Map heartbeat should provide a coarse dungeon level.");
        Verify(!level2Heartbeat.IsPrecise, "Heartbeat-only progress should be marked coarse.");
        Assert.Equal(70, level2Heartbeat.CompletionPercent);

        Assert.Null(DungeonProgressParser.TryParseFroggyProgress("[2026-05-15 20:30:58] [INFO] [WATCHDOG] runtime alive map=638 gameThreadResponsive=1"));

        var completeProgress = Required(DungeonProgressParser.TryParseFroggyProgress("[19:06:23] [BOT] Run #1 complete in 1689359 ms (best: 1689359 ms finalMap=638)"), "Completion log should parse.");
        Assert.Equal(100, completeProgress.CompletionPercent);

        var chestProgress = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-16 12:11:39] [INFO] Froggy: OpenChestAt result signpost=223 picked=2"), "Chest-open completion progress should parse.");
        Assert.Equal(99, chestProgress.CompletionPercent);
        Assert.Equal("Chest looted", chestProgress.ProgressDetail);

        var rewardProgress = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-16 12:11:51] [INFO] Froggy: Boss reward QuestReward cleared=1 questPresent=0"), "Reward acceptance progress should parse.");
        Assert.Equal(100, rewardProgress.CompletionPercent);
        Assert.Equal("Quest reward accepted", rewardProgress.ProgressDetail);

        var returnWaitProgress = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-16 12:12:04] [INFO] Froggy transition poll elapsed=10031 map=616 loaded=1 myId=34 sawUnload=0"), "Post-reward transition wait should parse.");
        Assert.Equal(100, returnWaitProgress.CompletionPercent);
        Assert.Equal("Waiting for return", returnWaitProgress.ProgressDetail);

        var postRewardProgress = Required(DungeonProgressParser.TryParseFroggyProgress("[2026-05-16 12:12:14] [INFO] Froggy: Boss post-reward reward salvage result salvaged=0 rewardClaimed=1 rewardLatched=1"), "Post-reward maintenance progress should parse.");
        Assert.Equal("Waiting for return", postRewardProgress.ProgressDetail);
    }

    [Fact]
    public void FroggyMonitoringParserReadsCounters()
    {
        var runStart = Required(FroggyMonitoringParser.TryParseRunStart("[19:41:31] [BOT] State: TownSetup (run #3)"), "TownSetup run marker should parse.");
        Assert.Equal(3, runStart.RunNumber);

        var runCompletion = Required(FroggyMonitoringParser.TryParseRunCompletion("[19:41:31] [BOT] Run #2 complete in 1723047 ms (best: 1689359 ms finalMap=638)"), "Run completion marker should parse.");
        Assert.Equal(2, runCompletion.RunNumber);
        Assert.Equal(1_723_047, runCompletion.DurationMilliseconds);
        Assert.Equal(1_689_359, runCompletion.BestMilliseconds);

        Assert.Equal(1, FroggyMonitoringParser.TryParseOpenedChestIncrement("[2026-05-15 19:24:04] [INFO] Froggy: OpenChestAt result signpost=1737 picked=3"));

        var monitoringStats = Required(FroggyMonitoringParser.TryParseMonitoringStats("[2026-05-15 19:24:05] [INFO] Froggy: MonitoringStats reason=loot titleReady=1 deldrimor=11 asura=22 norn=33 vanguard=44 lockpicks=7 wipes=1 rareSkins=2 goldItems=3 droppedLockpicks=4 chestsOpened=5 blackDyes=6 tomes=8"), "Native monitoring snapshot should parse.");
        Assert.Equal(11, monitoringStats.DeldrimorPoints);
        Assert.Equal(7, monitoringStats.Lockpicks);
        Assert.Equal(3, monitoringStats.GoldItems);
        Assert.Equal(5, monitoringStats.ChestsOpened);
        Assert.Equal(8, monitoringStats.Tomes);
        Verify(!FroggyMonitoringParser.IsWipeLine("[2026-05-16 21:07:54] [INFO] Froggy: MonitoringStats reason=waiting-return lockpicks=361 wipes=1"),
            "Native monitoring lines must not be double-counted as inferred wipe events.");
        Verify(FroggyMonitoringParser.IsWipeLine("[2026-05-16 20:30:12] [WARN] Froggy: party wipe detected; return to outpost requested"),
            "Actual wipe event lines should still be detected.");

        var extendedMonitoringStats = Required(FroggyMonitoringParser.TryParseMonitoringStats("[2026-05-15 23:55:05] [INFO] Froggy: MonitoringStats reason=route-move titleReady=1 deldrimor=1 asura=2 norn=3 vanguard=4 lockpicks=12 wipes=0 rareSkins=0 goldItems=1 droppedLockpicks=2 chestsOpened=3 blackDyes=0 tomes=1 deldrimorTotal=1100 asuraTotal=2200 nornTotal=3300 vanguardTotal=4400 goldCharacter=68328 goldStorage=791813 experience=1234567 mapId=615 freeSlots=8 idKits=2 salvageKits=4 regularSalvageKits=1 highGradeSalvageKits=3 consetsInventory=6 consetsStorage=60 grailsInventory=2 essencesInventory=2 armorsInventory=2 grailsStorage=20 essencesStorage=20 armorsStorage=20 dustInventory=30 ironInventory=40 bonesInventory=50 feathersInventory=60 dustStorage=300 ironStorage=400 bonesStorage=500 feathersStorage=600 skillbarReady=1 skillbarNonZero=8 skill1=1 skill2=2 skill3=3 skill4=4 skill5=5 skill6=6 skill7=7 skill8=8 itemsPicked=9 skinsPicked=4 lockpicksGained=5 inventoryBaselineReady=1"), "Extended native monitoring snapshot should parse.");
        Assert.Equal(68328, extendedMonitoringStats.CharacterGold);
        Assert.Equal(791813, extendedMonitoringStats.StorageGold);
        Assert.Equal(1100, extendedMonitoringStats.DeldrimorTotal);
        Assert.Equal(4400, extendedMonitoringStats.VanguardTotal);
        Assert.Equal(8, extendedMonitoringStats.FreeSlots);
        Assert.Equal(3, extendedMonitoringStats.HighGradeSalvageKits);
        Assert.Equal(60, extendedMonitoringStats.ConsetsStorage);
        Assert.Equal(300, extendedMonitoringStats.DustStorage);
        Verify(extendedMonitoringStats.SkillbarReady == true, "Monitoring parser should extract skillbar readiness.");
        Assert.Equal(8, extendedMonitoringStats.SkillbarNonZero);
        Assert.Equal(8, extendedMonitoringStats.SkillbarSkillIds.Count);
        Assert.Equal(8, extendedMonitoringStats.SkillbarSkillIds[7]);
        Assert.Equal(9, extendedMonitoringStats.ItemsPicked);
        Assert.Equal(4, extendedMonitoringStats.SkinsPicked);
        Assert.Equal(5, extendedMonitoringStats.LockpicksGained);
        Verify(extendedMonitoringStats.InventoryBaselineReady == true, "Monitoring parser should extract inventory baseline readiness.");
    }

    [Fact]
    public void RuntimeLogParserReadsActionTelemetry()
    {
        var runtimeRoute = Required(RuntimeLogParser.TryParse(
            "Bot",
            "[2026-05-15 20:54:11] [INFO] Froggy: Bogroot post wp=10(28) map=615 loaded=1 alive=1 hp=0.94 pos=(1200,-350) distToWp=540 nearestEnemy=31 nearbyEnemies=4"), "Runtime-style route telemetry should parse.");
        Assert.Equal("map 615", runtimeRoute.Map);
        Verify((runtimeRoute.Health ?? "").Contains("alive", StringComparison.OrdinalIgnoreCase), "Runtime parser should expose health state.");
        Verify((runtimeRoute.Position ?? "").Contains("dist 540", StringComparison.OrdinalIgnoreCase), "Runtime parser should expose waypoint distance.");
        Verify((runtimeRoute.Target ?? "").Contains("nearby 4", StringComparison.OrdinalIgnoreCase), "Runtime parser should expose nearby enemy count.");

        var runtimeQueue = Required(RuntimeLogParser.TryParse(
            "DLL",
            "[2026-05-15 20:54:27] [INFO] CtoS: EnqueueBotshubCommand size=16 idx=558 fn=0x555EC820 defer=0 deferCmd=0 exec=558 hb=91305"), "Runtime-style action queue telemetry should parse.");
        Verify((runtimeQueue.ActionQueue ?? "").Contains("idx 558", StringComparison.OrdinalIgnoreCase), "Runtime parser should expose queued action id.");

        var runtimeCasting = Required(RuntimeLogParser.TryParse(
            "DLL",
            "[2026-05-15 20:54:04] [INFO] SkillMgr: UseSkill packet slot=4 skillId=2100 target=31"), "Runtime-style casting telemetry should parse.");
        Verify((runtimeCasting.Casting ?? "").Contains("skill 2100", StringComparison.OrdinalIgnoreCase), "Runtime parser should summarize skill casting.");
    }

    private static async Task VerifyOwnedClientCleanupAsync(
        SessionStage expectedStage,
        int expectedTerminatedProcessId,
        HealthGateResult healthGateResult,
        InjectionResult injectionResult,
        BridgeResult bridgeResult)
    {
        var plan = CreateMinimalLaunchPlan(dryRun: false);
        var terminator = new RecordingProcessTerminator();
        var statusSink = new InMemorySessionStatusSink();
        var supervisor = new SessionSupervisor(
            new StubLaunchPlanValidator(ValidationResult.Success()),
            new StubLauncherService(12_345),
            new StubHealthGate(healthGateResult),
            new StubInjector(injectionResult),
            new StubBridge(bridgeResult),
            statusSink,
            terminator);

        var result = await supervisor.StartAsync(plan);
        Assert.False(result.Succeeded);
        Assert.Equal(expectedStage, result.FinalStage);
        Assert.Contains(expectedTerminatedProcessId, terminator.TerminatedProcessIds);
        Verify(statusSink.Events.Any(evt => evt.Stage == expectedStage && evt.Severity == StatusSeverity.Warning && evt.Message.Contains("Stopped owned Guild Wars PID", StringComparison.OrdinalIgnoreCase)),
            "Supervisor should publish a cleanup warning when it stops an owned client after startup failure.");
    }

    private static async Task<(Gwa3Profile Profile, string ProfilePath, string RepoRoot)> LoadDefaultProfileAsync()
    {
        var repoRoot = ResolveRepositoryRoot();
        var profilePath = Path.Combine(repoRoot, "ui", "profiles", "defaults", "froggy-hm.default.json");
        Verify(File.Exists(profilePath), $"Expected default profile at {profilePath}");

        var store = new ProfileStore();
        var load = await store.LoadAndValidateAsync(profilePath);
        Verify(load.Validation.IsValid, string.Join(Environment.NewLine, load.Validation.Errors.Select(error => error.Message)));
        return (load.Profile, profilePath, repoRoot);
    }

    private static LaunchPlan CreateDryRunLaunchPlan(Gwa3Profile profile, string profilePath, string repoRoot)
    {
        var launcherScript = ResolvePath(repoRoot, profile.Launch.LauncherScriptPath);
        return new LaunchPlan
        {
            ProfileName = profile.ProfileName,
            DryRun = true,
            Mode = ToSupervisorLaunchMode(profile.Launch.LaunchMode),
            AccountIndex = profile.Character.AccountIndex ?? 0,
            CharacterName = profile.Character.CharacterName,
            LaneTag = profile.Launch.LaneTag,
            AutoItExecutablePath = ResolveAutoItExecutablePath(),
            LauncherScriptPath = launcherScript,
            LauncherWorkingDirectory = Path.GetDirectoryName(launcherScript),
            BuildDirectory = ResolvePath(repoRoot, profile.Launch.BuildDirectory),
            InjectorPath = ResolvePath(repoRoot, profile.Launch.InjectorPath),
            DllName = profile.Launch.DllName,
            BotModule = profile.Bot.ModuleId,
            ResolvedProfilePath = profilePath,
            Bridge = new BridgePipeMetadata
            {
                PipeName = profile.Launch.PipeName,
                LaneTag = profile.Launch.LaneTag,
                ExpectedDllName = profile.Launch.DllName,
                BridgeWorkingDirectory = repoRoot,
                Endpoint = profile.Llm.Endpoint,
                Model = profile.Llm.Model,
                Profile = profile.Llm.Profile,
                HourlyTokenCap = profile.Llm.HourlyTokenCap,
                AllowRemote = profile.Llm.AllowRemote
            },
            HealthGate = new HealthGateOptions
            {
                Timeout = TimeSpan.FromSeconds(profile.Launch.HealthGate.TimeoutSeconds),
                PollInterval = TimeSpan.FromMilliseconds(profile.Launch.HealthGate.PollIntervalMilliseconds),
                MinimumWorkingSetBytes = profile.Launch.HealthGate.RequiredWorkingSetKilobytes * 1024L,
                PostHealthySettle = TimeSpan.FromMilliseconds(profile.Launch.HealthGate.PostHealthySettleMilliseconds),
                TreatDryRunAsHealthy = true
            }
        };
    }

    private static LaunchPlan CreateMinimalLaunchPlan(bool dryRun) =>
        new()
        {
            SessionId = "test-session",
            ProfileName = "Test",
            DryRun = dryRun,
            Mode = SupervisorLaunchMode.Llm,
            AccountIndex = 0,
            CharacterName = "GWA3 SAMPLE ONE",
            LaneTag = "sample-one",
            AutoItExecutablePath = "AutoIt3.exe",
            LauncherScriptPath = "launch_sample_one_via_gwlauncher.au3",
            BuildDirectory = "build_sample-one",
            InjectorPath = "injector.exe",
            DllName = "gwa3_sample_one.dll",
            Bridge = new BridgePipeMetadata
            {
                PipeName = @"\\.\pipe\gwa3_llm_sample-one",
                LaneTag = "sample-one",
                ExpectedDllName = "gwa3_sample_one.dll"
            },
            HealthGate = new HealthGateOptions
            {
                Timeout = TimeSpan.FromMilliseconds(1),
                PollInterval = TimeSpan.FromMilliseconds(1),
                MinimumWorkingSetBytes = 1,
                TreatDryRunAsHealthy = true
            }
        };

    private static string ResolvePath(string repoRoot, string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return "";
        }

        return Path.IsPathRooted(path) ? path : Path.GetFullPath(Path.Combine(repoRoot, path));
    }

    private static SupervisorLaunchMode ToSupervisorLaunchMode(CoreLaunchMode mode) =>
        mode switch
        {
            CoreLaunchMode.Llm => SupervisorLaunchMode.Llm,
            CoreLaunchMode.Advisory => SupervisorLaunchMode.Advisory,
            _ => SupervisorLaunchMode.Bot
        };

    private static string ResolveAutoItExecutablePath()
    {
        var candidates = new[]
        {
            @"C:\Program Files (x86)\AutoIt3\AutoIt3.exe",
            @"C:\Program Files\AutoIt3\AutoIt3.exe"
        };

        return candidates.FirstOrDefault(File.Exists) ?? candidates[0];
    }

    private static string ResolveRepositoryRoot()
    {
        var start = new DirectoryInfo(Environment.CurrentDirectory);
        while (start is not null)
        {
            if (Directory.Exists(Path.Combine(start.FullName, "ui", "profiles", "defaults")))
            {
                return start.FullName;
            }

            start = start.Parent;
        }

        throw new InvalidOperationException("Could not find repository root from current directory.");
    }

    private static void Verify(bool condition, string message) => Assert.True(condition, message);

    private static T Required<T>(T? value, string message)
        where T : class
    {
        Verify(value is not null, message);
        return value!;
    }

    private sealed class RecordingProcessRunner : IProcessRunner
    {
        public int RunCount { get; private set; }
        public int StartCount { get; private set; }

        public Task<ProcessCommandResult> RunAsync(ProcessCommand command, CancellationToken cancellationToken = default)
        {
            RunCount++;
            return Task.FromResult(ProcessCommandResult.DryRunSuccess(command));
        }

        public Task<ProcessCommandResult> StartAsync(ProcessCommand command, CancellationToken cancellationToken = default)
        {
            StartCount++;
            return Task.FromResult(new ProcessCommandResult
            {
                Succeeded = true,
                ProcessId = 43_000,
                Command = command
            });
        }
    }

    private sealed class RecordingProcessTerminator : IProcessTerminator
    {
        public List<int> TerminatedProcessIds { get; } = [];

        public Task TerminateAsync(int processId, CancellationToken cancellationToken = default)
        {
            TerminatedProcessIds.Add(processId);
            return Task.CompletedTask;
        }
    }

    private sealed class StubLaunchPlanValidator : ILaunchPlanValidator
    {
        private readonly ValidationResult _result;

        public StubLaunchPlanValidator(ValidationResult result)
        {
            _result = result;
        }

        public ValidationResult Validate(LaunchPlan plan) => _result;
    }

    private sealed class StubLauncherService : ILauncherService
    {
        private readonly int _processId;

        public StubLauncherService(int processId)
        {
            _processId = processId;
        }

        public ProcessCommand BuildCommand(LaunchPlan plan) => new();

        public Task<LauncherResult> LaunchAsync(LaunchPlan plan, CancellationToken cancellationToken = default) =>
            Task.FromResult(new LauncherResult
            {
                Succeeded = true,
                GuildWarsProcessId = _processId,
                Message = "launched"
            });
    }

    private sealed class StubHealthGate : IProcessHealthGate
    {
        private readonly HealthGateResult _result;

        public StubHealthGate(HealthGateResult result)
        {
            _result = result;
        }

        public Task<HealthGateResult> WaitForHealthyAsync(LaunchPlan plan, int guildWarsProcessId, CancellationToken cancellationToken = default) =>
            Task.FromResult(_result);
    }

    private sealed class StubInjector : IInjectorService
    {
        private readonly InjectionResult _result;

        public StubInjector(InjectionResult result)
        {
            _result = result;
        }

        public ProcessCommand BuildCommand(LaunchPlan plan, int guildWarsProcessId) => new();

        public Task<InjectionResult> InjectAsync(LaunchPlan plan, int guildWarsProcessId, CancellationToken cancellationToken = default) =>
            Task.FromResult(_result);
    }

    private sealed class StubBridge : IBridgeService
    {
        private readonly BridgeResult _result;

        public StubBridge(BridgeResult result)
        {
            _result = result;
        }

        public ProcessCommand BuildCommand(LaunchPlan plan) => new();

        public Task<BridgeResult> StartAsync(LaunchPlan plan, CancellationToken cancellationToken = default) =>
            Task.FromResult(_result);
    }
}
