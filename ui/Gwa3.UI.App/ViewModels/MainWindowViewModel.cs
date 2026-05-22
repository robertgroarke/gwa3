using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using Gwa3.UI.Core.Models;
using Gwa3.UI.Core.Profiles;
using Gwa3.UI.Core.Validation;
using Gwa3.UI.Supervisor.Models;
using Gwa3.UI.Supervisor.Services;

using CoreLaunchMode = Gwa3.UI.Core.Models.LaunchMode;
using CoreLlmMode = Gwa3.UI.Core.Models.LlmMode;
using SupervisorLaunchMode = Gwa3.UI.Supervisor.Models.LaunchMode;

namespace Gwa3.UI.App.ViewModels;

public sealed class MainWindowViewModel : ObservableObject
{
    private readonly ProfileStore profileStore;
    private readonly ProfileValidator profileValidator = new();
    private readonly string profileDirectory;
    private readonly string repositoryRoot;
    private readonly FileLogTailService logTailService = new();
    private readonly UiLiveStatusSnapshotStore statusSnapshotStore;
    private readonly DispatcherTimer runtimeTimer = new() { Interval = TimeSpan.FromSeconds(1) };
    private readonly HashSet<SessionViewModel> observedHealthSessions = new();
    private readonly List<Task> backgroundTasks = [];
    private static readonly string[] UnhealthyHealthTokens =
    {
        "not running",
        "not launched",
        "validation failed",
        "blocked",
        "failed",
        "failure",
        "error",
        "issue",
        "cancelled",
        "stopped",
        "exited",
        "crash",
        "not responding",
        "frozen"
    };

    private static readonly string[] HealthyHealthTokens =
    {
        "alive",
        "healthy",
        "running",
        "dry run ok",
        "launching",
        "injecting",
        "health gate",
        "armed",
        "indungeon",
        "in dungeon",
        "intown",
        "in town",
        "traveling",
        "merchant",
        "looting",
        "maintenance"
    };

    private static readonly IReadOnlyList<CharacterLaunchDefaults> KnownCharacterLaunchDefaults =
    [
        new(
            AccountIndex: 0,
            CharacterName: "GWA3 SAMPLE ONE",
            AccountLabel: "sample-one",
            LaneTag: "sample-one",
            BuildDirectory: "build_sample-one",
            DllName: "gwa3_sample_one.dll",
            PipeName: @"\\.\pipe\gwa3_llm_sample-one",
            LauncherScriptPath: "launch_sample_one_via_gwlauncher.au3"),
        new(
            AccountIndex: 1,
            CharacterName: "GWA3 SAMPLE TWO",
            AccountLabel: "sample-two",
            LaneTag: "sample-two",
            BuildDirectory: "build_sample-two",
            DllName: "gwa3_sample_two.dll",
            PipeName: @"\\.\pipe\gwa3_llm_sample-two",
            LauncherScriptPath: "launch_sample_two_via_gwlauncher.au3"),
        new(
            AccountIndex: 2,
            CharacterName: "GWA3 SAMPLE THREE",
            AccountLabel: "trade-helper",
            LaneTag: "trade-helper",
            BuildDirectory: "build_trade",
            DllName: "gwa3_trade.dll",
            PipeName: @"\\.\pipe\gwa3_llm_trade",
            LauncherScriptPath: "launch_sample_three_via_gwlauncher.au3"),
        new(
            AccountIndex: 3,
            CharacterName: "GWA3 SAMPLE FOUR",
            AccountLabel: "sample-four",
            LaneTag: "sample-four",
            BuildDirectory: "build_sample-four",
            DllName: "gwa3_sample_four.dll",
            PipeName: @"\\.\pipe\gwa3_llm_sample-four",
            LauncherScriptPath: "launch_sample_four_via_gwlauncher.au3"),
        new(
            AccountIndex: 4,
            CharacterName: "GWA3 SAMPLE FIVE",
            AccountLabel: "sample-five",
            LaneTag: "sample-five",
            BuildDirectory: "build_sample-five",
            DllName: "gwa3_sample_five.dll",
            PipeName: @"\\.\pipe\gwa3_llm_sample-five",
            LauncherScriptPath: "launch_sample_five_via_gwlauncher.au3")
    ];

    private readonly record struct DllLogCandidate(int Pid, string LogPath, DateTime LastWriteTimeUtc);

    private Gwa3Profile? currentProfile;
    private string? currentProfilePath;
    private CancellationTokenSource? liveSessionCts;
    private Task? liveLaunchTask;
    private int? liveGuildWarsProcessId;
    private bool liveSessionOwnsGuildWarsProcess;
    private string? liveRuntimeIssue;
    private OptionItem? selectedProfile;
    private OptionItem? selectedBot;
    private OptionItem? selectedCharacter;
    private SessionViewModel? selectedSession;
    private string selectedLaunchMode = "Bot";
    private string selectedModel = "gpt-5";
    private string selectedAutonomyLevel = "Advisory";
    private string llmEndpoint = "http://localhost:11434/v1";
    private string llmMode = "Off";
    private string profileName = "No profile loaded";
    private string playerTemplate = "";
    private string heroConfigFile = "";
    private bool hardMode = true;
    private bool useConsets;
    private bool useStones;
    private bool disableRendering;
    private bool openChests = true;
    private bool pickupGold = true;
    private bool autoIdentify = true;
    private bool autoSalvage = true;
    private bool consetCraftingEnabled;
    private int consetMaterialSlotTrigger = 10;
    private int consetMaterialPressureFreeSlots = 10;
    private int consetBatchSets = 10;
    private string chatDraft = "";
    private string statusBanner = "Loading GWA3 UI profile data.";
    private string fleetHealthBanner = "Health: 0/0 bots healthy";
    private DateTimeOffset lastLegacyBotshubPoll = DateTimeOffset.MinValue;
    private DateTimeOffset lastLiveAttachmentProbe = DateTimeOffset.MinValue;

    private MainWindowViewModel(ProfileStore profileStore, string profileDirectory, string repositoryRoot)
    {
        this.profileStore = profileStore;
        this.profileDirectory = profileDirectory;
        this.repositoryRoot = repositoryRoot;
        statusSnapshotStore = new UiLiveStatusSnapshotStore(repositoryRoot);

        ValidateCommand = new RelayCommand(ValidateProfile);
        SaveProfileCommand = new RelayCommand(SaveProfile);
        DryRunCommand = new RelayCommand(BuildDryRunPlan);
        LaunchCommand = new RelayCommand(StartLiveLaunch);
        StopCommand = new RelayCommand(StopSelectedSession);
        OpenLogFolderCommand = new RelayCommand(OpenLogFolder);
        SendChatCommand = new RelayCommand(SendChat, CanSendChat);

        runtimeTimer.Tick += (_, _) =>
        {
            SelectedSession?.RefreshTimers(DateTimeOffset.Now);
            RefreshLiveAttachmentIfNeeded();
            RefreshFleetHealthBanner();
            UpdateLegacyBotshubStatusFromDisk(throttle: true);
        };
        runtimeTimer.Start();
    }

    public ObservableCollection<OptionItem> Profiles { get; } = new();

    public ObservableCollection<OptionItem> BotOptions { get; } = new();

    public ObservableCollection<OptionItem> CharacterOptions { get; } = new();

    public ObservableCollection<string> LaunchModes { get; } = new();

    public ObservableCollection<string> LlmModes { get; } = new();

    public ObservableCollection<string> ModelOptions { get; } = new();

    public ObservableCollection<string> AutonomyLevels { get; } = new();

    public ObservableCollection<SessionViewModel> Sessions { get; } = new();

    public ObservableCollection<ValidationCheckViewModel> ValidationChecks { get; } = new();

    public ObservableCollection<HeroBuildViewModel> HeroBuilds { get; } = new();

    public ObservableCollection<InventoryPolicyViewModel> InventoryPolicies { get; } = new();

    public ObservableCollection<UpgradeSalvageRuleViewModel> UpgradeSalvageRules { get; } = new();

    public ObservableCollection<MaintenanceSettingViewModel> MaintenanceSettings { get; } = new();

    public ObservableCollection<AllowedActionViewModel> AllowedActions { get; } = new();

    public ObservableCollection<ChatMessageViewModel> ChatMessages { get; } = new();

    public ObservableCollection<LogEntryViewModel> LogEntries { get; } = new();

    public ObservableCollection<LogEntryViewModel> LaunchLogEntries { get; } = new();

    public ObservableCollection<LogEntryViewModel> RuntimeLogEntries { get; } = new();

    public ObservableCollection<DiagnosticRowViewModel> Diagnostics { get; } = new();

    public ICommand ValidateCommand { get; }

    public ICommand SaveProfileCommand { get; }

    public ICommand DryRunCommand { get; }

    public ICommand LaunchCommand { get; }

    public ICommand StopCommand { get; }

    public ICommand OpenLogFolderCommand { get; }

    public ICommand SendChatCommand { get; }

    public OptionItem? SelectedProfile
    {
        get => selectedProfile;
        set
        {
            if (SetProperty(ref selectedProfile, value) && !string.IsNullOrWhiteSpace(value?.Path))
            {
                LoadProfile(value.Path);
            }
        }
    }

    public OptionItem? SelectedBot
    {
        get => selectedBot;
        set => SetProperty(ref selectedBot, value);
    }

    public OptionItem? SelectedCharacter
    {
        get => selectedCharacter;
        set => SetProperty(ref selectedCharacter, value);
    }

    public SessionViewModel? SelectedSession
    {
        get => selectedSession;
        set => SetProperty(ref selectedSession, value);
    }

    public string SelectedLaunchMode
    {
        get => selectedLaunchMode;
        set => SetProperty(ref selectedLaunchMode, value);
    }

    public string SelectedModel
    {
        get => selectedModel;
        set => SetProperty(ref selectedModel, value);
    }

    public string SelectedAutonomyLevel
    {
        get => selectedAutonomyLevel;
        set => SetProperty(ref selectedAutonomyLevel, value);
    }

    public string LlmEndpoint
    {
        get => llmEndpoint;
        set => SetProperty(ref llmEndpoint, value);
    }

    public string LlmMode
    {
        get => llmMode;
        set => SetProperty(ref llmMode, value);
    }

    public string ProfileName
    {
        get => profileName;
        set => SetProperty(ref profileName, value);
    }

    public string PlayerTemplate
    {
        get => playerTemplate;
        set => SetProperty(ref playerTemplate, value);
    }

    public string HeroConfigFile
    {
        get => heroConfigFile;
        set => SetProperty(ref heroConfigFile, value);
    }

    public bool HardMode
    {
        get => hardMode;
        set => SetProperty(ref hardMode, value);
    }

    public bool UseConsets
    {
        get => useConsets;
        set => SetProperty(ref useConsets, value);
    }

    public bool UseStones
    {
        get => useStones;
        set => SetProperty(ref useStones, value);
    }

    public bool DisableRendering
    {
        get => disableRendering;
        set => SetProperty(ref disableRendering, value);
    }

    public bool OpenChests
    {
        get => openChests;
        set => SetProperty(ref openChests, value);
    }

    public bool PickupGold
    {
        get => pickupGold;
        set => SetProperty(ref pickupGold, value);
    }

    public bool AutoIdentify
    {
        get => autoIdentify;
        set => SetProperty(ref autoIdentify, value);
    }

    public bool AutoSalvage
    {
        get => autoSalvage;
        set => SetProperty(ref autoSalvage, value);
    }

    public bool ConsetCraftingEnabled
    {
        get => consetCraftingEnabled;
        set => SetProperty(ref consetCraftingEnabled, value);
    }

    public int ConsetMaterialSlotTrigger
    {
        get => consetMaterialSlotTrigger;
        set => SetProperty(ref consetMaterialSlotTrigger, value);
    }

    public int ConsetMaterialPressureFreeSlots
    {
        get => consetMaterialPressureFreeSlots;
        set => SetProperty(ref consetMaterialPressureFreeSlots, value);
    }

    public int ConsetBatchSets
    {
        get => consetBatchSets;
        set => SetProperty(ref consetBatchSets, value);
    }

    public string ChatDraft
    {
        get => chatDraft;
        set
        {
            if (SetProperty(ref chatDraft, value))
            {
                CommandManager.InvalidateRequerySuggested();
            }
        }
    }

    public string StatusBanner
    {
        get => statusBanner;
        set => SetProperty(ref statusBanner, value);
    }

    public string FleetHealthBanner
    {
        get => fleetHealthBanner;
        set => SetProperty(ref fleetHealthBanner, value);
    }

    public static MainWindowViewModel CreateShellPreview()
    {
        var repositoryRoot = ResolveRepositoryRoot();
        var vm = new MainWindowViewModel(
            new ProfileStore(),
            Path.Combine(repositoryRoot, "ui", "profiles", "defaults"),
            repositoryRoot);

        vm.LoadStaticOptions();
        vm.LoadProfilesFromDisk();
        vm.ValidateProfile();
        vm.AttachLatestRunIfPresent();
        return vm;
    }

    private void LoadStaticOptions()
    {
        foreach (var module in KnownBotModules.All.Order(StringComparer.OrdinalIgnoreCase))
        {
            BotOptions.Add(new OptionItem(module, "GWA3 bot module", module));
        }

        foreach (var character in KnownCharacterLaunchDefaults)
        {
            CharacterOptions.Add(new OptionItem(
                character.CharacterName,
                $"account {character.AccountIndex} / {character.LaneTag} lane",
                character.LaneTag));
        }

        LaunchModes.Add("Bot");
        LaunchModes.Add("LLM");
        LaunchModes.Add("Advisory");

        LlmModes.Add("Off");
        LlmModes.Add("Co-Pilot");
        LlmModes.Add("Advisory");
        LlmModes.Add("Autonomous");

        ModelOptions.Add("gpt-5");
        ModelOptions.Add("gpt-5-mini");
        ModelOptions.Add("deepseek-v4-pro:cloud");
        ModelOptions.Add("local-qwen-coder");

        AutonomyLevels.Add("Observe Only");
        AutonomyLevels.Add("Advisory");
        AutonomyLevels.Add("Ask Before Action");
        AutonomyLevels.Add("Bounded Autonomy");
        AutonomyLevels.Add("Full Autonomy");

        ChatMessages.Add(new ChatMessageViewModel("System", "LLM mode starts in the selected profile's configured safety posture.", false));
        ChatMessages.Add(new ChatMessageViewModel("Operator", "When in doubt, prefer a safe pause over trying to recover mid-run.", true));

        AddLog("UI", "INFO", "Loaded GWA3 control panel.");
    }

    private void LoadProfilesFromDisk()
    {
        Profiles.Clear();

        try
        {
            var summaries = profileStore
                .ListProfilesAsync(profileDirectory)
                .GetAwaiter()
                .GetResult();

            foreach (var summary in summaries)
            {
                var state = summary.IsValid ? "valid" : "needs attention";
                Profiles.Add(new OptionItem(
                    summary.ProfileName,
                    $"{summary.BotModule} / {summary.CharacterName} / {summary.LaneTag} ({state})",
                    summary.BotModule,
                    summary.Path));
            }

            if (Profiles.Count == 0)
            {
                AddLog("Profile", "WARN", $"No profile JSON files found under {profileDirectory}.");
                StatusBanner = "No profile JSON files were found. Add profiles under ui/profiles/defaults.";
                return;
            }

            SelectedProfile = Profiles[0];
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            AddLog("Profile", "ERROR", $"Could not enumerate profiles: {ex.Message}");
            StatusBanner = "Profile directory could not be read.";
        }
    }

    private void LoadProfile(string path)
    {
        try
        {
            var result = profileStore.LoadAndValidateAsync(path).GetAwaiter().GetResult();
            ApplyProfile(result.Profile, path, result.Validation);
            AddLog("Profile", result.Validation.IsValid ? "INFO" : "WARN", $"Loaded {result.Profile.ProfileName}.");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            AddLog("Profile", "ERROR", $"Could not load selected profile: {ex.Message}");
            StatusBanner = "Selected profile could not be loaded.";
        }
    }

    private void ApplyProfile(Gwa3Profile profile, string path, ProfileValidationResult validation)
    {
        currentProfile = profile;
        currentProfilePath = path;
        ApplyKnownCharacterLaunchDefaults(profile, profile.Character.CharacterName);

        ProfileName = profile.ProfileName;
        PlayerTemplate = profile.Build.Player.Skillbar.TemplateCode;
        HeroConfigFile = profile.Build.HeroConfigFile;
        HardMode = profile.Runtime.HardMode;
        UseConsets = profile.Runtime.UseConsets;
        UseStones = profile.Runtime.UseStones;
        DisableRendering = profile.Runtime.DisableRendering;
        OpenChests = profile.Runtime.OpenChests;
        PickupGold = profile.Runtime.PickupGold;
        AutoIdentify = profile.Runtime.AutoIdentify;
        AutoSalvage = profile.Runtime.AutoSalvage;
        ConsetCraftingEnabled = profile.Maintenance.Consets.Enabled;
        ConsetMaterialSlotTrigger = profile.Maintenance.Consets.MaterialSlotTrigger;
        ConsetMaterialPressureFreeSlots = profile.Maintenance.Consets.MaterialPressureFreeSlots;
        ConsetBatchSets = profile.Maintenance.Consets.BatchSets;
        SelectedBot = FindOrAdd(BotOptions, profile.Bot.ModuleId, profile.Bot.DisplayName, profile.Bot.ModuleId);
        SelectedCharacter = FindOrAdd(CharacterOptions, profile.Character.CharacterName, profile.Character.AccountLabel, profile.Launch.LaneTag);
        SelectedLaunchMode = FormatLaunchMode(profile.Launch.LaunchMode);
        LlmMode = FormatLlmMode(profile.Llm.Mode);
        LlmEndpoint = string.IsNullOrWhiteSpace(profile.Llm.Endpoint) ? LlmEndpoint : profile.Llm.Endpoint;
        SelectedModel = EnsureOption(ModelOptions, string.IsNullOrWhiteSpace(profile.Llm.Model) ? SelectedModel : profile.Llm.Model);
        SelectedAutonomyLevel = EnsureOption(AutonomyLevels, FormatAutonomy(profile.Llm.AutonomyLevel));

        LoadHeroBuilds(profile);
        LoadInventoryPolicies(profile);
        LoadMaintenanceSettings(profile);
        LoadAllowedActions(profile);
        UpsertSession(profile);
        LoadDiagnostics(profile);
        PublishProfileValidation(validation);

        StatusBanner = validation.IsValid
            ? $"Profile loaded: {profile.ProfileName}."
            : $"Profile loaded with {validation.Errors.Count} error(s) and {validation.Warnings.Count} warning(s).";
    }

    private void LoadHeroBuilds(Gwa3Profile profile)
    {
        HeroBuilds.Clear();
        HeroBuilds.Add(new HeroBuildViewModel(
            "Player",
            string.IsNullOrWhiteSpace(profile.Character.CharacterName) ? "Player" : profile.Character.CharacterName,
            profile.Build.Player.Skillbar.TemplateCode,
            $"{profile.Build.Player.PrimaryProfession}/{profile.Build.Player.SecondaryProfession}".Trim('/'),
            "Operator"));

        var heroesBySlot = profile.Build.Heroes
            .Where(hero => hero.Slot is >= 1 and <= 7)
            .GroupBy(hero => hero.Slot)
            .ToDictionary(group => group.Key, group => group.First());

        for (var slot = 1; slot <= 7; ++slot)
        {
            heroesBySlot.TryGetValue(slot, out var hero);
            var heroName = hero is null || string.IsNullOrWhiteSpace(hero.HeroName)
                ? $"Hero {slot}"
                : hero.HeroName;
            var template = hero?.Skillbar.TemplateCode ?? "";
            var enabled = hero?.Enabled ?? true;
            var behavior = hero?.Behavior.ToString() ?? HeroBehavior.Guard.ToString();

            HeroBuilds.Add(new HeroBuildViewModel(
                $"Hero {slot}",
                heroName,
                template,
                enabled ? "Enabled" : "Disabled",
                behavior));
        }
    }

    private void LoadInventoryPolicies(Gwa3Profile profile)
    {
        InventoryPolicies.Clear();
        UpgradeSalvageRules.Clear();
        AddRuleSet("Pickup", profile.InventoryPolicy.Pickup);
        AddRuleSet("Identify", profile.InventoryPolicy.Identify);
        AddRuleSet("Salvage Materials", profile.InventoryPolicy.SalvageMaterials);
        AddRuleSet("Salvage Upgrades", profile.InventoryPolicy.SalvageUpgrades);
        AddRuleSet("Sell", profile.InventoryPolicy.Sell);
        AddRuleSet("Store", profile.InventoryPolicy.Store);
        AddRuleSet("Keep Components", profile.InventoryPolicy.KeepComponents);

        foreach (var rule in profile.InventoryPolicy.UpgradeSalvageRules)
        {
            InventoryPolicies.Add(new InventoryPolicyViewModel(
                "Upgrade Rule",
                rule.Name,
                $"{rule.Action} {rule.MinimumRarity}+ / {FormatSalvageIndex(rule.SalvageIndex)}",
                $"{DescribeUpgradeItemTypes(rule.ItemTypes)} / {rule.ModifierPattern}"));

            UpgradeSalvageRules.Add(new UpgradeSalvageRuleViewModel(rule));
        }

        void AddRuleSet(string category, PolicyRuleSet ruleSet)
        {
            if (ruleSet.Rules.Count == 0)
            {
                InventoryPolicies.Add(new InventoryPolicyViewModel(
                    category,
                    string.Join(", ", ruleSet.CategoryTags),
                    ruleSet.Mode.ToString(),
                    "No explicit item rules."));
                return;
            }

            foreach (var rule in ruleSet.Rules)
            {
                InventoryPolicies.Add(new InventoryPolicyViewModel(
                    category,
                    rule.Name,
                    ruleSet.Mode.ToString(),
                    string.IsNullOrWhiteSpace(rule.Notes) ? DescribeItemRule(rule) : rule.Notes));
            }
        }
    }

    private static string FormatSalvageIndex(int salvageIndex) =>
        salvageIndex switch
        {
            0 => "Prefix",
            1 => "Suffix/Rune",
            2 => "Inscription",
            _ => $"Index {salvageIndex}"
        };

    private static string DescribeUpgradeItemTypes(IReadOnlyCollection<string> itemTypes) =>
        itemTypes.Count == 0
            ? "All item types"
            : string.Join(", ", itemTypes);

    private void LoadMaintenanceSettings(Gwa3Profile profile)
    {
        MaintenanceSettings.Clear();
        MaintenanceSettings.Add(new MaintenanceSettingViewModel("Free inventory slots", profile.Maintenance.MinimumFreeSlots.ToString(CultureInfo.InvariantCulture), "Stop or vendor below target", true));
        MaintenanceSettings.Add(new MaintenanceSettingViewModel("Identification kits", profile.Maintenance.Kits.TargetIdentificationKits.ToString(CultureInfo.InvariantCulture), $"Buy below {profile.Maintenance.Kits.MinimumIdentificationKits}", true));
        MaintenanceSettings.Add(new MaintenanceSettingViewModel("Salvage kits", profile.Maintenance.Kits.TargetSalvageKits.ToString(CultureInfo.InvariantCulture), $"Buy below {profile.Maintenance.Kits.MinimumSalvageKits}", true));
        MaintenanceSettings.Add(new MaintenanceSettingViewModel("Expert salvage kits", profile.Maintenance.Kits.TargetExpertSalvageKits.ToString(CultureInfo.InvariantCulture), "Maintain before salvage passes", true));
        MaintenanceSettings.Add(new MaintenanceSettingViewModel("Gold on character", profile.Maintenance.Gold.KeepOnCharacterGold.ToString("N0", CultureInfo.InvariantCulture), $"Deposit at {profile.Maintenance.Gold.DepositWhenCharacterGoldAtLeast:N0}", true));
        MaintenanceSettings.Add(new MaintenanceSettingViewModel("Conset material crafting", profile.Maintenance.Consets.BatchSets.ToString(CultureInfo.InvariantCulture), $"Craft at {profile.Maintenance.Consets.MaterialSlotTrigger} material slots", profile.Maintenance.Consets.Enabled));
    }

    private void LoadAllowedActions(Gwa3Profile profile)
    {
        AllowedActions.Clear();
        foreach (var action in new[] { "observe", "chat", "set_bot_goal", "pause_bot", "resume_bot", "move", "use_skill", "inventory" })
        {
            AllowedActions.Add(new AllowedActionViewModel(action, profile.Llm.AllowedActions.Contains(action, StringComparer.OrdinalIgnoreCase)));
        }
    }

    private void UpsertSession(Gwa3Profile profile)
    {
        var existing = Sessions.FirstOrDefault(session =>
            string.Equals(session.LaneTag, profile.Launch.LaneTag, StringComparison.OrdinalIgnoreCase));

        if (existing is null)
        {
            existing = new SessionViewModel(
                profile.Launch.LaneTag,
                profile.Character.CharacterName,
                profile.Bot.ModuleId,
                profile.Launch.DllName,
                profile.Launch.PipeName,
                "Idle",
                "not launched");
            ObserveHealthSession(existing);
            Sessions.Insert(0, existing);
        }
        else
        {
            ObserveHealthSession(existing);
            existing.CharacterName = profile.Character.CharacterName;
            existing.BotModule = profile.Bot.ModuleId;
            existing.DllName = profile.Launch.DllName;
            existing.PipeName = profile.Launch.PipeName;
            existing.Status = "Idle";
            existing.Pid = "not launched";
            existing.Health = "not running";
            existing.ResetExternalMonitoring();
        }

        SelectedSession = existing;
        RefreshFleetHealthBanner();
        UpdateLegacyBotshubStatusFromDisk(throttle: false);
    }

    private void ObserveHealthSession(SessionViewModel session)
    {
        if (!observedHealthSessions.Add(session))
        {
            return;
        }

        session.PropertyChanged += (_, args) =>
        {
            if (IsFleetHealthProperty(args.PropertyName))
            {
                RefreshFleetHealthBanner();
            }
        };
    }

    private static bool IsFleetHealthProperty(string? propertyName) =>
        propertyName is null ||
        propertyName == nameof(SessionViewModel.Status) ||
        propertyName == nameof(SessionViewModel.Pid) ||
        propertyName == nameof(SessionViewModel.Health) ||
        propertyName == nameof(SessionViewModel.BotPhase);

    private void RefreshFleetHealthBanner()
    {
        var total = Sessions.Count;
        if (total == 0)
        {
            FleetHealthBanner = "Health: 0/0 bots healthy";
            return;
        }

        var healthy = Sessions.Count(IsSessionHealthy);
        var percent = (int)Math.Round(healthy * 100d / total, MidpointRounding.AwayFromZero);
        FleetHealthBanner = $"Health: {healthy}/{total} bots healthy ({percent}%)";
    }

    private static bool IsSessionHealthy(SessionViewModel session)
    {
        if (IsUnhealthyText(session.Status) ||
            IsUnhealthyText(session.Health) ||
            IsUnhealthyText(session.BotPhase))
        {
            return false;
        }

        if (int.TryParse(session.Pid, NumberStyles.Integer, CultureInfo.InvariantCulture, out var pid) && pid > 0)
        {
            return true;
        }

        return IsHealthyText(session.Status) ||
            IsHealthyText(session.Health) ||
            IsHealthyText(session.BotPhase);
    }

    private static bool IsUnhealthyText(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var text = value.Trim();
        return UnhealthyHealthTokens.Any(token => text.Contains(token, StringComparison.OrdinalIgnoreCase));
    }

    private static bool IsHealthyText(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var text = value.Trim();
        return HealthyHealthTokens.Any(token => text.Contains(token, StringComparison.OrdinalIgnoreCase));
    }

    private void UpdateLegacyBotshubStatusFromDisk(bool throttle)
    {
        if (SelectedSession is null)
        {
            return;
        }

        var now = DateTimeOffset.Now;
        if (throttle && now - lastLegacyBotshubPoll < TimeSpan.FromSeconds(3))
        {
            return;
        }

        lastLegacyBotshubPoll = now;
        var statusPath = FindLegacyBotshubStatusPath(SelectedSession.CharacterName);
        if (string.IsNullOrWhiteSpace(statusPath))
        {
            SelectedSession.BotshubIpcStatus = "not attached";
            return;
        }

        try
        {
            var snapshot = BotshubWebIpcStatusParser.TryParse(File.ReadAllText(statusPath));
            if (snapshot is null)
            {
                SelectedSession.BotshubIpcStatus = "invalid status.json";
                return;
            }

            ApplyLegacyBotshubStatus(snapshot, statusPath);
        }
        catch (JsonException ex)
        {
            SelectedSession.BotshubIpcStatus = $"invalid status.json: {ex.Message}";
        }
        catch (IOException ex)
        {
            SelectedSession.BotshubIpcStatus = $"read failed: {ex.Message}";
        }
        catch (UnauthorizedAccessException ex)
        {
            SelectedSession.BotshubIpcStatus = $"read failed: {ex.Message}";
        }
    }

    private void ApplyLegacyBotshubStatus(BotshubWebIpcStatus snapshot, string statusPath)
    {
        if (SelectedSession is null)
        {
            return;
        }

        var timestampInstant = snapshot.TimestampUnix is long unix
            ? DateTimeOffset.FromUnixTimeSeconds(unix).ToLocalTime()
            : new DateTimeOffset(File.GetLastWriteTime(statusPath));
        var timestamp = timestampInstant.ToString("M/d HH:mm:ss", CultureInfo.InvariantCulture);
        var age = DateTimeOffset.Now - timestampInstant;
        var legacyIsStale = age > TimeSpan.FromSeconds(30);
        if (legacyIsStale &&
            string.Equals(SelectedSession.BotshubStatusSource, "native Froggy MonitoringStats", StringComparison.OrdinalIgnoreCase))
        {
            if (!SelectedSession.BotshubIpcStatus.StartsWith("native GWA3", StringComparison.OrdinalIgnoreCase))
            {
                SelectedSession.BotshubIpcStatus = "native GWA3 active";
            }

            return;
        }

        SelectedSession.BotshubIpcStatus = $"updated {timestamp} ({FormatMonitorAge(age)} old)";
        SelectedSession.BotshubStatusSource = statusPath;
        SelectedSession.BotshubScript = EmptyToFallback(snapshot.Script, "n/a");
        SelectedSession.BotshubPid = snapshot.ProcessId?.ToString(CultureInfo.InvariantCulture) ?? "n/a";
        SelectedSession.BotshubState = EmptyToFallback(snapshot.State, "n/a");
        SelectedSession.BotshubMapId = snapshot.MapId?.ToString(CultureInfo.InvariantCulture) ?? "n/a";
        SelectedSession.BotshubRunning = snapshot.BotRunning is bool running ? (running ? "running" : "stopped") : "n/a";
        SelectedSession.BotshubUptime = snapshot.UptimeSeconds is int uptime ? FormatMonitorDuration(TimeSpan.FromSeconds(uptime)) : "n/a";
        SelectedSession.BotshubGold = snapshot.CharacterGold is not null || snapshot.StorageGold is not null
            ? $"char {snapshot.CharacterGold?.ToString("N0", CultureInfo.InvariantCulture) ?? "n/a"} / storage {snapshot.StorageGold?.ToString("N0", CultureInfo.InvariantCulture) ?? "n/a"}"
            : "n/a";
        SelectedSession.BotshubSettings = EmptyToFallback(snapshot.SettingsSummary, "n/a");
        SelectedSession.BotshubRuns = FormatMonitorNumber(snapshot.RunCount);
        SelectedSession.BotshubSuccesses = FormatMonitorNumber(snapshot.SuccessCount);
        SelectedSession.BotshubFailures = FormatMonitorNumber(snapshot.FailCount);
        SelectedSession.BotshubSuccessRatio = FormatBotshubSuccessRatio(snapshot);
        SelectedSession.BotshubCurrentRunTime = EmptyToFallback(snapshot.CurrentRunTime, "n/a");
        SelectedSession.BotshubTotalTime = EmptyToFallback(snapshot.TotalTime, "n/a");
        SelectedSession.BotshubBestRunTime = EmptyToFallback(snapshot.BestRunTime, "n/a");
        SelectedSession.BotshubAverageRunTime = EmptyToFallback(snapshot.AverageRunTime, "n/a");
        SelectedSession.BotshubTimePerRun = EmptyToFallback(snapshot.TimePerRun, "n/a");
        SelectedSession.BotshubExperience = FormatMonitorNumber(snapshot.Experience);
        SelectedSession.BotshubChests = FormatMonitorNumber(snapshot.Chests);
        SelectedSession.BotshubGoldItems = FormatMonitorNumber(snapshot.GoldItems);
        SelectedSession.BotshubTitleSummary = EmptyToFallback(snapshot.TitleSummary, "n/a");
        SelectedSession.BotshubLootSummary = EmptyToFallback(snapshot.LootSummary, "n/a");
        SelectedSession.BotshubMaterialSummary = EmptyToFallback(snapshot.MaterialSummary, "n/a");
        SelectedSession.BotshubInventorySummary = EmptyToFallback(snapshot.InventorySummary, "n/a");
        SelectedSession.BotshubMaintenanceSummary = EmptyToFallback(snapshot.MaintenanceSummary, "n/a");
        SelectedSession.BotshubRunStats = $"runs {snapshot.RunCount?.ToString(CultureInfo.InvariantCulture) ?? "n/a"}, fails {snapshot.FailCount?.ToString(CultureInfo.InvariantCulture) ?? "n/a"}, current {EmptyToFallback(snapshot.CurrentRunTime, "n/a")}, total {EmptyToFallback(snapshot.TotalTime, "n/a")}, best {EmptyToFallback(snapshot.BestRunTime, "n/a")}, avg {EmptyToFallback(snapshot.AverageRunTime, "n/a")}";
        SelectedSession.BotshubLastLog = EmptyToFallback(snapshot.LastLogLine, "n/a");
    }

    private string? FindLegacyBotshubStatusPath(string characterName)
    {
        var characterKey = SanitizeCharacterKey(characterName);
        if (string.IsNullOrWhiteSpace(characterKey))
        {
            return null;
        }

        foreach (var root in EnumerateLegacyBotshubRoots())
        {
            var candidate = Path.Combine(root, "ipc", characterKey, "status.json");
            if (File.Exists(candidate))
            {
                return candidate;
            }
        }

        return null;
    }

    private IEnumerable<string> EnumerateLegacyBotshubRoots()
    {
        if (currentProfile is not null)
        {
            var launcherPath = ResolvePath(currentProfile.Launch.LauncherScriptPath);
            var launcherDirectory = Path.GetDirectoryName(launcherPath);
            foreach (var ancestor in EnumerateAncestors(launcherDirectory))
            {
                yield return ancestor;
            }
        }

        foreach (var ancestor in EnumerateAncestors(repositoryRoot))
        {
            yield return ancestor;
        }
    }

    private static IEnumerable<string> EnumerateAncestors(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            yield break;
        }

        var directory = new DirectoryInfo(path);
        while (directory is not null)
        {
            yield return directory.FullName;
            directory = directory.Parent;
        }
    }

    private static string SanitizeCharacterKey(string characterName) =>
        new(characterName.Where(ch => !char.IsWhiteSpace(ch)).Select(char.ToLowerInvariant).ToArray());

    private static string EmptyToFallback(string? value, string fallback) =>
        string.IsNullOrWhiteSpace(value) ? fallback : value.Trim();

    private static string FormatMonitorNumber(int? value) =>
        value?.ToString("N0", CultureInfo.InvariantCulture) ?? "n/a";

    private static string FormatBotshubSuccessRatio(BotshubWebIpcStatus snapshot)
    {
        if (!string.IsNullOrWhiteSpace(snapshot.SuccessRatio))
        {
            return snapshot.SuccessRatio.Trim();
        }

        if (snapshot.RunCount is int runs && runs > 0)
        {
            var successes = snapshot.SuccessCount ?? Math.Max(0, runs - (snapshot.FailCount ?? 0));
            return $"{successes * 100.0 / runs:0.#}%";
        }

        return "n/a";
    }

    private static string FormatMonitorAge(TimeSpan age)
    {
        if (age < TimeSpan.Zero)
        {
            age = TimeSpan.Zero;
        }

        return age.TotalDays >= 1.0
            ? $"{(int)age.TotalDays}d {age.Hours:D2}h"
            : age.TotalHours >= 1.0
            ? $"{(int)age.TotalHours}h {age.Minutes:D2}m"
            : age.TotalMinutes >= 1.0
                ? $"{(int)age.TotalMinutes}m {age.Seconds:D2}s"
                : $"{Math.Max(0, (int)age.TotalSeconds)}s";
    }

    private static string FormatMonitorDuration(TimeSpan elapsed) =>
        elapsed.TotalDays >= 1.0
            ? $"{(int)elapsed.TotalDays}d {elapsed:hh\\:mm\\:ss}"
            : elapsed.ToString("hh\\:mm\\:ss", CultureInfo.InvariantCulture);

    private void LoadDiagnostics(Gwa3Profile profile)
    {
        var plan = CreateLaunchPlan(profile, dryRun: true);
        Diagnostics.Clear();
        Diagnostics.Add(new DiagnosticRowViewModel("Profile", currentProfilePath ?? "", File.Exists(currentProfilePath ?? "") ? "Loaded" : "Missing"));
        Diagnostics.Add(new DiagnosticRowViewModel("Launcher", plan.LauncherScriptPath, File.Exists(plan.LauncherScriptPath) ? "Found" : "Not found"));
        Diagnostics.Add(new DiagnosticRowViewModel("AutoIt", plan.AutoItExecutablePath, File.Exists(plan.AutoItExecutablePath) ? "Found" : "Configured"));
        Diagnostics.Add(new DiagnosticRowViewModel("Injector", plan.InjectorPath, File.Exists(plan.InjectorPath) ? "Found" : "Pending build"));
        Diagnostics.Add(new DiagnosticRowViewModel("DLL", plan.DllName, "Lane locked"));
        Diagnostics.Add(new DiagnosticRowViewModel("Bridge pipe", plan.Bridge.PipeName, "Lane locked"));
        Diagnostics.Add(new DiagnosticRowViewModel("Health gate", $"{plan.HealthGate.MinimumWorkingSetBytes / 1024:N0} KB", "Dry-run safe"));
        Diagnostics.Add(new DiagnosticRowViewModel("Launch mode", plan.Mode.ToString(), "Profile value"));
    }

    private void ValidateProfile()
    {
        if (currentProfile is null)
        {
            ValidationChecks.Clear();
            ValidationChecks.Add(new ValidationCheckViewModel("Profile loaded", "WARN", "No profile is selected."));
            StatusBanner = "Select a profile before validation.";
            return;
        }

        ApplyEditableFieldsToProfile(currentProfile);
        PublishProfileValidation(profileValidator.Validate(currentProfile));
        AddLog("Profile", "INFO", "Validated current profile and launch plan.");
    }

    private void SaveProfile()
    {
        if (currentProfile is null || string.IsNullOrWhiteSpace(currentProfilePath))
        {
            AddLog("Profile", "WARN", "No profile is loaded to save.");
            StatusBanner = "No profile is loaded to save.";
            return;
        }

        try
        {
            ApplyEditableFieldsToProfile(currentProfile);
            var validation = profileValidator.Validate(currentProfile);
            PublishProfileValidation(validation);
            if (!validation.IsValid)
            {
                AddLog("Profile", "ERROR", "Profile was not saved because validation failed.");
                StatusBanner = $"Profile not saved: {validation.Errors.Count} validation error(s).";
                return;
            }

            profileStore.Save(currentProfilePath, currentProfile);
            LoadDiagnostics(currentProfile);
            StatusBanner = $"Saved profile: {currentProfile.ProfileName}.";
            AddLog("Profile", "INFO", $"Saved profile to {currentProfilePath}.");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidDataException or ProfileValidationException)
        {
            AddLog("Profile", "ERROR", $"Could not save profile: {ex.Message}");
            StatusBanner = "Profile could not be saved.";
        }
    }

    private void ApplyEditableFieldsToProfile(Gwa3Profile profile)
    {
        profile.ProfileName = ProfileName.Trim();
        profile.Bot.ModuleId = SelectedBot?.Value ?? profile.Bot.ModuleId;
        profile.Bot.DisplayName = SelectedBot?.Name ?? profile.Bot.DisplayName;
        profile.Character.CharacterName = SelectedCharacter?.Name ?? profile.Character.CharacterName;
        if (!ApplyKnownCharacterLaunchDefaults(profile, profile.Character.CharacterName))
        {
            profile.Launch.LaneTag = SelectedCharacter?.Value ?? profile.Launch.LaneTag;
        }

        profile.Launch.LaunchMode = ParseLaunchMode(SelectedLaunchMode);
        profile.Build.Player.Skillbar.TemplateCode = PlayerTemplate.Trim();
        profile.Build.HeroConfigFile = HeroConfigFile.Trim();
        profile.Runtime.HardMode = HardMode;
        profile.Runtime.UseConsets = UseConsets;
        profile.Runtime.UseStones = UseStones;
        profile.Runtime.DisableRendering = DisableRendering;
        profile.Runtime.OpenChests = OpenChests;
        profile.Runtime.PickupGold = PickupGold;
        profile.Runtime.AutoIdentify = AutoIdentify;
        profile.Runtime.AutoSalvage = AutoSalvage;
        profile.Maintenance.Consets.Enabled = ConsetCraftingEnabled;
        profile.Maintenance.Consets.MaterialSlotTrigger = ConsetMaterialSlotTrigger;
        profile.Maintenance.Consets.MaterialPressureFreeSlots = ConsetMaterialPressureFreeSlots;
        profile.Maintenance.Consets.BatchSets = ConsetBatchSets;
        profile.InventoryPolicy.UpgradeSalvageRules = UpgradeSalvageRules.Select(rule => rule.ToModel()).ToList();
        profile.Llm.Mode = ParseLlmMode(LlmMode);
        profile.Llm.Endpoint = LlmEndpoint.Trim();
        profile.Llm.Model = SelectedModel.Trim();
        profile.Llm.AutonomyLevel = ParseAutonomyLevel(SelectedAutonomyLevel);
        profile.Llm.AllowedActions = AllowedActions
            .Where(action => action.Enabled)
            .Select(action => action.Name)
            .ToList();
    }

    private void PublishProfileValidation(ProfileValidationResult validation)
    {
        ValidationChecks.Clear();
        ValidationChecks.Add(new ValidationCheckViewModel("Profile JSON", validation.IsValid ? "OK" : "ERROR", validation.IsValid ? "Profile schema passed." : "Profile has blocking errors."));

        foreach (var issue in validation.Issues)
        {
            ValidationChecks.Add(new ValidationCheckViewModel(
                issue.Path,
                issue.Severity == ProfileValidationSeverity.Error ? "ERROR" : "WARN",
                issue.Message));
        }

        if (currentProfile is null)
        {
            return;
        }

        var launchValidation = new LaunchPlanValidator().Validate(CreateLaunchPlan(currentProfile, dryRun: true));
        ValidationChecks.Add(new ValidationCheckViewModel("Launch plan", launchValidation.IsValid ? "OK" : "ERROR", launchValidation.IsValid ? "Required lane fields are present." : string.Join("; ", launchValidation.Errors)));

        foreach (var warning in launchValidation.Warnings)
        {
            ValidationChecks.Add(new ValidationCheckViewModel("Launch warning", "WARN", warning));
        }
    }

    private void BuildDryRunPlan()
    {
        if (currentProfile is null || SelectedSession is null)
        {
            AddLog("Launch", "ERROR", "Cannot build a dry-run plan without a selected profile and session.");
            return;
        }

        ApplyEditableFieldsToProfile(currentProfile);
        var plan = CreateLaunchPlan(currentProfile, dryRun: true);
        var validation = new LaunchPlanValidator().Validate(plan);
        if (!validation.IsValid)
        {
            AddLog("Launch", "ERROR", string.Join("; ", validation.Errors));
            PublishProfileValidation(profileValidator.Validate(currentProfile));
            return;
        }

        var statusSink = new InMemorySessionStatusSink();
        statusSink.StatusPublished += (_, status) =>
            AddLog("Supervisor", status.Severity.ToString().ToUpperInvariant(), $"{status.Stage}: {status.Message}");

        var processRunner = new ProcessRunner();
        var supervisor = new SessionSupervisor(
            new LaunchPlanValidator(),
            new LauncherService(processRunner),
            new ProcessHealthGate(new WindowsProcessProbe()),
            new InjectorService(processRunner),
            new BridgeService(processRunner),
            statusSink);

        var result = supervisor.StartAsync(plan).GetAwaiter().GetResult();
        SelectedSession.Status = result.Succeeded ? "Dry Run OK" : result.FinalStage.ToString();
        SelectedSession.Pid = result.GuildWarsProcessId?.ToString(CultureInfo.InvariantCulture) ?? "not launched";
        SelectedSession.Health = result.HealthGate?.Message ?? result.Message;
        StatusBanner = result.Succeeded
            ? $"Dry-run launch plan succeeded for {SelectedSession.CharacterName}."
            : $"Dry-run launch plan failed: {result.Message}";

        AddLog("Launch", result.Succeeded ? "INFO" : "ERROR", result.Message);
    }

    private void StartLiveLaunch()
    {
        if (currentProfile is null || SelectedSession is null)
        {
            AddLog("Launch", "ERROR", "Cannot launch without a selected profile and session.");
            return;
        }

        ApplyEditableFieldsToProfile(currentProfile);
        var plan = CreateLaunchPlan(currentProfile, dryRun: false);
        var validation = new LaunchPlanValidator().Validate(plan);
        if (liveSessionCts is not null)
        {
            AddLog("Launch", "WARN", "A live session is already active. Stop it before launching again.");
            return;
        }

        foreach (var error in validation.Errors)
        {
            AddLog("Launch", "ERROR", error);
        }

        foreach (var warning in validation.Warnings)
        {
            AddLog("Launch", "WARN", warning);
        }

        if (!validation.IsValid)
        {
            SelectedSession.Status = "Blocked";
            SelectedSession.Pid = "not launched";
            SelectedSession.Health = "validation failed";
            SelectedSession.BotPhase = "Blocked";
            StatusBanner = "Live launch plan is blocked by validation errors.";
            WriteStatusSnapshot("launch-blocked", "validation failed");
            return;
        }

        liveSessionCts = new CancellationTokenSource();
        liveGuildWarsProcessId = null;
        liveSessionOwnsGuildWarsProcess = true;
        liveRuntimeIssue = null;
        SelectedSession.Status = "Launching";
        SelectedSession.Pid = "pending GWLauncher";
        SelectedSession.Health = "starting GWLauncher";
        SelectedSession.BotPhase = "Launching";
        SelectedSession.ResetMonitoringStatistics();
        SelectedSession.DungeonLevel = "not in dungeon";
        SelectedSession.CompletionPercent = 0;
        SelectedSession.CompletionText = "0%";
        SelectedSession.ProgressDetail = "waiting for route telemetry";
        SelectedSession.ResetRuntime();
        SelectedSession.MarkRuntimeStarted(DateTimeOffset.Now);
        StatusBanner = $"Launching {plan.CharacterName} through GWLauncher.";
        AddLog("Launch", "ACTION", $"Live launch started for {plan.CharacterName} using {plan.DllName}.");
        WriteStatusSnapshot("launch-started", "GWLauncher");

        StartLogTail(Path.ChangeExtension(plan.LauncherScriptPath, ".log"), "Launcher", readFromEnd: false, liveSessionCts.Token);
        StartLogTail(Path.Combine(plan.BuildDirectory, "bin", "Release", "gwa3_bot.log"), "Bot", readFromEnd: true, liveSessionCts.Token);

        liveLaunchTask = TrackBackgroundTask(RunLiveLaunchAsync(plan, SelectedSession, liveSessionCts.Token), "live launch");
    }

    private void AttachLatestRunIfPresent()
    {
        if (SelectedSession is null || currentProfile is null || liveSessionCts is not null)
        {
            return;
        }

        try
        {
            var plan = CreateLaunchPlan(currentProfile, dryRun: false);
            var snapshot = statusSnapshotStore.TryReadLatest();
            if (snapshot is not null &&
                SnapshotMatchesCurrentProfile(snapshot) &&
                int.TryParse(snapshot.Pid, NumberStyles.Integer, CultureInfo.InvariantCulture, out var snapshotPid) &&
                snapshotPid > 0 &&
                IsLiveGuildWarsProcess(snapshotPid))
            {
                AttachToLiveProcess(plan, snapshotPid, "latest status snapshot");
                return;
            }

            if (TryFindLatestLiveDllLog(plan, out var candidate))
            {
                AttachToLiveProcess(plan, candidate.Pid, $"live DLL log {Path.GetFileName(candidate.LogPath)}");
            }
        }
        catch (JsonException ex)
        {
            AddLog("Session", "WARN", $"Could not read latest status snapshot: {ex.Message}");
        }
        catch (IOException ex)
        {
            AddLog("Session", "WARN", $"Could not attach latest run: {ex.Message}");
        }
        catch (UnauthorizedAccessException ex)
        {
            AddLog("Session", "WARN", $"Could not attach latest run: {ex.Message}");
        }
    }

    private void RefreshLiveAttachmentIfNeeded()
    {
        if (SelectedSession is null || currentProfile is null || liveSessionOwnsGuildWarsProcess)
        {
            return;
        }

        var now = DateTimeOffset.Now;
        if (now - lastLiveAttachmentProbe < TimeSpan.FromSeconds(5))
        {
            return;
        }

        lastLiveAttachmentProbe = now;

        try
        {
            var sessionPid = TryGetSelectedSessionPid(out var parsedSessionPid)
                ? parsedSessionPid
                : (int?)null;
            var attachedPid = liveGuildWarsProcessId;
            var attachedPidIsLive = attachedPid is int currentPid && IsLiveGuildWarsProcess(currentPid);

            if (attachedPidIsLive && sessionPid == attachedPid)
            {
                return;
            }

            var plan = CreateLaunchPlan(currentProfile, dryRun: false);
            if (!TryFindLatestLiveDllLog(plan, out var candidate))
            {
                if (!attachedPidIsLive && liveSessionCts is not null)
                {
                    DetachLiveMonitoringForReattach("attached Guild Wars PID is no longer live");
                }

                return;
            }

            if (attachedPidIsLive && attachedPid == candidate.Pid && sessionPid == candidate.Pid)
            {
                return;
            }

            if (attachedPid is int oldPid)
            {
                DetachLiveMonitoringForReattach(
                    oldPid == candidate.Pid
                        ? "session metadata was stale"
                        : $"newer live DLL log found for PID {candidate.Pid}");
            }

            AttachToLiveProcess(plan, candidate.Pid, $"auto-reattach {Path.GetFileName(candidate.LogPath)}");
        }
        catch (JsonException ex)
        {
            AddLog("Session", "WARN", $"Could not refresh live attachment: {ex.Message}");
        }
        catch (IOException ex)
        {
            AddLog("Session", "WARN", $"Could not refresh live attachment: {ex.Message}");
        }
        catch (UnauthorizedAccessException ex)
        {
            AddLog("Session", "WARN", $"Could not refresh live attachment: {ex.Message}");
        }
    }

    private bool TryGetSelectedSessionPid(out int pid)
    {
        pid = 0;
        return SelectedSession is not null &&
               int.TryParse(SelectedSession.Pid, NumberStyles.Integer, CultureInfo.InvariantCulture, out pid) &&
               pid > 0;
    }

    private void DetachLiveMonitoringForReattach(string reason)
    {
        liveSessionCts?.Cancel();
        liveSessionCts?.Dispose();
        liveSessionCts = null;
        liveLaunchTask = null;

        if (liveGuildWarsProcessId is int pid)
        {
            AddLog("Session", "INFO", $"Detached stale monitoring tail for PID {pid}: {reason}.");
        }

        liveGuildWarsProcessId = null;
        liveSessionOwnsGuildWarsProcess = false;
    }

    private bool SnapshotMatchesCurrentProfile(UiLiveStatusSnapshot snapshot) =>
        currentProfile is not null &&
        string.Equals(snapshot.Lane, currentProfile.Launch.LaneTag, StringComparison.OrdinalIgnoreCase) &&
        string.Equals(snapshot.Character, currentProfile.Character.CharacterName, StringComparison.OrdinalIgnoreCase);

    private void AttachToLiveProcess(LaunchPlan plan, int pid, string sourceDescription)
    {
        if (SelectedSession is null)
        {
            return;
        }

        var attachingDifferentPid = !TryGetSelectedSessionPid(out var existingPid) || existingPid != pid;
        if (attachingDifferentPid)
        {
            ResetSelectedSessionForNewLivePid(pid);
        }

        liveGuildWarsProcessId = pid;
        liveSessionOwnsGuildWarsProcess = false;
        liveRuntimeIssue = null;
        liveSessionCts = new CancellationTokenSource();
        var token = liveSessionCts.Token;
        var releaseDirectory = Path.Combine(plan.BuildDirectory, "bin", "Release");
        var botLogPath = Path.Combine(releaseDirectory, "gwa3_bot.log");
        var dllLogPath = Path.Combine(releaseDirectory, $"gwa3_log_{pid}.txt");

        SelectedSession.Pid = pid.ToString(CultureInfo.InvariantCulture);
        if (!IsBotRuntimeStatus(SelectedSession.Status))
        {
            SelectedSession.Status = "Bot Running";
        }

        if (string.IsNullOrWhiteSpace(SelectedSession.BotPhase) ||
            SelectedSession.BotPhase.Equals("Idle", StringComparison.OrdinalIgnoreCase) ||
            SelectedSession.BotPhase.Equals("Stopped", StringComparison.OrdinalIgnoreCase))
        {
            SelectedSession.BotPhase = "Attached";
        }

        SelectedSession.Health = $"attached to live Guild Wars PID {pid}";
        HydrateRunStatisticsFromBotLog(botLogPath);
        HydrateLiveSessionFromDllLog(dllLogPath);

        StartLogTail(Path.ChangeExtension(plan.LauncherScriptPath, ".log"), "Launcher", readFromEnd: true, token);
        StartLogTail(botLogPath, "Bot", readFromEnd: true, token);
        StartLogTail(dllLogPath, "DLL", readFromEnd: true, token);

        StatusBanner = $"Attached to live {SelectedSession.BotModule} session for {SelectedSession.CharacterName}.";
        AddLog("Session", "INFO", $"Attached monitoring to PID {pid} from {sourceDescription}.");
        WriteStatusSnapshot("attached-live-session", sourceDescription);
    }

    private void ResetSelectedSessionForNewLivePid(int pid)
    {
        if (SelectedSession is null)
        {
            return;
        }

        SelectedSession.ResetMonitoringStatistics();
        SelectedSession.Status = "Bot Running";
        SelectedSession.BotPhase = "Attached";
        SelectedSession.Pid = pid.ToString(CultureInfo.InvariantCulture);
        SelectedSession.Health = $"attaching to live Guild Wars PID {pid}";
        SelectedSession.LastEvent = "Waiting for live telemetry from attached process.";
        SelectedSession.DungeonLevel = "not in dungeon";
        SelectedSession.CompletionPercent = 0;
        SelectedSession.CompletionText = "0%";
        SelectedSession.ProgressDetail = "waiting for route telemetry";
    }

    private bool TryFindLatestLiveDllLog(LaunchPlan plan, out DllLogCandidate candidate)
    {
        candidate = default;
        var releaseDirectory = Path.Combine(plan.BuildDirectory, "bin", "Release");
        if (!Directory.Exists(releaseDirectory))
        {
            return false;
        }

        var candidates = new List<DllLogCandidate>();
        foreach (var logPath in Directory.EnumerateFiles(releaseDirectory, "gwa3_log_*.txt"))
        {
            var logFile = new FileInfo(logPath);
            if (!TryParseDllLogPid(logFile.Name, out var pid) ||
                !IsLiveGuildWarsProcess(pid) ||
                !DllLogMatchesBotModule(logPath, plan.BotModule))
            {
                continue;
            }

            candidates.Add(new DllLogCandidate(pid, logPath, logFile.LastWriteTimeUtc));
        }

        if (candidates.Count == 0)
        {
            return false;
        }

        candidate = candidates.OrderByDescending(item => item.LastWriteTimeUtc).First();
        return true;
    }

    private static bool TryParseDllLogPid(string fileName, out int pid)
    {
        pid = 0;
        const string prefix = "gwa3_log_";
        var name = Path.GetFileNameWithoutExtension(fileName);
        if (!name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return int.TryParse(name[prefix.Length..], NumberStyles.Integer, CultureInfo.InvariantCulture, out pid) && pid > 0;
    }

    private bool DllLogMatchesBotModule(string logPath, string? botModule)
    {
        if (string.IsNullOrWhiteSpace(botModule))
        {
            return true;
        }

        try
        {
            var lines = ReadAllLinesShared(logPath);
            return lines.Any(line => line.Contains($"Selected bot module: {botModule}", StringComparison.OrdinalIgnoreCase)) ||
                   (string.Equals(botModule, KnownBotModules.FroggyHM, StringComparison.OrdinalIgnoreCase) &&
                    lines.Any(line => line.Contains("Froggy:", StringComparison.OrdinalIgnoreCase)));
        }
        catch (IOException)
        {
            return false;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
    }

    private static bool IsLiveGuildWarsProcess(int pid)
    {
        try
        {
            using var process = Process.GetProcessById(pid);
            return !process.HasExited &&
                   process.ProcessName.Equals("Gw", StringComparison.OrdinalIgnoreCase);
        }
        catch (ArgumentException)
        {
            return false;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
        catch (System.ComponentModel.Win32Exception)
        {
            return false;
        }
    }

    private void HydrateLiveSessionFromDllLog(string dllLogPath)
    {
        if (SelectedSession is null || !File.Exists(dllLogPath))
        {
            return;
        }

        try
        {
            var lines = ReadAllLinesShared(dllLogPath);
            var startIndex = Math.Max(0, lines.Count - 700);
            string? latestState = null;
            string? latestEvent = null;
            var matchedLines = 0;

            if (SelectedSession.RuntimeStartedAt is null)
            {
                var runtimeStart = lines.FirstOrDefault(IsRuntimeStartLine) ??
                    lines.FirstOrDefault(line => TryExtractLogTimestamp(line) is not null);
                if (runtimeStart is not null)
                {
                    SelectedSession.MarkRuntimeStarted(TryExtractLogTimestamp(runtimeStart) ?? DateTimeOffset.Now);
                    if (SelectedSession.CurrentRunStartedAt is null)
                    {
                        SelectedSession.MarkCurrentRunStarted(TryExtractLogTimestamp(runtimeStart) ?? DateTimeOffset.Now);
                    }
                }
            }

            foreach (var line in lines.Skip(startIndex))
            {
                var updated =
                    UpdateRunCountersFromLog(line) |
                    UpdateDungeonProgressFromLog(line) |
                    UpdateExternalMonitoringFromLog("DLL", line);

                if (TryExtractBotState(line) is { Length: > 0 } state)
                {
                    latestState = state;
                    latestEvent = line;
                    updated = true;
                }
                else if (line.Contains("MonitoringStats", StringComparison.OrdinalIgnoreCase) ||
                         line.Contains("wp=", StringComparison.OrdinalIgnoreCase) ||
                         line.Contains("Bot thread started", StringComparison.OrdinalIgnoreCase))
                {
                    latestEvent = line;
                }

                if (updated)
                {
                    matchedLines++;
                }
            }

            if (!string.IsNullOrWhiteSpace(latestState))
            {
                SelectedSession.Status = latestState;
                SelectedSession.BotPhase = latestState;
            }
            else if (!IsBotRuntimeStatus(SelectedSession.Status))
            {
                SelectedSession.Status = "Bot Running";
                SelectedSession.BotPhase = "Attached";
            }

            if (!string.IsNullOrWhiteSpace(latestEvent))
            {
                SelectedSession.LastEvent = SummarizeStatusText(latestEvent);
            }

            if (string.IsNullOrWhiteSpace(liveRuntimeIssue))
            {
                SelectedSession.Health = $"live telemetry hydrated {DateTime.Now:HH:mm:ss}";
            }

            SelectedSession.RefreshTimers(DateTimeOffset.Now);
            AddLog("Session", "INFO", $"Hydrated live DLL telemetry from {Path.GetFileName(dllLogPath)} ({matchedLines}/{lines.Count - startIndex} matched).");
        }
        catch (IOException ex)
        {
            AddLog("Session", "WARN", $"Could not hydrate live DLL telemetry: {ex.Message}");
        }
        catch (UnauthorizedAccessException ex)
        {
            AddLog("Session", "WARN", $"Could not hydrate live DLL telemetry: {ex.Message}");
        }
    }

    private void HydrateRunStatisticsFromBotLog(string botLogPath)
    {
        if (SelectedSession is null || !File.Exists(botLogPath))
        {
            return;
        }

        try
        {
            var lines = ReadAllLinesShared(botLogPath);
            SelectedSession.PrepareLogStatisticsHydration();
            var matchedLines = 0;
            var startIndex = FindLatestBotSessionStartIndex(lines);

            foreach (var line in lines.Skip(startIndex))
            {
                if (UpdateRunCountersFromLog(line))
                {
                    matchedLines++;
                }
            }

            SelectedSession.RefreshTimers(DateTimeOffset.Now);
            AddLog("Session", "INFO", $"Hydrated GWA2-style run statistics from {Path.GetFileName(botLogPath)} latest session ({matchedLines}/{lines.Count - startIndex} matched).");
            WriteStatusSnapshot("statistics-hydrated", botLogPath);
        }
        catch (IOException ex)
        {
            AddLog("Session", "WARN", $"Could not hydrate GWA2-style run statistics: {ex.Message}");
        }
        catch (UnauthorizedAccessException ex)
        {
            AddLog("Session", "WARN", $"Could not hydrate GWA2-style run statistics: {ex.Message}");
        }
    }

    private static int FindLatestBotSessionStartIndex(IReadOnlyList<string> lines)
    {
        for (var index = lines.Count - 1; index >= 0; index--)
        {
            if (lines[index].Contains("Bot thread started", StringComparison.OrdinalIgnoreCase))
            {
                return index;
            }
        }

        return 0;
    }

    private static IReadOnlyList<string> ReadAllLinesShared(string path)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        using var reader = new StreamReader(stream);
        var lines = new List<string>();
        while (reader.ReadLine() is { } line)
        {
            lines.Add(line);
        }

        return lines;
    }

    private async Task RunLiveLaunchAsync(LaunchPlan plan, SessionViewModel session, CancellationToken cancellationToken)
    {
        try
        {
            var statusSink = new InMemorySessionStatusSink();
            statusSink.StatusPublished += (_, status) =>
                RunOnUi(() => ApplySupervisorStatus(session, status));

            var processRunner = new ProcessRunner(allowLiveProcessExecution: true);
            var supervisor = new SessionSupervisor(
                new LaunchPlanValidator(),
                new LauncherService(processRunner),
                new ProcessHealthGate(new WindowsProcessProbe()),
                new InjectorService(processRunner),
                new BridgeService(processRunner),
                statusSink);

            var result = await supervisor.StartAsync(plan, cancellationToken).ConfigureAwait(false);
            RunOnUi(() =>
            {
                liveGuildWarsProcessId = result.GuildWarsProcessId;
                session.Pid = result.GuildWarsProcessId?.ToString(CultureInfo.InvariantCulture) ?? "not launched";
                session.Health = result.HealthGate?.Message ?? result.Message;

                if (result.Succeeded)
                {
                    session.Status = "Bot Running";
                    session.BotPhase = "Initializing";
                    StatusBanner = $"Live Froggy session is running for {session.CharacterName}.";
                    AddLog("Launch", "INFO", $"Live launch succeeded; injected PID {session.Pid}.");

                    if (result.GuildWarsProcessId is int pid)
                    {
                        StartLogTail(
                            Path.Combine(plan.BuildDirectory, "bin", "Release", $"gwa3_log_{pid}.txt"),
                            "DLL",
                            readFromEnd: false,
                            liveSessionCts?.Token ?? CancellationToken.None);
                    }
                }
                else
                {
                    session.Status = result.FinalStage.ToString();
                    session.BotPhase = "Failed";
                    StatusBanner = $"Live launch failed: {result.Message}";
                    AddLog("Launch", "ERROR", result.Message);
                }

                WriteStatusSnapshot("launch-result", result.Message);
            });
        }
        catch (OperationCanceledException)
        {
            RunOnUi(() =>
            {
                session.Status = "Cancelled";
                session.Health = "operator cancelled";
                session.BotPhase = "Stopped";
                AddLog("Launch", "WARN", "Live launch was cancelled.");
                WriteStatusSnapshot("launch-cancelled", "operator cancelled");
            });
        }
        catch (Exception ex) when (ex is IOException or InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            RunOnUi(() =>
            {
                session.Status = "Failed";
                session.Health = ex.Message;
                session.BotPhase = "Failed";
                StatusBanner = $"Live launch failed: {ex.Message}";
                AddLog("Launch", "ERROR", ex.Message);
                WriteStatusSnapshot("launch-exception", ex.Message);
            });
        }
        catch (Exception ex)
        {
            RunOnUi(() =>
            {
                session.Status = "Failed";
                session.Health = ex.Message;
                session.BotPhase = "Failed";
                StatusBanner = $"Live launch failed unexpectedly: {ex.Message}";
                AddLog("Launch", "ERROR", ex.ToString());
                WriteStatusSnapshot("launch-exception", ex.Message);
            });
        }
    }

    private void StartLogTail(string path, string source, bool readFromEnd, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        var tailTask = Task.Run(async () =>
        {
            try
            {
                await foreach (var line in logTailService.TailAsync(path, source, readFromEnd, cancellationToken).ConfigureAwait(false))
                {
                    RunOnUi(() =>
                    {
                        AddLog(source, DetectLogLevel(line.Text), line.Text);
                        ApplyLiveLogStatus(source, line.Text);
                    });
                }
            }
            catch (OperationCanceledException)
            {
            }
            catch (IOException ex)
            {
                RunOnUi(() => AddLog(source, "WARN", $"Log tail stopped for {path}: {ex.Message}"));
            }
            catch (UnauthorizedAccessException ex)
            {
                RunOnUi(() => AddLog(source, "WARN", $"Log tail could not read {path}: {ex.Message}"));
            }
            catch (Exception ex)
            {
                RunOnUi(() =>
                {
                    AddLog(source, "ERROR", $"Log tail failed for {path}: {ex.Message}");
                    WriteStatusSnapshot("log-tail-failed", $"{source}: {ex.Message}");
                });
            }
        }, cancellationToken);

        TrackBackgroundTask(tailTask, $"{source} log tail");
    }

    private Task TrackBackgroundTask(Task task, string description)
    {
        lock (backgroundTasks)
        {
            backgroundTasks.Add(task);
        }

        task.ContinueWith(completedTask =>
        {
            lock (backgroundTasks)
            {
                backgroundTasks.Remove(completedTask);
            }

            if (!completedTask.IsFaulted)
            {
                return;
            }

            var message = completedTask.Exception?.GetBaseException().Message ?? "Unknown background task failure.";
            RunOnUi(() =>
            {
                AddLog("UI", "ERROR", $"{description} failed: {message}");
                if (description.Contains("launch", StringComparison.OrdinalIgnoreCase) && SelectedSession is not null)
                {
                    SelectedSession.Status = "Failed";
                    SelectedSession.BotPhase = "Failed";
                    SelectedSession.Health = message;
                    StatusBanner = $"Live launch failed: {message}";
                    WriteStatusSnapshot("background-task-failed", message);
                }
            });
        }, CancellationToken.None, TaskContinuationOptions.ExecuteSynchronously, TaskScheduler.Default);

        return task;
    }

    private void ApplySupervisorStatus(SessionViewModel session, SessionStatusEvent status)
    {
        session.Status = status.Stage switch
        {
            SessionStage.Validating => "Validating",
            SessionStage.Launching => "Launching",
            SessionStage.WaitingForHealthyClient => "Health Gate",
            SessionStage.Injecting => "Injecting",
            SessionStage.StartingBridge => "Bridge",
            SessionStage.Running => "Injected",
            SessionStage.Cancelled => "Cancelled",
            _ when status.Stage.ToString().StartsWith("Failed", StringComparison.OrdinalIgnoreCase) => "Failed",
            _ => status.Stage.ToString()
        };

        session.BotPhase = status.Stage switch
        {
            SessionStage.Launching => "GWLauncher",
            SessionStage.WaitingForHealthyClient => "Client loading",
            SessionStage.Injecting => "DLL injection",
            SessionStage.StartingBridge => "Bridge start",
            SessionStage.Running => "DLL initializing",
            _ => session.BotPhase
        };

        if (status.Details.TryGetValue("DryRun", out var dryRun) &&
            string.Equals(dryRun, "False", StringComparison.OrdinalIgnoreCase))
        {
            session.Health = status.Message;
        }

        AddLog("Supervisor", status.Severity.ToString().ToUpperInvariant(), $"{status.Stage}: {status.Message}");
        WriteStatusSnapshot(status.Stage.ToString(), status.Message);
    }

    private void ApplyLiveLogStatus(string source, string text)
    {
        if (SelectedSession is null)
        {
            return;
        }

        UpdateTotalRuntimeFromLog(text);
        var countersUpdated = UpdateRunCountersFromLog(text);
        var progressUpdated = UpdateDungeonProgressFromLog(text);
        var externalMonitorUpdated = UpdateExternalMonitoringFromLog(source, text);

        if (source.Equals("Launcher", StringComparison.OrdinalIgnoreCase) &&
            IsBotRuntimeStatus(SelectedSession.Status))
        {
            if (progressUpdated || externalMonitorUpdated)
            {
                WriteStatusSnapshot("runtime-progress", text);
            }

            return;
        }

        if (text.Contains("WATCHDOG_SCREENSHOT", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("CRASH DIALOG", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("WINDOW NOT RESPONDING", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("RENDER FROZEN", StringComparison.OrdinalIgnoreCase))
        {
            IncrementFailureCount();
            SelectedSession.Status = "Watchdog";
            SelectedSession.BotPhase = "Failure";
            SelectedSession.Health = text;
            StatusBanner = "Runtime watchdog reported a live-client failure.";
            WriteStatusSnapshot("watchdog", text);
            return;
        }

        if (IsRuntimeDiagnosticIssue(source, text))
        {
            liveRuntimeIssue = text;
            SelectedSession.Status = "Runtime Issue";
            SelectedSession.BotPhase = "Diagnostics";
            SelectedSession.Health = SummarizeStatusText(text);
            SelectedSession.LastEvent = text;
            StatusBanner = "Runtime diagnostics reported a live-client issue.";
            WriteStatusSnapshot("runtime-diagnostic", text);
            return;
        }

        if (!source.Equals("Launcher", StringComparison.OrdinalIgnoreCase) && IsBotErrorState(text))
        {
            liveRuntimeIssue = text;
            IncrementFailureCount();
            SelectedSession.Status = "Error";
            SelectedSession.BotPhase = "Error";
            SelectedSession.Health = SummarizeStatusText(text);
            SelectedSession.LastEvent = text;
            StatusBanner = "The running bot reported an error state.";
            WriteStatusSnapshot("bot-error", text);
            return;
        }

        if (text.Contains("Bot thread started", StringComparison.OrdinalIgnoreCase))
        {
            SelectedSession.Status = "Bot Running";
            SelectedSession.BotPhase = "Bot thread";
            SelectedSession.LastEvent = text;
            PreserveRuntimeIssueHealth();
            StatusBanner = "Froggy bot thread is running.";
            WriteStatusSnapshot("bot-thread-started", text);
            return;
        }

        var state = TryExtractBotState(text);
        if (!string.IsNullOrWhiteSpace(state))
        {
            SelectedSession.Status = state;
            SelectedSession.BotPhase = state;
            SelectedSession.LastEvent = text;
            PreserveRuntimeIssueHealth();
            StatusBanner = $"Froggy state: {state}.";
            WriteStatusSnapshot("bot-state", text);
            return;
        }

        if (text.Contains("gwa3.dll initialization complete - bot started", StringComparison.OrdinalIgnoreCase))
        {
            SelectedSession.Status = "Bot Running";
            SelectedSession.BotPhase = "DLL initialized";
            SelectedSession.LastEvent = text;
            PreserveRuntimeIssueHealth();
            StatusBanner = liveRuntimeIssue is null
                ? "GWA3 DLL initialized and bot mode started."
                : "GWA3 DLL initialized with runtime diagnostics pending review.";
            WriteStatusSnapshot(liveRuntimeIssue is null ? "dll-ready" : "dll-ready-with-diagnostics", text);
            return;
        }

        if (text.Contains("Bootstrap:", StringComparison.OrdinalIgnoreCase) ||
            source.Equals("Launcher", StringComparison.OrdinalIgnoreCase))
        {
            if (liveRuntimeIssue is null || source.Equals("Launcher", StringComparison.OrdinalIgnoreCase))
            {
                SelectedSession.Health = SummarizeStatusText(text);
            }
            SelectedSession.LastEvent = text;
            WriteStatusSnapshot("live-log", text);
        }

        if (countersUpdated || progressUpdated || externalMonitorUpdated)
        {
            SelectedSession.LastEvent = SummarizeStatusText(text);
            WriteStatusSnapshot(progressUpdated ? "runtime-progress" : "runtime-counter", text);
        }
    }

    private bool UpdateExternalMonitoringFromLog(string source, string text)
    {
        if (SelectedSession is null || string.IsNullOrWhiteSpace(text))
        {
            return false;
        }

        var updated = false;
        var parsedRuntimeLine = false;

        if (RuntimeLogParser.TryParse(source, text) is { } runtimeSnapshot)
        {
            parsedRuntimeLine = true;
            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.ActionQueue))
            {
                SelectedSession.BotshubCommandQueue = runtimeSnapshot.ActionQueue;
                SelectedSession.RuntimeActionQueue = runtimeSnapshot.ActionQueue;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Map))
            {
                SelectedSession.RuntimeMap = SelectedSession.DungeonLevel != "not in dungeon"
                    ? $"{SelectedSession.DungeonLevel} ({runtimeSnapshot.Map})"
                    : runtimeSnapshot.Map;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Health))
            {
                SelectedSession.RuntimeHealth = runtimeSnapshot.Health;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Position))
            {
                SelectedSession.RuntimePosition = runtimeSnapshot.Position;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Target))
            {
                SelectedSession.RuntimeTarget = runtimeSnapshot.Target;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Pathing))
            {
                SelectedSession.RuntimePathing = SelectedSession.ProgressDetail != "waiting for route telemetry"
                    ? SelectedSession.ProgressDetail
                    : runtimeSnapshot.Pathing;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Casting))
            {
                SelectedSession.RuntimeCasting = runtimeSnapshot.Casting;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Skillbar))
            {
                SelectedSession.RuntimeSkillbar = runtimeSnapshot.Skillbar;
            }

            if (!string.IsNullOrWhiteSpace(runtimeSnapshot.Overwatch))
            {
                SelectedSession.RuntimeOverwatch = runtimeSnapshot.Overwatch;
            }

            SelectedSession.RuntimeLastUpdate = DateTime.Now.ToString("HH:mm:ss", CultureInfo.InvariantCulture);
            updated = true;
        }

        if (TryExtractBotStateTransition(text) is { } transition)
        {
            SelectedSession.RuntimePreviousStep = EmptyToFallback(transition.Previous, "n/a");
            SelectedSession.RuntimeCurrentStep = transition.Current;
            SelectedSession.RuntimeNextStep = InferNextRuntimeStep(transition.Current);
            SelectedSession.RuntimeStateMachine = "started";
            updated = true;
        }
        else if (TryExtractRunNumber(text) is { } runNumber &&
                 text.Contains("State: TownSetup", StringComparison.OrdinalIgnoreCase))
        {
            SelectedSession.RuntimePreviousStep = SelectedSession.RuntimeCurrentStep;
            SelectedSession.RuntimeCurrentStep = $"TownSetup run {runNumber}";
            SelectedSession.RuntimeNextStep = "Traveling";
            SelectedSession.RuntimeStateMachine = "started";
            updated = true;
        }
        else if (text.Contains("Run #", StringComparison.OrdinalIgnoreCase) &&
                 text.Contains("complete in", StringComparison.OrdinalIgnoreCase))
        {
            SelectedSession.RuntimePreviousStep = SelectedSession.RuntimeCurrentStep;
            SelectedSession.RuntimeCurrentStep = "Run complete";
            SelectedSession.RuntimeNextStep = "TownSetup";
            SelectedSession.RuntimeStateMachine = "finished";
            updated = true;
        }

        if (text.Contains("SkillBar Loaded", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("Mapping your skill bar - completed", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("CacheSkillBar", StringComparison.OrdinalIgnoreCase))
        {
            SelectedSession.RuntimeSkillbar = "loaded";
            updated = true;
        }

        if (!parsedRuntimeLine &&
            (text.Contains("UseSkill", StringComparison.OrdinalIgnoreCase) ||
             text.Contains("casting", StringComparison.OrdinalIgnoreCase)))
        {
            SelectedSession.RuntimeCasting = SummarizeStatusText(text);
            updated = true;
        }

        if (!parsedRuntimeLine &&
            (text.Contains("AggroMove", StringComparison.OrdinalIgnoreCase) ||
             text.Contains("Moving to waypoint", StringComparison.OrdinalIgnoreCase) ||
             text.Contains("Nearest waypoint", StringComparison.OrdinalIgnoreCase) ||
             text.Contains("FollowPath", StringComparison.OrdinalIgnoreCase) ||
             text.Contains("wp=", StringComparison.OrdinalIgnoreCase)))
        {
            SelectedSession.RuntimePathing = SelectedSession.ProgressDetail != "waiting for route telemetry"
                ? SelectedSession.ProgressDetail
                : SummarizeStatusText(text);
            updated = true;
        }

        return updated;
    }

    private static string InferNextRuntimeStep(string current) =>
        current.ToLowerInvariant() switch
        {
            "townsetup" => "Traveling",
            "traveling" => "InDungeon",
            "indungeon" => "Route waypoint",
            "awaitingreturn" => "Sparkfly return",
            "maintenance" => "TownSetup",
            "bot thread" => "Initializing",
            "dll initialized" => "Bot state",
            _ => "awaiting next transition"
        };

    private void UpdateTotalRuntimeFromLog(string text)
    {
        if (SelectedSession is null)
        {
            return;
        }

        if (SelectedSession.RuntimeStartedAt is null && IsRuntimeStartLine(text))
        {
            SelectedSession.MarkRuntimeStarted(TryExtractLogTimestamp(text) ?? DateTimeOffset.Now);
        }

        SelectedSession.RefreshTotalRuntime(DateTimeOffset.Now);
    }

    private bool UpdateDungeonProgressFromLog(string text)
    {
        if (SelectedSession is null)
        {
            return false;
        }

        var snapshot = DungeonProgressParser.TryParseFroggyProgress(text);
        if (snapshot is null)
        {
            return false;
        }

        var previousLevel = SelectedSession.DungeonLevel;
        var previousPercent = SelectedSession.CompletionPercent;
        var previousDetail = SelectedSession.ProgressDetail;
        var percent = snapshot.CompletionPercent;
        var completionText = snapshot.CompletionText;
        var progressDetail = snapshot.ProgressDetail;

        if (!snapshot.IsPrecise &&
            previousPercent > snapshot.CompletionPercent &&
            (string.Equals(previousLevel, snapshot.DungeonLevel, StringComparison.OrdinalIgnoreCase) ||
             IsPostRewardOrCompleteProgress(previousPercent, previousDetail)))
        {
            percent = previousPercent;
            completionText = previousPercent.ToString(CultureInfo.InvariantCulture) + "%";
            progressDetail = previousDetail;
        }

        SelectedSession.DungeonLevel = snapshot.DungeonLevel;
        SelectedSession.CompletionPercent = percent;
        SelectedSession.CompletionText = completionText;
        SelectedSession.ProgressDetail = progressDetail;
        SelectedSession.RuntimeMap = snapshot.DungeonLevel;
        SelectedSession.RuntimePathing = progressDetail;
        SelectedSession.RuntimeLastUpdate = DateTime.Now.ToString("HH:mm:ss", CultureInfo.InvariantCulture);

        var inferredPhase = InferRuntimePhaseFromDungeonProgress(snapshot.DungeonLevel, progressDetail);
        if (liveRuntimeIssue is null &&
            !string.IsNullOrWhiteSpace(inferredPhase) &&
            IsBotRuntimeStatus(SelectedSession.Status))
        {
            SelectedSession.Status = inferredPhase;
            SelectedSession.BotPhase = inferredPhase;
            if (inferredPhase == "AwaitingReturn")
            {
                SelectedSession.RuntimeCurrentStep = inferredPhase;
                SelectedSession.RuntimeNextStep = "Sparkfly return";
                SelectedSession.RuntimeStateMachine = "started";
            }
        }

        return !string.Equals(previousLevel, SelectedSession.DungeonLevel, StringComparison.Ordinal) ||
               previousPercent != SelectedSession.CompletionPercent ||
               !string.Equals(previousDetail, SelectedSession.ProgressDetail, StringComparison.Ordinal);
    }

    private static string InferRuntimePhaseFromDungeonProgress(string dungeonLevel, string progressDetail = "")
    {
        if (progressDetail.Contains("Waiting for return", StringComparison.OrdinalIgnoreCase))
        {
            return "AwaitingReturn";
        }

        if (dungeonLevel.Contains("Bogroot", StringComparison.OrdinalIgnoreCase))
        {
            return "InDungeon";
        }

        if (dungeonLevel.Contains("Travel", StringComparison.OrdinalIgnoreCase))
        {
            return "Traveling";
        }

        if (dungeonLevel.Contains("Sparkfly", StringComparison.OrdinalIgnoreCase))
        {
            return "InDungeon";
        }

        if (dungeonLevel.Contains("Town", StringComparison.OrdinalIgnoreCase))
        {
            return "InTown";
        }

        if (dungeonLevel.Contains("Complete", StringComparison.OrdinalIgnoreCase))
        {
            return "Complete";
        }

        return "";
    }

    private static bool IsPostRewardOrCompleteProgress(int completionPercent, string progressDetail)
    {
        if (completionPercent < 99)
        {
            return false;
        }

        return progressDetail.Contains("Chest", StringComparison.OrdinalIgnoreCase) ||
               progressDetail.Contains("Quest reward", StringComparison.OrdinalIgnoreCase) ||
               progressDetail.Contains("Waiting for return", StringComparison.OrdinalIgnoreCase) ||
               progressDetail.Contains("Run complete", StringComparison.OrdinalIgnoreCase);
    }

    private bool UpdateRunCountersFromLog(string text)
    {
        if (SelectedSession is null)
        {
            return false;
        }

        var updated = false;
        var timestamp = TryExtractLogTimestamp(text) ?? DateTimeOffset.Now;
        var runStart = FroggyMonitoringParser.TryParseRunStart(text);
        if (runStart is not null)
        {
            SelectedSession.RecordRunStarted(runStart.RunNumber, timestamp);
            updated = true;
        }

        var runCompletion = FroggyMonitoringParser.TryParseRunCompletion(text);
        if (runCompletion is not null)
        {
            SelectedSession.RecordRunCompletion(
                runCompletion.RunNumber,
                runCompletion.DurationMilliseconds,
                runCompletion.BestMilliseconds);
            updated = true;
        }

        var openedChests = FroggyMonitoringParser.TryParseOpenedChestIncrement(text);
        if (openedChests is int chestIncrement && chestIncrement > 0)
        {
            SelectedSession.IncrementChestsOpened(chestIncrement);
            updated = true;
        }

        var monitoringStats = FroggyMonitoringParser.TryParseMonitoringStats(text);
        if (monitoringStats is not null)
        {
            SelectedSession.ApplyMonitoringStats(monitoringStats);
            updated = true;
        }

        if (FroggyMonitoringParser.IsWipeLine(text))
        {
            SelectedSession.IncrementWipes();
            updated = true;
        }

        if (text.Contains("[WATCHDOG] runtime alive", StringComparison.OrdinalIgnoreCase))
        {
            SelectedSession.Heartbeat = $"alive {DateTime.Now:HH:mm:ss}";
            if (string.IsNullOrWhiteSpace(liveRuntimeIssue))
            {
                SelectedSession.Health = SummarizeStatusText(text);
            }
            updated = true;
        }

        return updated;
    }

    private void IncrementFailureCount()
    {
        if (SelectedSession is null)
        {
            return;
        }

        SelectedSession.IncrementFailureCount();
    }

    private static string? TryExtractRunNumber(string text)
    {
        const string marker = "(run #";
        var index = text.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        index += marker.Length;
        var end = index;
        while (end < text.Length && char.IsDigit(text[end]))
        {
            end++;
        }

        return end > index ? text[index..end] : null;
    }

    private static string? TryExtractBotState(string text)
    {
        return TryExtractBotStateTransition(text)?.Current;
    }

    private static (string Previous, string Current)? TryExtractBotStateTransition(string text)
    {
        const string transition = "State transition:";
        const string stateSet = "State set:";
        var marker = text.Contains(transition, StringComparison.OrdinalIgnoreCase) ? transition :
            text.Contains(stateSet, StringComparison.OrdinalIgnoreCase) ? stateSet : null;

        if (marker is null)
        {
            return null;
        }

        var index = text.IndexOf("->", StringComparison.OrdinalIgnoreCase);
        if (index >= 0 && index + 2 < text.Length)
        {
            var previousStart = text.IndexOf(marker, StringComparison.OrdinalIgnoreCase) + marker.Length;
            var previous = text[previousStart..index].Trim();
            var current = text[(index + 2)..].Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault();
            return string.IsNullOrWhiteSpace(current) ? null : (previous, current);
        }

        var valueStart = text.IndexOf(marker, StringComparison.OrdinalIgnoreCase) + marker.Length;
        var value = text[valueStart..].Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault();
        return string.IsNullOrWhiteSpace(value) ? null : ("n/a", value);
    }

    private void PreserveRuntimeIssueHealth()
    {
        if (SelectedSession is not null && !string.IsNullOrWhiteSpace(liveRuntimeIssue))
        {
            SelectedSession.Health = SummarizeStatusText(liveRuntimeIssue);
        }
    }

    private static bool IsRuntimeDiagnosticIssue(string source, string text) =>
        source.Equals("DLL", StringComparison.OrdinalIgnoreCase) &&
        !IsStartupDiagnosticProbeLine(text) &&
        (text.Contains("[ERROR]", StringComparison.OrdinalIgnoreCase) ||
         text.Contains("[FATAL]", StringComparison.OrdinalIgnoreCase));

    private static bool IsBotErrorState(string text) =>
        text.Contains("ERROR state", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("State: ERROR", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("Run failed", StringComparison.OrdinalIgnoreCase);

    private static bool IsStartupDiagnosticProbeLine(string text) =>
        text.Contains("HookMarker: SelfTest", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("CrashDiag: active hook=", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("CrashDiag: hook tick snapshot", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("CrashDiag:   ", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("CrashDiag: most recently entered hook", StringComparison.OrdinalIgnoreCase);

    private static bool IsBotRuntimeStatus(string status) =>
        status.Equals("Bot Running", StringComparison.OrdinalIgnoreCase) ||
        status.Equals("InTown", StringComparison.OrdinalIgnoreCase) ||
        status.Equals("Traveling", StringComparison.OrdinalIgnoreCase) ||
        status.Equals("InDungeon", StringComparison.OrdinalIgnoreCase) ||
        status.Equals("Runtime Issue", StringComparison.OrdinalIgnoreCase);

    private static bool IsRuntimeStartLine(string text) =>
        text.Contains("Bot thread started", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("State: TownSetup (run #1)", StringComparison.OrdinalIgnoreCase);

    private static DateTimeOffset? TryExtractLogTimestamp(string text)
    {
        if (string.IsNullOrWhiteSpace(text) || text[0] != '[')
        {
            return null;
        }

        var close = text.IndexOf(']');
        if (close <= 1)
        {
            return null;
        }

        var timestamp = text[1..close];
        if (DateTime.TryParseExact(
                timestamp,
                "yyyy-MM-dd HH:mm:ss",
                CultureInfo.InvariantCulture,
                DateTimeStyles.AssumeLocal,
                out var fullDateTime))
        {
            return new DateTimeOffset(fullDateTime);
        }

        if (TimeSpan.TryParseExact(timestamp, "c", CultureInfo.InvariantCulture, out var timeOfDay))
        {
            var now = DateTimeOffset.Now;
            var candidate = new DateTimeOffset(now.Date + timeOfDay, now.Offset);
            if (candidate > now.AddMinutes(5))
            {
                candidate = candidate.AddDays(-1);
            }

            return candidate;
        }

        return null;
    }

    private static string SummarizeStatusText(string text)
    {
        const int maxLength = 220;
        var normalized = text.Replace('\r', ' ').Replace('\n', ' ').Trim();
        return normalized.Length <= maxLength ? normalized : normalized[..(maxLength - 3)] + "...";
    }

    private static string DetectLogLevel(string text)
    {
        if (text.Contains("WATCHDOG_SCREENSHOT", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("CRASH DIALOG", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("WINDOW NOT RESPONDING", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("RENDER FROZEN", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("ERROR", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("[FAIL]", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("[FATAL]", StringComparison.OrdinalIgnoreCase))
        {
            return "ERROR";
        }

        if (text.Contains("WARN", StringComparison.OrdinalIgnoreCase))
        {
            return "WARN";
        }

        if (text.Contains("State transition", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("State set", StringComparison.OrdinalIgnoreCase))
        {
            return "ACTION";
        }

        return "INFO";
    }

    private void StopSelectedSession()
    {
        if (SelectedSession is null)
        {
            return;
        }

        liveSessionCts?.Cancel();
        liveSessionCts?.Dispose();
        liveSessionCts = null;
        liveLaunchTask = null;
        liveRuntimeIssue = null;

        if (liveSessionOwnsGuildWarsProcess && liveGuildWarsProcessId is int pid)
        {
            try
            {
                var process = Process.GetProcessById(pid);
                if (!process.HasExited)
                {
                    process.Kill(entireProcessTree: false);
                    AddLog("Session", "ACTION", $"Stopped exact UI-launched Guild Wars PID {pid}.");
                }
            }
            catch (ArgumentException)
            {
            }
            catch (InvalidOperationException ex)
            {
                AddLog("Session", "WARN", $"Could not stop PID {pid}: {ex.Message}");
            }
            catch (System.ComponentModel.Win32Exception ex)
            {
                AddLog("Session", "WARN", $"Could not stop PID {pid}: {ex.Message}");
            }
        }
        else if (liveGuildWarsProcessId is int attachedPid)
        {
            AddLog("Session", "ACTION", $"Detached UI monitoring from existing Guild Wars PID {attachedPid}.");
        }

        liveGuildWarsProcessId = null;
        liveSessionOwnsGuildWarsProcess = false;
        SelectedSession.Status = "Idle";
        SelectedSession.Pid = "not launched";
        SelectedSession.Health = "not running";
        SelectedSession.BotPhase = "Idle";
        SelectedSession.DungeonLevel = "not in dungeon";
        SelectedSession.CompletionPercent = 0;
        SelectedSession.CompletionText = "0%";
        SelectedSession.ProgressDetail = "waiting for route telemetry";
        SelectedSession.ResetRuntime();
        StatusBanner = $"Session {SelectedSession.CharacterName} returned to idle.";
        AddLog("Session", "ACTION", $"Stopped UI session state for {SelectedSession.CharacterName}.");
        WriteStatusSnapshot("stopped", "operator stopped session");
    }

    private bool CanSendChat()
    {
        return !string.IsNullOrWhiteSpace(ChatDraft);
    }

    private void SendChat()
    {
        var text = ChatDraft.Trim();
        if (text.Length == 0)
        {
            return;
        }

        ChatMessages.Add(new ChatMessageViewModel("Operator", text, true));
        ChatMessages.Add(new ChatMessageViewModel("Assistant", $"Queued steering note for {SelectedModel} in {SelectedAutonomyLevel} mode.", false));
        ChatDraft = "";
        AddLog("LLM", "ACTION", $"Queued chat steering message for {SelectedModel}.");
    }

    private void OpenLogFolder()
    {
        var directory = Path.Combine(repositoryRoot, "ui", "runs", "latest");
        Directory.CreateDirectory(directory);
        Process.Start(new ProcessStartInfo
        {
            FileName = directory,
            UseShellExecute = true
        });
        AddLog("UI", "ACTION", $"Opened log folder: {directory}");
    }

    private LaunchPlan CreateLaunchPlan(Gwa3Profile profile, bool dryRun)
    {
        ApplyKnownCharacterLaunchDefaults(profile, profile.Character.CharacterName);
        var launcherScriptPath = ResolvePath(profile.Launch.LauncherScriptPath);
        var buildDirectory = ResolvePath(profile.Launch.BuildDirectory);

        return new LaunchPlan
        {
            ProfileName = profile.ProfileName,
            DryRun = dryRun,
            Mode = ToSupervisorLaunchMode(profile.Launch.LaunchMode),
            AccountIndex = profile.Character.AccountIndex ?? 0,
            CharacterName = profile.Character.CharacterName,
            LaneTag = profile.Launch.LaneTag,
            AutoItExecutablePath = ResolveAutoItExecutablePath(),
            LauncherScriptPath = launcherScriptPath,
            LauncherWorkingDirectory = Path.GetDirectoryName(launcherScriptPath),
            BuildDirectory = buildDirectory,
            InjectorPath = ResolvePath(profile.Launch.InjectorPath),
            DllName = profile.Launch.DllName,
            BotModule = profile.Bot.ModuleId,
            ResolvedProfilePath = null,
            Bridge = new BridgePipeMetadata
            {
                PipeName = profile.Launch.PipeName,
                LaneTag = profile.Launch.LaneTag,
                ExpectedDllName = profile.Launch.DllName,
                BridgeWorkingDirectory = repositoryRoot,
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

    private string ResolvePath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return "";
        }

        return Path.IsPathRooted(path)
            ? path
            : Path.GetFullPath(Path.Combine(repositoryRoot, path));
    }

    private static bool ApplyKnownCharacterLaunchDefaults(Gwa3Profile profile, string? characterName)
    {
        var defaults = FindKnownCharacterLaunchDefaults(characterName);
        if (defaults is null)
        {
            return false;
        }

        profile.Character.AccountIndex = defaults.AccountIndex;
        profile.Character.CharacterName = defaults.CharacterName;
        profile.Character.AccountLabel = defaults.AccountLabel;
        profile.Launch.LaneTag = defaults.LaneTag;
        profile.Launch.BuildDirectory = defaults.BuildDirectory;
        profile.Launch.DllName = defaults.DllName;
        profile.Launch.PipeName = defaults.PipeName;
        profile.Launch.LauncherScriptPath = defaults.LauncherScriptPath;
        profile.Launch.InjectorPath = Path.Combine(defaults.BuildDirectory, "bin", "Release", "injector.exe");
        return true;
    }

    private static CharacterLaunchDefaults? FindKnownCharacterLaunchDefaults(string? characterName)
    {
        var key = NormalizeCharacterKey(characterName);
        return string.IsNullOrWhiteSpace(key)
            ? null
            : KnownCharacterLaunchDefaults.FirstOrDefault(candidate =>
                string.Equals(NormalizeCharacterKey(candidate.CharacterName), key, StringComparison.OrdinalIgnoreCase));
    }

    private static string NormalizeCharacterKey(string? characterName) =>
        string.IsNullOrWhiteSpace(characterName)
            ? ""
            : new string(characterName.Where(character => !char.IsWhiteSpace(character)).ToArray()).ToLowerInvariant();

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
        foreach (var start in new[] { AppContext.BaseDirectory, Environment.CurrentDirectory })
        {
            var directory = new DirectoryInfo(start);
            while (directory is not null)
            {
                if (Directory.Exists(Path.Combine(directory.FullName, "ui", "profiles", "defaults")))
                {
                    return directory.FullName;
                }

                directory = directory.Parent;
            }
        }

        return Environment.CurrentDirectory;
    }

    private static OptionItem FindOrAdd(ObservableCollection<OptionItem> options, string name, string detail, string? value = null, string? path = null)
    {
        var existing = options.FirstOrDefault(option =>
            string.Equals(option.Name, name, StringComparison.OrdinalIgnoreCase)
            || string.Equals(option.Value, value, StringComparison.OrdinalIgnoreCase));

        if (existing is not null)
        {
            return existing;
        }

        var created = new OptionItem(name, detail, value, path);
        options.Add(created);
        return created;
    }

    private static string EnsureOption(ObservableCollection<string> options, string value)
    {
        if (!options.Contains(value, StringComparer.OrdinalIgnoreCase))
        {
            options.Add(value);
        }

        return value;
    }

    private static string FormatLaunchMode(CoreLaunchMode mode) =>
        mode == CoreLaunchMode.Llm ? "LLM" : mode.ToString();

    private static string FormatLlmMode(CoreLlmMode mode) =>
        mode switch
        {
            CoreLlmMode.ChatOnly => "Co-Pilot",
            _ => mode.ToString()
        };

    private static CoreLaunchMode ParseLaunchMode(string value) =>
        value switch
        {
            "LLM" => CoreLaunchMode.Llm,
            "Advisory" => CoreLaunchMode.Advisory,
            _ => CoreLaunchMode.Bot
        };

    private static CoreLlmMode ParseLlmMode(string value) =>
        value switch
        {
            "Co-Pilot" => CoreLlmMode.ChatOnly,
            "Advisory" => CoreLlmMode.Advisory,
            "Autonomous" => CoreLlmMode.Autonomous,
            _ => CoreLlmMode.Off
        };

    private static LlmAutonomyLevel ParseAutonomyLevel(string value) =>
        value switch
        {
            "Observe Only" => LlmAutonomyLevel.ObserveOnly,
            "Ask Before Action" => LlmAutonomyLevel.ConfirmBeforeAction,
            "Bounded Autonomy" => LlmAutonomyLevel.LimitedAutonomy,
            "Full Autonomy" => LlmAutonomyLevel.FullAutonomy,
            _ => LlmAutonomyLevel.Advisory
        };

    private static string FormatAutonomy(LlmAutonomyLevel level) =>
        level switch
        {
            LlmAutonomyLevel.ObserveOnly => "Observe Only",
            LlmAutonomyLevel.ConfirmBeforeAction => "Ask Before Action",
            LlmAutonomyLevel.LimitedAutonomy => "Bounded Autonomy",
            LlmAutonomyLevel.FullAutonomy => "Full Autonomy",
            _ => "Advisory"
        };

    private static SupervisorLaunchMode ToSupervisorLaunchMode(CoreLaunchMode mode) =>
        mode switch
        {
            CoreLaunchMode.Llm => SupervisorLaunchMode.Llm,
            CoreLaunchMode.Advisory => SupervisorLaunchMode.Advisory,
            _ => SupervisorLaunchMode.Bot
        };

    private static string DescribeItemRule(ItemRule rule)
    {
        var parts = new[] { rule.ItemType, rule.Rarity.ToString(), rule.Material, rule.ModifierPattern }
            .Where(part => !string.IsNullOrWhiteSpace(part) && !string.Equals(part, "Any", StringComparison.OrdinalIgnoreCase));
        return string.Join(" / ", parts);
    }

    private static void RunOnUi(Action action)
    {
        var dispatcher = Application.Current?.Dispatcher;
        if (dispatcher is not null && !dispatcher.CheckAccess())
        {
            dispatcher.Invoke(action);
            return;
        }

        action();
    }

    private UiLiveStatusSnapshot CreateLiveStatusSnapshot(string eventName, string detail)
    {
        var session = SelectedSession ?? throw new InvalidOperationException("Cannot write a status snapshot without a selected session.");

        return new UiLiveStatusSnapshot
        {
            Timestamp = DateTimeOffset.Now,
            EventName = eventName,
            Profile = ProfileName,
            Character = session.CharacterName,
            Lane = session.LaneTag,
            Bot = session.BotModule,
            Status = session.Status,
            BotPhase = session.BotPhase,
            Pid = session.Pid,
            Health = session.Health,
            LastEvent = session.LastEvent,
            RunCount = session.RunCount,
            SuccessCount = session.SuccessCount,
            FailureCount = session.FailureCount,
            FailureRatio = session.FailureRatio,
            Gold = session.Gold,
            Heartbeat = session.Heartbeat,
            RuntimeStartedAt = session.RuntimeStartedAt,
            CurrentRunStartedAt = session.CurrentRunStartedAt,
            TotalRuntime = session.TotalRuntime,
            CurrentRunTime = session.CurrentRunTime,
            BestRunTime = session.BestRunTime,
            AverageRunTime = session.AverageRunTime,
            DungeonLevel = session.DungeonLevel,
            CompletionPercent = session.CompletionPercent,
            CompletionText = session.CompletionText,
            ProgressDetail = session.ProgressDetail,
            DeldrimorPoints = session.DeldrimorPoints,
            AsuraPoints = session.AsuraPoints,
            NornPoints = session.NornPoints,
            VanguardPoints = session.VanguardPoints,
            Lockpicks = session.Lockpicks,
            Wipes = session.Wipes,
            RareSkins = session.RareSkins,
            GoldItems = session.GoldItems,
            DroppedLockpicks = session.DroppedLockpicks,
            ChestsOpened = session.ChestsOpened,
            BlackDyes = session.BlackDyes,
            Tomes = session.Tomes,
            BotshubIpcStatus = session.BotshubIpcStatus,
            BotshubScript = session.BotshubScript,
            BotshubPid = session.BotshubPid,
            BotshubState = session.BotshubState,
            BotshubMapId = session.BotshubMapId,
            BotshubRunning = session.BotshubRunning,
            BotshubUptime = session.BotshubUptime,
            BotshubGold = session.BotshubGold,
            BotshubSettings = session.BotshubSettings,
            BotshubRunStats = session.BotshubRunStats,
            BotshubRuns = session.BotshubRuns,
            BotshubSuccesses = session.BotshubSuccesses,
            BotshubFailures = session.BotshubFailures,
            BotshubSuccessRatio = session.BotshubSuccessRatio,
            BotshubCurrentRunTime = session.BotshubCurrentRunTime,
            BotshubTotalTime = session.BotshubTotalTime,
            BotshubBestRunTime = session.BotshubBestRunTime,
            BotshubAverageRunTime = session.BotshubAverageRunTime,
            BotshubTimePerRun = session.BotshubTimePerRun,
            BotshubExperience = session.BotshubExperience,
            BotshubChests = session.BotshubChests,
            BotshubGoldItems = session.BotshubGoldItems,
            BotshubTitleSummary = session.BotshubTitleSummary,
            BotshubLootSummary = session.BotshubLootSummary,
            BotshubMaterialSummary = session.BotshubMaterialSummary,
            BotshubInventorySummary = session.BotshubInventorySummary,
            BotshubMaintenanceSummary = session.BotshubMaintenanceSummary,
            BotshubStatusSource = session.BotshubStatusSource,
            BotshubCommandQueue = session.BotshubCommandQueue,
            BotshubLastLog = session.BotshubLastLog,
            RuntimePreviousStep = session.RuntimePreviousStep,
            RuntimeCurrentStep = session.RuntimeCurrentStep,
            RuntimeNextStep = session.RuntimeNextStep,
            RuntimeStateMachine = session.RuntimeStateMachine,
            RuntimeMap = session.RuntimeMap,
            RuntimeHealth = session.RuntimeHealth,
            RuntimePosition = session.RuntimePosition,
            RuntimeTarget = session.RuntimeTarget,
            RuntimePathing = session.RuntimePathing,
            RuntimeCasting = session.RuntimeCasting,
            RuntimeSkillbar = session.RuntimeSkillbar,
            RuntimeActionQueue = session.RuntimeActionQueue,
            RuntimeOverwatch = session.RuntimeOverwatch,
            RuntimeLastUpdate = session.RuntimeLastUpdate,
            Detail = detail
        };
    }

    private void ApplyLiveStatusSnapshot(UiLiveStatusSnapshot snapshot, int pid)
    {
        if (SelectedSession is null)
        {
            return;
        }

        static string ValueOrCurrent(string value, string current) =>
            string.IsNullOrWhiteSpace(value) ? current : value;

        var session = SelectedSession;
        var awaitingReturn =
            snapshot.ProgressDetail.Contains("Waiting for return", StringComparison.OrdinalIgnoreCase) ||
            snapshot.BotshubLastLog.Contains("waiting-return", StringComparison.OrdinalIgnoreCase) ||
            snapshot.Detail.Contains("waiting-return", StringComparison.OrdinalIgnoreCase);
        var effectiveStatus = awaitingReturn ? "AwaitingReturn" : snapshot.Status;
        var effectiveBotPhase = awaitingReturn ? "AwaitingReturn" : snapshot.BotPhase;
        var effectiveBotshubState = awaitingReturn ? "AwaitingReturn" : snapshot.BotshubState;
        var effectiveBotshubRunning = awaitingReturn ? "waiting" : snapshot.BotshubRunning;
        var effectiveRuntimeCurrentStep = awaitingReturn ? "AwaitingReturn" : snapshot.RuntimeCurrentStep;

        session.Status = ValueOrCurrent(effectiveStatus, session.Status);
        session.BotPhase = ValueOrCurrent(effectiveBotPhase, session.BotPhase);
        session.Pid = pid.ToString(CultureInfo.InvariantCulture);
        session.Health = ValueOrCurrent(snapshot.Health, session.Health);
        session.LastEvent = ValueOrCurrent(snapshot.LastEvent, session.LastEvent);
        session.RunCount = ValueOrCurrent(snapshot.RunCount, session.RunCount);
        session.SuccessCount = ValueOrCurrent(snapshot.SuccessCount, session.SuccessCount);
        session.FailureCount = ValueOrCurrent(snapshot.FailureCount, session.FailureCount);
        session.FailureRatio = ValueOrCurrent(snapshot.FailureRatio, session.FailureRatio);
        session.Gold = ValueOrCurrent(snapshot.Gold, session.Gold);
        session.Heartbeat = ValueOrCurrent(snapshot.Heartbeat, session.Heartbeat);
        session.TotalRuntime = ValueOrCurrent(snapshot.TotalRuntime, session.TotalRuntime);
        session.CurrentRunTime = ValueOrCurrent(snapshot.CurrentRunTime, session.CurrentRunTime);
        session.BestRunTime = ValueOrCurrent(snapshot.BestRunTime, session.BestRunTime);
        session.AverageRunTime = ValueOrCurrent(snapshot.AverageRunTime, session.AverageRunTime);
        if (snapshot.RuntimeStartedAt is DateTimeOffset runtimeStartedAt)
        {
            session.MarkRuntimeStarted(runtimeStartedAt);
        }

        if (snapshot.CurrentRunStartedAt is DateTimeOffset currentRunStartedAt)
        {
            session.MarkCurrentRunStarted(currentRunStartedAt);
        }

        var snapshotIndicatesRunComplete =
            snapshot.BotshubLastLog.Contains("run-complete", StringComparison.OrdinalIgnoreCase) ||
            snapshot.ProgressDetail.Contains("Run complete", StringComparison.OrdinalIgnoreCase) ||
            snapshot.Detail.Contains("run-complete", StringComparison.OrdinalIgnoreCase);
        var incomingDungeonLevel = snapshotIndicatesRunComplete ? "Complete" : snapshot.DungeonLevel;
        var incomingCompletionPercent = snapshotIndicatesRunComplete ? 100 : snapshot.CompletionPercent;
        var incomingCompletionText = snapshotIndicatesRunComplete ? "100%" : snapshot.CompletionText;
        var incomingProgressDetail = snapshotIndicatesRunComplete ? "Run complete" : snapshot.ProgressDetail;
        var keepCurrentProgress =
            !snapshotIndicatesRunComplete &&
            incomingCompletionPercent > 0 &&
            session.CompletionPercent > incomingCompletionPercent &&
            IsPostRewardOrCompleteProgress(session.CompletionPercent, session.ProgressDetail);

        if (!keepCurrentProgress)
        {
            session.DungeonLevel = ValueOrCurrent(incomingDungeonLevel, session.DungeonLevel);
            if (incomingCompletionPercent > 0 || !string.IsNullOrWhiteSpace(incomingCompletionText))
            {
                session.CompletionPercent = incomingCompletionPercent;
            }

            session.CompletionText = ValueOrCurrent(incomingCompletionText, session.CompletionText);
            session.ProgressDetail = ValueOrCurrent(incomingProgressDetail, session.ProgressDetail);
        }
        session.DeldrimorPoints = ValueOrCurrent(snapshot.DeldrimorPoints, session.DeldrimorPoints);
        session.AsuraPoints = ValueOrCurrent(snapshot.AsuraPoints, session.AsuraPoints);
        session.NornPoints = ValueOrCurrent(snapshot.NornPoints, session.NornPoints);
        session.VanguardPoints = ValueOrCurrent(snapshot.VanguardPoints, session.VanguardPoints);
        session.Lockpicks = ValueOrCurrent(snapshot.Lockpicks, session.Lockpicks);
        session.Wipes = ValueOrCurrent(snapshot.Wipes, session.Wipes);
        session.RareSkins = ValueOrCurrent(snapshot.RareSkins, session.RareSkins);
        session.GoldItems = ValueOrCurrent(snapshot.GoldItems, session.GoldItems);
        session.DroppedLockpicks = ValueOrCurrent(snapshot.DroppedLockpicks, session.DroppedLockpicks);
        session.ChestsOpened = ValueOrCurrent(snapshot.ChestsOpened, session.ChestsOpened);
        session.BlackDyes = ValueOrCurrent(snapshot.BlackDyes, session.BlackDyes);
        session.Tomes = ValueOrCurrent(snapshot.Tomes, session.Tomes);
        session.BotshubIpcStatus = ValueOrCurrent(snapshot.BotshubIpcStatus, session.BotshubIpcStatus);
        session.BotshubScript = ValueOrCurrent(snapshot.BotshubScript, session.BotshubScript);
        session.BotshubPid = ValueOrCurrent(snapshot.BotshubPid, session.BotshubPid);
        session.BotshubState = ValueOrCurrent(effectiveBotshubState, session.BotshubState);
        session.BotshubMapId = ValueOrCurrent(snapshot.BotshubMapId, session.BotshubMapId);
        session.BotshubRunning = ValueOrCurrent(effectiveBotshubRunning, session.BotshubRunning);
        session.BotshubUptime = ValueOrCurrent(snapshot.BotshubUptime, session.BotshubUptime);
        session.BotshubGold = ValueOrCurrent(snapshot.BotshubGold, session.BotshubGold);
        session.BotshubSettings = ValueOrCurrent(snapshot.BotshubSettings, session.BotshubSettings);
        session.BotshubRunStats = ValueOrCurrent(snapshot.BotshubRunStats, session.BotshubRunStats);
        session.BotshubRuns = ValueOrCurrent(snapshot.BotshubRuns, session.BotshubRuns);
        session.BotshubSuccesses = ValueOrCurrent(snapshot.BotshubSuccesses, session.BotshubSuccesses);
        session.BotshubFailures = ValueOrCurrent(snapshot.BotshubFailures, session.BotshubFailures);
        session.BotshubSuccessRatio = ValueOrCurrent(snapshot.BotshubSuccessRatio, session.BotshubSuccessRatio);
        session.BotshubCurrentRunTime = ValueOrCurrent(snapshot.BotshubCurrentRunTime, session.BotshubCurrentRunTime);
        session.BotshubTotalTime = ValueOrCurrent(snapshot.BotshubTotalTime, session.BotshubTotalTime);
        session.BotshubBestRunTime = ValueOrCurrent(snapshot.BotshubBestRunTime, session.BotshubBestRunTime);
        session.BotshubAverageRunTime = ValueOrCurrent(snapshot.BotshubAverageRunTime, session.BotshubAverageRunTime);
        session.BotshubTimePerRun = ValueOrCurrent(snapshot.BotshubTimePerRun, session.BotshubTimePerRun);
        session.BotshubExperience = ValueOrCurrent(snapshot.BotshubExperience, session.BotshubExperience);
        session.BotshubChests = ValueOrCurrent(snapshot.BotshubChests, session.BotshubChests);
        session.BotshubGoldItems = ValueOrCurrent(snapshot.BotshubGoldItems, session.BotshubGoldItems);
        session.BotshubTitleSummary = ValueOrCurrent(snapshot.BotshubTitleSummary, session.BotshubTitleSummary);
        session.BotshubLootSummary = ValueOrCurrent(snapshot.BotshubLootSummary, session.BotshubLootSummary);
        session.BotshubMaterialSummary = ValueOrCurrent(snapshot.BotshubMaterialSummary, session.BotshubMaterialSummary);
        session.BotshubInventorySummary = ValueOrCurrent(snapshot.BotshubInventorySummary, session.BotshubInventorySummary);
        session.BotshubMaintenanceSummary = ValueOrCurrent(snapshot.BotshubMaintenanceSummary, session.BotshubMaintenanceSummary);
        session.BotshubStatusSource = ValueOrCurrent(snapshot.BotshubStatusSource, session.BotshubStatusSource);
        session.BotshubCommandQueue = ValueOrCurrent(snapshot.BotshubCommandQueue, session.BotshubCommandQueue);
        session.BotshubLastLog = ValueOrCurrent(snapshot.BotshubLastLog, session.BotshubLastLog);
        session.RuntimePreviousStep = ValueOrCurrent(snapshot.RuntimePreviousStep, session.RuntimePreviousStep);
        session.RuntimeCurrentStep = ValueOrCurrent(effectiveRuntimeCurrentStep, session.RuntimeCurrentStep);
        session.RuntimeNextStep = ValueOrCurrent(snapshot.RuntimeNextStep, session.RuntimeNextStep);
        session.RuntimeStateMachine = ValueOrCurrent(snapshot.RuntimeStateMachine, session.RuntimeStateMachine);
        session.RuntimeMap = ValueOrCurrent(snapshot.RuntimeMap, session.RuntimeMap);
        session.RuntimeHealth = ValueOrCurrent(snapshot.RuntimeHealth, session.RuntimeHealth);
        session.RuntimePosition = ValueOrCurrent(snapshot.RuntimePosition, session.RuntimePosition);
        session.RuntimeTarget = ValueOrCurrent(snapshot.RuntimeTarget, session.RuntimeTarget);
        session.RuntimePathing = ValueOrCurrent(snapshot.RuntimePathing, session.RuntimePathing);
        session.RuntimeCasting = ValueOrCurrent(snapshot.RuntimeCasting, session.RuntimeCasting);
        session.RuntimeSkillbar = ValueOrCurrent(snapshot.RuntimeSkillbar, session.RuntimeSkillbar);
        session.RuntimeActionQueue = ValueOrCurrent(snapshot.RuntimeActionQueue, session.RuntimeActionQueue);
        session.RuntimeOverwatch = ValueOrCurrent(snapshot.RuntimeOverwatch, session.RuntimeOverwatch);
        session.RuntimeLastUpdate = ValueOrCurrent(snapshot.RuntimeLastUpdate, session.RuntimeLastUpdate);
    }

    private void WriteStatusSnapshot(string eventName, string detail)
    {
        if (SelectedSession is null)
        {
            return;
        }

        try
        {
            statusSnapshotStore.WriteLatest(CreateLiveStatusSnapshot(eventName, detail));
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private void AddLog(string source, string level, string message)
    {
        var entry = new LogEntryViewModel(DateTime.Now, source, level, message);
        AddBoundedLogEntry(LogEntries, entry, 300);

        if (LogStreamRouter.ShouldShowInRuntimeTail(source))
        {
            AddBoundedLogEntry(RuntimeLogEntries, entry, 300);
        }

        if (LogStreamRouter.ShouldShowInLaunchSummary(source, message))
        {
            AddBoundedLogEntry(LaunchLogEntries, entry, 120);
        }
    }

    private static void AddBoundedLogEntry(ObservableCollection<LogEntryViewModel> entries, LogEntryViewModel entry, int limit)
    {
        entries.Insert(0, entry);

        while (entries.Count > limit)
        {
            entries.RemoveAt(entries.Count - 1);
        }
    }

}

internal sealed record CharacterLaunchDefaults(
    int AccountIndex,
    string CharacterName,
    string AccountLabel,
    string LaneTag,
    string BuildDirectory,
    string DllName,
    string PipeName,
    string LauncherScriptPath);

public sealed class OptionItem
{
    public OptionItem(string name, string detail, string? value = null, string? path = null)
    {
        Name = name;
        Detail = detail;
        Value = value ?? name;
        Path = path;
    }

    public string Name { get; }

    public string Detail { get; }

    public string Value { get; }

    public string? Path { get; }
}

public sealed class SessionViewModel : ObservableObject
{
    private string laneTag;
    private string characterName;
    private string botModule;
    private string dllName;
    private string pipeName;
    private string status;
    private string pid;
    private string health;
    private string botPhase;
    private string lastEvent;
    private string runCount;
    private string successCount;
    private string failureCount;
    private string failureRatio;
    private string gold;
    private string heartbeat;
    private DateTimeOffset? runtimeStartedAt;
    private DateTimeOffset? currentRunStartedAt;
    private string totalRuntime;
    private string currentRunTime;
    private string bestRunTime;
    private string averageRunTime;
    private string dungeonLevel;
    private int completionPercent;
    private string completionText;
    private string progressDetail;
    private string deldrimorPoints;
    private string asuraPoints;
    private string nornPoints;
    private string vanguardPoints;
    private string lockpicks;
    private string wipes;
    private string rareSkins;
    private string goldItems;
    private string droppedLockpicks;
    private string chestsOpened;
    private string blackDyes;
    private string tomes;
    private string botshubIpcStatus;
    private string botshubScript;
    private string botshubPid;
    private string botshubState;
    private string botshubMapId;
    private string botshubRunning;
    private string botshubUptime;
    private string botshubGold;
    private string botshubSettings;
    private string botshubRunStats;
    private string botshubRuns;
    private string botshubSuccesses;
    private string botshubFailures;
    private string botshubSuccessRatio;
    private string botshubCurrentRunTime;
    private string botshubTotalTime;
    private string botshubBestRunTime;
    private string botshubAverageRunTime;
    private string botshubTimePerRun;
    private string botshubExperience;
    private string botshubChests;
    private string botshubGoldItems;
    private string botshubTitleSummary;
    private string botshubLootSummary;
    private string botshubMaterialSummary;
    private string botshubInventorySummary;
    private string botshubMaintenanceSummary;
    private string botshubStatusSource;
    private string botshubCommandQueue;
    private string botshubLastLog;
    private string py4GwPreviousStep;
    private string py4GwCurrentStep;
    private string py4GwNextStep;
    private string py4GwStateMachine;
    private string py4GwMap;
    private string py4GwHealth;
    private string py4GwPosition;
    private string py4GwTarget;
    private string py4GwPathing;
    private string py4GwCasting;
    private string py4GwSkillbar;
    private string py4GwActionQueue;
    private string py4GwOverwatch;
    private string py4GwLastUpdate;
    private readonly HashSet<int> completedRunNumbers = new();
    private long completedRunDurationMilliseconds;
    private int? bestRunDurationMilliseconds;

    public SessionViewModel(string laneTag, string characterName, string botModule, string dllName, string pipeName, string status, string pid)
    {
        this.laneTag = laneTag;
        this.characterName = characterName;
        this.botModule = botModule;
        this.dllName = dllName;
        this.pipeName = pipeName;
        this.status = status;
        this.pid = pid;
        health = "not running";
        botPhase = "Idle";
        lastEvent = "";
        runCount = "0";
        successCount = "0";
        failureCount = "0";
        failureRatio = "0% failed";
        gold = "n/a";
        heartbeat = "pending";
        totalRuntime = "0:00:00";
        currentRunTime = "00:00:00";
        bestRunTime = "00:00:00";
        averageRunTime = "00:00:00";
        dungeonLevel = "not in dungeon";
        completionPercent = 0;
        completionText = "0%";
        progressDetail = "waiting for route telemetry";
        deldrimorPoints = "0";
        asuraPoints = "0";
        nornPoints = "0";
        vanguardPoints = "0";
        lockpicks = "n/a";
        wipes = "0";
        rareSkins = "0";
        goldItems = "0";
        droppedLockpicks = "0";
        chestsOpened = "0";
        blackDyes = "0";
        tomes = "0";
        botshubIpcStatus = "not attached";
        botshubScript = "n/a";
        botshubPid = "n/a";
        botshubState = "n/a";
        botshubMapId = "n/a";
        botshubRunning = "n/a";
        botshubUptime = "n/a";
        botshubGold = "n/a";
        botshubSettings = "n/a";
        botshubRunStats = "n/a";
        botshubRuns = "n/a";
        botshubSuccesses = "n/a";
        botshubFailures = "n/a";
        botshubSuccessRatio = "n/a";
        botshubCurrentRunTime = "n/a";
        botshubTotalTime = "n/a";
        botshubBestRunTime = "n/a";
        botshubAverageRunTime = "n/a";
        botshubTimePerRun = "n/a";
        botshubExperience = "n/a";
        botshubChests = "n/a";
        botshubGoldItems = "n/a";
        botshubTitleSummary = "n/a";
        botshubLootSummary = "n/a";
        botshubMaterialSummary = "n/a";
        botshubInventorySummary = "n/a";
        botshubMaintenanceSummary = "n/a";
        botshubStatusSource = "n/a";
        botshubCommandQueue = "no queue telemetry";
        botshubLastLog = "n/a";
        py4GwPreviousStep = "n/a";
        py4GwCurrentStep = "waiting for state telemetry";
        py4GwNextStep = "n/a";
        py4GwStateMachine = "not started";
        py4GwMap = "n/a";
        py4GwHealth = "n/a";
        py4GwPosition = "n/a";
        py4GwTarget = "n/a";
        py4GwPathing = "waiting for pathing telemetry";
        py4GwCasting = "n/a";
        py4GwSkillbar = "n/a";
        py4GwActionQueue = "no queue telemetry";
        py4GwOverwatch = "n/a";
        py4GwLastUpdate = "n/a";
    }

    public string LaneTag
    {
        get => laneTag;
        set => SetProperty(ref laneTag, value);
    }

    public string CharacterName
    {
        get => characterName;
        set => SetProperty(ref characterName, value);
    }

    public string DllName
    {
        get => dllName;
        set => SetProperty(ref dllName, value);
    }

    public string PipeName
    {
        get => pipeName;
        set => SetProperty(ref pipeName, value);
    }

    public string BotModule
    {
        get => botModule;
        set => SetProperty(ref botModule, value);
    }

    public string Status
    {
        get => status;
        set => SetProperty(ref status, value);
    }

    public string Pid
    {
        get => pid;
        set => SetProperty(ref pid, value);
    }

    public string Health
    {
        get => health;
        set => SetProperty(ref health, value);
    }

    public string BotPhase
    {
        get => botPhase;
        set => SetProperty(ref botPhase, value);
    }

    public string LastEvent
    {
        get => lastEvent;
        set => SetProperty(ref lastEvent, value);
    }

    public string RunCount
    {
        get => runCount;
        set
        {
            if (SetProperty(ref runCount, value))
            {
                UpdateRunDerivedCounters();
            }
        }
    }

    public string SuccessCount
    {
        get => successCount;
        set => SetProperty(ref successCount, value);
    }

    public string FailureCount
    {
        get => failureCount;
        set
        {
            if (SetProperty(ref failureCount, value))
            {
                UpdateRunDerivedCounters();
            }
        }
    }

    public string FailureRatio
    {
        get => failureRatio;
        set => SetProperty(ref failureRatio, value);
    }

    public string Gold
    {
        get => gold;
        set => SetProperty(ref gold, value);
    }

    public string Heartbeat
    {
        get => heartbeat;
        set => SetProperty(ref heartbeat, value);
    }

    public DateTimeOffset? RuntimeStartedAt
    {
        get => runtimeStartedAt;
        private set => SetProperty(ref runtimeStartedAt, value);
    }

    public string TotalRuntime
    {
        get => totalRuntime;
        set => SetProperty(ref totalRuntime, value);
    }

    public DateTimeOffset? CurrentRunStartedAt
    {
        get => currentRunStartedAt;
        private set => SetProperty(ref currentRunStartedAt, value);
    }

    public string CurrentRunTime
    {
        get => currentRunTime;
        set => SetProperty(ref currentRunTime, value);
    }

    public string BestRunTime
    {
        get => bestRunTime;
        set => SetProperty(ref bestRunTime, value);
    }

    public string AverageRunTime
    {
        get => averageRunTime;
        set => SetProperty(ref averageRunTime, value);
    }

    public void MarkRuntimeStarted(DateTimeOffset startedAt)
    {
        if (RuntimeStartedAt is null || startedAt < RuntimeStartedAt.Value)
        {
            RuntimeStartedAt = startedAt;
        }

        RefreshTimers(DateTimeOffset.Now);
    }

    public void MarkCurrentRunStarted(DateTimeOffset startedAt)
    {
        CurrentRunStartedAt = startedAt;
        RefreshCurrentRunTime(DateTimeOffset.Now);
    }

    public void RecordRunStarted(int runNumber, DateTimeOffset startedAt)
    {
        SetRunCountAtLeast(runNumber);
        MarkCurrentRunStarted(startedAt);
    }

    public void RecordRunCompletion(int runNumber, int durationMilliseconds, int bestMilliseconds)
    {
        SetRunCountAtLeast(runNumber);

        if (durationMilliseconds > 0 && completedRunNumbers.Add(runNumber))
        {
            completedRunDurationMilliseconds += durationMilliseconds;
        }

        var bestCandidate = bestMilliseconds > 0 ? bestMilliseconds : durationMilliseconds;
        if (bestCandidate > 0)
        {
            bestRunDurationMilliseconds = bestRunDurationMilliseconds is int currentBest
                ? Math.Min(currentBest, bestCandidate)
                : bestCandidate;
        }

        CurrentRunStartedAt = null;
        CurrentRunTime = durationMilliseconds > 0
            ? FormatDuration(TimeSpan.FromMilliseconds(durationMilliseconds))
            : "00:00:00";
        BestRunTime = bestRunDurationMilliseconds is int best
            ? FormatDuration(TimeSpan.FromMilliseconds(best))
            : "00:00:00";
        AverageRunTime = completedRunNumbers.Count > 0
            ? FormatDuration(TimeSpan.FromMilliseconds(completedRunDurationMilliseconds / completedRunNumbers.Count))
            : "00:00:00";
        SuccessCount = completedRunNumbers.Count.ToString(CultureInfo.InvariantCulture);
        UpdateRunDerivedCounters();
    }

    public void RefreshTimers(DateTimeOffset now)
    {
        RefreshTotalRuntime(now);
        RefreshCurrentRunTime(now);
    }

    public void RefreshTotalRuntime(DateTimeOffset now)
    {
        if (RuntimeStartedAt is not DateTimeOffset startedAt)
        {
            return;
        }

        var elapsed = now - startedAt;
        if (elapsed < TimeSpan.Zero)
        {
            elapsed = TimeSpan.Zero;
        }

        TotalRuntime = elapsed.TotalDays >= 1.0
            ? $"{(int)elapsed.TotalDays}d {elapsed:hh\\:mm\\:ss}"
            : elapsed.ToString("h\\:mm\\:ss", CultureInfo.InvariantCulture);
    }

    public void RefreshCurrentRunTime(DateTimeOffset now)
    {
        if (CurrentRunStartedAt is not DateTimeOffset startedAt)
        {
            return;
        }

        var elapsed = now - startedAt;
        if (elapsed < TimeSpan.Zero)
        {
            elapsed = TimeSpan.Zero;
        }

        CurrentRunTime = FormatDuration(elapsed);
    }

    public void ResetRuntime()
    {
        RuntimeStartedAt = null;
        TotalRuntime = "0:00:00";
        CurrentRunStartedAt = null;
        CurrentRunTime = "00:00:00";
    }

    public void ResetMonitoringStatistics()
    {
        completedRunNumbers.Clear();
        completedRunDurationMilliseconds = 0;
        bestRunDurationMilliseconds = null;
        RunCount = "0";
        SuccessCount = "0";
        FailureCount = "0";
        FailureRatio = "0% failed";
        Gold = "n/a";
        Heartbeat = "pending";
        ResetRuntime();
        BestRunTime = "00:00:00";
        AverageRunTime = "00:00:00";
        DeldrimorPoints = "0";
        AsuraPoints = "0";
        NornPoints = "0";
        VanguardPoints = "0";
        Lockpicks = "n/a";
        Wipes = "0";
        RareSkins = "0";
        GoldItems = "0";
        DroppedLockpicks = "0";
        ChestsOpened = "0";
        BlackDyes = "0";
        Tomes = "0";
        ResetExternalMonitoring();
    }

    public void PrepareLogStatisticsHydration()
    {
        completedRunNumbers.Clear();
        completedRunDurationMilliseconds = 0;
        bestRunDurationMilliseconds = null;
        RunCount = "0";
        SuccessCount = "0";
        FailureCount = "0";
        FailureRatio = "0% failed";
        CurrentRunStartedAt = null;
        CurrentRunTime = "00:00:00";
        BestRunTime = "00:00:00";
        AverageRunTime = "00:00:00";
        ChestsOpened = "0";
        Wipes = "0";
    }

    public void ResetExternalMonitoring()
    {
        BotshubIpcStatus = "not attached";
        BotshubScript = "n/a";
        BotshubPid = "n/a";
        BotshubState = "n/a";
        BotshubMapId = "n/a";
        BotshubRunning = "n/a";
        BotshubUptime = "n/a";
        BotshubGold = "n/a";
        BotshubSettings = "n/a";
        BotshubRunStats = "n/a";
        BotshubRuns = "n/a";
        BotshubSuccesses = "n/a";
        BotshubFailures = "n/a";
        BotshubSuccessRatio = "n/a";
        BotshubCurrentRunTime = "n/a";
        BotshubTotalTime = "n/a";
        BotshubBestRunTime = "n/a";
        BotshubAverageRunTime = "n/a";
        BotshubTimePerRun = "n/a";
        BotshubExperience = "n/a";
        BotshubChests = "n/a";
        BotshubGoldItems = "n/a";
        BotshubTitleSummary = "n/a";
        BotshubLootSummary = "n/a";
        BotshubMaterialSummary = "n/a";
        BotshubInventorySummary = "n/a";
        BotshubMaintenanceSummary = "n/a";
        BotshubStatusSource = "n/a";
        BotshubCommandQueue = "no queue telemetry";
        BotshubLastLog = "n/a";
        RuntimePreviousStep = "n/a";
        RuntimeCurrentStep = "waiting for state telemetry";
        RuntimeNextStep = "n/a";
        RuntimeStateMachine = "not started";
        RuntimeMap = "n/a";
        RuntimeHealth = "n/a";
        RuntimePosition = "n/a";
        RuntimeTarget = "n/a";
        RuntimePathing = "waiting for pathing telemetry";
        RuntimeCasting = "n/a";
        RuntimeSkillbar = "n/a";
        RuntimeActionQueue = "no queue telemetry";
        RuntimeOverwatch = "n/a";
        RuntimeLastUpdate = "n/a";
    }

    public void IncrementFailureCount()
    {
        FailureCount = (ParseInt(FailureCount) + 1).ToString(CultureInfo.InvariantCulture);
    }

    public void ApplyMonitoringStats(FroggyMonitoringStatsSnapshot snapshot)
    {
        DeldrimorPoints = snapshot.DeldrimorPoints.ToString(CultureInfo.InvariantCulture);
        AsuraPoints = snapshot.AsuraPoints.ToString(CultureInfo.InvariantCulture);
        NornPoints = snapshot.NornPoints.ToString(CultureInfo.InvariantCulture);
        VanguardPoints = snapshot.VanguardPoints.ToString(CultureInfo.InvariantCulture);
        Lockpicks = snapshot.Lockpicks.ToString(CultureInfo.InvariantCulture);
        Wipes = snapshot.Wipes.ToString(CultureInfo.InvariantCulture);
        RareSkins = Math.Max(snapshot.RareSkins, snapshot.SkinsPicked ?? 0).ToString(CultureInfo.InvariantCulture);
        GoldItems = snapshot.GoldItems.ToString(CultureInfo.InvariantCulture);
        DroppedLockpicks = (snapshot.LockpicksGained ?? snapshot.DroppedLockpicks).ToString(CultureInfo.InvariantCulture);
        ChestsOpened = snapshot.ChestsOpened.ToString(CultureInfo.InvariantCulture);
        BlackDyes = snapshot.BlackDyes.ToString(CultureInfo.InvariantCulture);
        Tomes = snapshot.Tomes.ToString(CultureInfo.InvariantCulture);

        if (snapshot.CharacterGold is not null || snapshot.StorageGold is not null)
        {
            Gold = FormatGoldBuckets(snapshot.CharacterGold, snapshot.StorageGold);
        }

        ApplyNativeMonitoringCounters(snapshot);

        if (HasExtendedNativeMonitoringStats(snapshot))
        {
            ApplyNativeMonitoringPanel(snapshot);
        }
    }

    private void ApplyNativeMonitoringCounters(FroggyMonitoringStatsSnapshot snapshot)
    {
        BotshubChests = ChestsOpened;
        BotshubGoldItems = GoldItems;
        BotshubTitleSummary = BuildNativeTitleSummary(snapshot);
        BotshubLootSummary = BuildNativeLootSummary(snapshot);
        BotshubStatusSource = "native Froggy MonitoringStats";
        BotshubLastLog = FormatMonitoringStatsReason(snapshot.Reason);
    }

    private void ApplyNativeMonitoringPanel(FroggyMonitoringStatsSnapshot snapshot)
    {
        var now = DateTime.Now.ToString("HH:mm:ss", CultureInfo.InvariantCulture);

        BotshubIpcStatus = $"native GWA3 {now}";
        BotshubScript = BotModule;
        BotshubPid = EmptyToDisplay(Pid, "not launched");
        var runtimeState = PreferredRuntimeDisplayState();
        BotshubState = runtimeState;
        BotshubMapId = snapshot.MapId?.ToString(CultureInfo.InvariantCulture) ?? EmptyToDisplay(BotshubMapId, "0");
        BotshubRunning = IsRuntimeStatusRunning(Status, runtimeState) ? "running" : "stopped";
        BotshubUptime = TotalRuntime;
        BotshubGold = Gold;
        BotshubRuns = RunCount;
        BotshubSuccesses = SuccessCount;
        BotshubFailures = FailureCount;
        BotshubSuccessRatio = FormatSuccessRatio(RunCount, SuccessCount);
        BotshubCurrentRunTime = CurrentRunTime;
        BotshubTotalTime = TotalRuntime;
        BotshubBestRunTime = BestRunTime;
        BotshubAverageRunTime = AverageRunTime;
        BotshubTimePerRun = AverageRunTime;
        BotshubExperience = FormatOptionalNumber(snapshot.Experience, BotshubExperience, "0");
        BotshubChests = ChestsOpened;
        BotshubGoldItems = GoldItems;
        BotshubTitleSummary = BuildNativeTitleSummary(snapshot);
        BotshubLootSummary = BuildNativeLootSummary(snapshot);
        BotshubMaterialSummary = BuildNativeMaterialSummary(snapshot);
        BotshubInventorySummary = BuildNativeInventorySummary(snapshot);
        BotshubMaintenanceSummary = BuildNativeMaintenanceSummary(snapshot);
        BotshubRunStats = $"runs {RunCount}, success {SuccessCount}, fails {FailureCount}, current {CurrentRunTime}, total {TotalRuntime}, best {BestRunTime}, avg {AverageRunTime}";
        BotshubStatusSource = "native Froggy MonitoringStats";
        BotshubLastLog = FormatMonitoringStatsReason(snapshot.Reason);

        if (BuildSkillbarSummary(snapshot) is { Length: > 0 } skillbarSummary)
        {
            RuntimeSkillbar = skillbarSummary;
        }
    }

    private static bool HasExtendedNativeMonitoringStats(FroggyMonitoringStatsSnapshot snapshot) =>
        snapshot.CharacterGold is not null ||
        snapshot.StorageGold is not null ||
        snapshot.Experience is not null ||
        snapshot.MapId is not null ||
        snapshot.FreeSlots is not null ||
        snapshot.SkillbarReady is not null;

    private static string FormatMonitoringStatsReason(string? reason)
    {
        if (string.IsNullOrWhiteSpace(reason))
        {
            return "MonitoringStats snapshot";
        }

        var trimmed = reason.Trim();
        return IsAllDigits(trimmed)
            ? "MonitoringStats waypoint-loot"
            : $"MonitoringStats {trimmed}";
    }

    private static bool IsAllDigits(string value)
    {
        foreach (var ch in value)
        {
            if (!char.IsDigit(ch))
            {
                return false;
            }
        }

        return value.Length > 0;
    }

    private static string FormatGoldBuckets(int? characterGold, int? storageGold) =>
        $"char {FormatOptionalNumber(characterGold, "0", "0")} / storage {FormatOptionalNumber(storageGold, "0", "0")}";

    private static string FormatOptionalNumber(int? value, string current, string fallback) =>
        value?.ToString("N0", CultureInfo.InvariantCulture)
        ?? (IsUnavailableValue(current) ? fallback : current);

    private static string EmptyToDisplay(string? value, string fallback) =>
        string.IsNullOrWhiteSpace(value) ? fallback : value.Trim();

    private string PreferredRuntimeDisplayState()
    {
        if (!IsUnavailableValue(RuntimeCurrentStep) &&
            !RuntimeCurrentStep.Contains("waiting", StringComparison.OrdinalIgnoreCase))
        {
            return RuntimeCurrentStep.Trim();
        }

        return EmptyToDisplay(BotPhase, Status);
    }

    private static bool IsRuntimeStatusRunning(string status, string phase) =>
        status.Contains("running", StringComparison.OrdinalIgnoreCase) ||
        status.Contains("dungeon", StringComparison.OrdinalIgnoreCase) ||
        status.Contains("town", StringComparison.OrdinalIgnoreCase) ||
        status.Contains("travel", StringComparison.OrdinalIgnoreCase) ||
        status.Contains("awaiting", StringComparison.OrdinalIgnoreCase) ||
        status.Contains("injected", StringComparison.OrdinalIgnoreCase) ||
        phase.Contains("running", StringComparison.OrdinalIgnoreCase) ||
        phase.Contains("dungeon", StringComparison.OrdinalIgnoreCase) ||
        phase.Contains("town", StringComparison.OrdinalIgnoreCase) ||
        phase.Contains("travel", StringComparison.OrdinalIgnoreCase) ||
        phase.Contains("awaiting", StringComparison.OrdinalIgnoreCase);

    private static bool IsUnavailableValue(string? value) =>
        string.IsNullOrWhiteSpace(value) ||
        string.Equals(value.Trim(), "n/a", StringComparison.OrdinalIgnoreCase) ||
        string.Equals(value.Trim(), "---", StringComparison.OrdinalIgnoreCase);

    private static string FormatSuccessRatio(string runCount, string successCount)
    {
        var runs = ParseInt(runCount);
        var successes = ParseInt(successCount);
        return runs > 0
            ? $"{successes * 100.0 / runs:0.#}%"
            : "0%";
    }

    private static string BuildNativeTitleSummary(FroggyMonitoringStatsSnapshot snapshot) =>
        string.Join("; ", new[]
        {
            FormatTitleCounter("Asura", snapshot.AsuraPoints, snapshot.AsuraTotal),
            FormatTitleCounter("Deldrimor", snapshot.DeldrimorPoints, snapshot.DeldrimorTotal),
            FormatTitleCounter("Norn", snapshot.NornPoints, snapshot.NornTotal),
            FormatTitleCounter("Vanguard", snapshot.VanguardPoints, snapshot.VanguardTotal)
        });

    private static string FormatTitleCounter(string label, int gained, int? total) =>
        total is int totalValue
            ? $"{label} +{gained.ToString("N0", CultureInfo.InvariantCulture)} ({totalValue.ToString("N0", CultureInfo.InvariantCulture)} total)"
            : $"{label} +{gained.ToString("N0", CultureInfo.InvariantCulture)}";

    private static string BuildNativeLootSummary(FroggyMonitoringStatsSnapshot snapshot) =>
        $"items {FormatOptionalNumber(snapshot.ItemsPicked, snapshot.ItemsPicked?.ToString(CultureInfo.InvariantCulture) ?? "0", "0")} picked; lockpicks {snapshot.Lockpicks.ToString("N0", CultureInfo.InvariantCulture)} held, {(snapshot.LockpicksGained ?? snapshot.DroppedLockpicks).ToString("N0", CultureInfo.InvariantCulture)} gained; gold items {snapshot.GoldItems.ToString("N0", CultureInfo.InvariantCulture)}; rare skins {Math.Max(snapshot.RareSkins, snapshot.SkinsPicked ?? 0).ToString("N0", CultureInfo.InvariantCulture)}; tomes {snapshot.Tomes.ToString("N0", CultureInfo.InvariantCulture)}; black dyes {snapshot.BlackDyes.ToString("N0", CultureInfo.InvariantCulture)}";

    private static string BuildNativeMaterialSummary(FroggyMonitoringStatsSnapshot snapshot)
    {
        var dust = (snapshot.DustInventory ?? 0) + (snapshot.DustStorage ?? 0);
        var iron = (snapshot.IronInventory ?? 0) + (snapshot.IronStorage ?? 0);
        var bones = (snapshot.BonesInventory ?? 0) + (snapshot.BonesStorage ?? 0);
        var feathers = (snapshot.FeathersInventory ?? 0) + (snapshot.FeathersStorage ?? 0);
        return $"dust {dust.ToString("N0", CultureInfo.InvariantCulture)}, iron {iron.ToString("N0", CultureInfo.InvariantCulture)}, bones {bones.ToString("N0", CultureInfo.InvariantCulture)}, feathers {feathers.ToString("N0", CultureInfo.InvariantCulture)}";
    }

    private static string BuildNativeInventorySummary(FroggyMonitoringStatsSnapshot snapshot)
    {
        var freeSlots = FormatOptionalNumber(snapshot.FreeSlots, "0", "0");
        var idKits = FormatOptionalNumber(snapshot.IdentificationKits, "0", "0");
        var salvageKits = FormatOptionalNumber(snapshot.SalvageKits, "0", "0");
        var highGrade = FormatOptionalNumber(snapshot.HighGradeSalvageKits, "0", "0");
        var consetsInventory = FormatOptionalNumber(snapshot.ConsetsInventory, "0", "0");
        var consetsStorage = FormatOptionalNumber(snapshot.ConsetsStorage, "0", "0");
        return $"free slots {freeSlots}, ID kits {idKits}, salvage kits {salvageKits} ({highGrade} high-grade), consets {consetsInventory} carried / {consetsStorage} stored";
    }

    private static string BuildNativeMaintenanceSummary(FroggyMonitoringStatsSnapshot snapshot)
    {
        var freeSlots = snapshot.FreeSlots ?? 0;
        var idKits = snapshot.IdentificationKits ?? 0;
        var salvageKits = snapshot.SalvageKits ?? 0;
        var highGrade = snapshot.HighGradeSalvageKits ?? 0;
        var characterGold = snapshot.CharacterGold ?? 0;
        var needsAttention = freeSlots < 5 || idKits == 0 || salvageKits == 0 || highGrade == 0 || characterGold >= 100_000;
        return needsAttention
            ? $"attention: free slots {freeSlots}, ID kits {idKits}, salvage kits {salvageKits}, high-grade {highGrade}, gold {characterGold.ToString("N0", CultureInfo.InvariantCulture)}"
            : $"ready: free slots {freeSlots}, ID kits {idKits}, salvage kits {salvageKits}, high-grade {highGrade}, gold {characterGold.ToString("N0", CultureInfo.InvariantCulture)}";
    }

    private static string BuildSkillbarSummary(FroggyMonitoringStatsSnapshot snapshot)
    {
        if (snapshot.SkillbarReady is null && snapshot.SkillbarSkillIds.Count == 0)
        {
            return "";
        }

        var ids = snapshot.SkillbarSkillIds.Count == 0
            ? "no ids"
            : string.Join(",", snapshot.SkillbarSkillIds);
        var loaded = snapshot.SkillbarReady == true ? "loaded" : "unavailable";
        return $"{loaded} {snapshot.SkillbarNonZero.GetValueOrDefault()}/8 [{ids}]";
    }

    public void IncrementWipes(int count = 1)
    {
        Wipes = (ParseInt(Wipes) + Math.Max(0, count)).ToString(CultureInfo.InvariantCulture);
    }

    public void IncrementChestsOpened(int count = 1)
    {
        ChestsOpened = (ParseInt(ChestsOpened) + Math.Max(0, count)).ToString(CultureInfo.InvariantCulture);
    }

    private void SetRunCountAtLeast(int runNumber)
    {
        if (runNumber <= 0)
        {
            return;
        }

        var current = ParseInt(RunCount);
        if (runNumber > current)
        {
            RunCount = runNumber.ToString(CultureInfo.InvariantCulture);
        }
    }

    private void UpdateRunDerivedCounters()
    {
        var runs = ParseInt(RunCount);
        var failures = ParseInt(FailureCount);
        FailureRatio = runs > 0
            ? $"{(failures * 100.0 / runs):0.#}% failed"
            : "0% failed";
    }

    private static int ParseInt(string value) =>
        int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out var parsed)
            ? parsed
            : 0;

    private static string FormatDuration(TimeSpan elapsed) =>
        elapsed.TotalDays >= 1.0
            ? $"{(int)elapsed.TotalDays}d {elapsed:hh\\:mm\\:ss}"
            : elapsed.ToString("hh\\:mm\\:ss", CultureInfo.InvariantCulture);

    public string DungeonLevel
    {
        get => dungeonLevel;
        set => SetProperty(ref dungeonLevel, value);
    }

    public int CompletionPercent
    {
        get => completionPercent;
        set => SetProperty(ref completionPercent, Math.Clamp(value, 0, 100));
    }

    public string CompletionText
    {
        get => completionText;
        set => SetProperty(ref completionText, value);
    }

    public string ProgressDetail
    {
        get => progressDetail;
        set => SetProperty(ref progressDetail, value);
    }

    public string DeldrimorPoints
    {
        get => deldrimorPoints;
        set => SetProperty(ref deldrimorPoints, value);
    }

    public string AsuraPoints
    {
        get => asuraPoints;
        set => SetProperty(ref asuraPoints, value);
    }

    public string NornPoints
    {
        get => nornPoints;
        set => SetProperty(ref nornPoints, value);
    }

    public string VanguardPoints
    {
        get => vanguardPoints;
        set => SetProperty(ref vanguardPoints, value);
    }

    public string Lockpicks
    {
        get => lockpicks;
        set => SetProperty(ref lockpicks, value);
    }

    public string Wipes
    {
        get => wipes;
        set => SetProperty(ref wipes, value);
    }

    public string RareSkins
    {
        get => rareSkins;
        set => SetProperty(ref rareSkins, value);
    }

    public string GoldItems
    {
        get => goldItems;
        set => SetProperty(ref goldItems, value);
    }

    public string DroppedLockpicks
    {
        get => droppedLockpicks;
        set => SetProperty(ref droppedLockpicks, value);
    }

    public string ChestsOpened
    {
        get => chestsOpened;
        set => SetProperty(ref chestsOpened, value);
    }

    public string BlackDyes
    {
        get => blackDyes;
        set => SetProperty(ref blackDyes, value);
    }

    public string Tomes
    {
        get => tomes;
        set => SetProperty(ref tomes, value);
    }

    public string BotshubIpcStatus
    {
        get => botshubIpcStatus;
        set => SetProperty(ref botshubIpcStatus, value);
    }

    public string BotshubScript
    {
        get => botshubScript;
        set => SetProperty(ref botshubScript, value);
    }

    public string BotshubPid
    {
        get => botshubPid;
        set => SetProperty(ref botshubPid, value);
    }

    public string BotshubState
    {
        get => botshubState;
        set => SetProperty(ref botshubState, value);
    }

    public string BotshubMapId
    {
        get => botshubMapId;
        set => SetProperty(ref botshubMapId, value);
    }

    public string BotshubRunning
    {
        get => botshubRunning;
        set => SetProperty(ref botshubRunning, value);
    }

    public string BotshubUptime
    {
        get => botshubUptime;
        set => SetProperty(ref botshubUptime, value);
    }

    public string BotshubGold
    {
        get => botshubGold;
        set => SetProperty(ref botshubGold, value);
    }

    public string BotshubSettings
    {
        get => botshubSettings;
        set => SetProperty(ref botshubSettings, value);
    }

    public string BotshubRunStats
    {
        get => botshubRunStats;
        set => SetProperty(ref botshubRunStats, value);
    }

    public string BotshubRuns
    {
        get => botshubRuns;
        set => SetProperty(ref botshubRuns, value);
    }

    public string BotshubSuccesses
    {
        get => botshubSuccesses;
        set => SetProperty(ref botshubSuccesses, value);
    }

    public string BotshubFailures
    {
        get => botshubFailures;
        set => SetProperty(ref botshubFailures, value);
    }

    public string BotshubSuccessRatio
    {
        get => botshubSuccessRatio;
        set => SetProperty(ref botshubSuccessRatio, value);
    }

    public string BotshubCurrentRunTime
    {
        get => botshubCurrentRunTime;
        set => SetProperty(ref botshubCurrentRunTime, value);
    }

    public string BotshubTotalTime
    {
        get => botshubTotalTime;
        set => SetProperty(ref botshubTotalTime, value);
    }

    public string BotshubBestRunTime
    {
        get => botshubBestRunTime;
        set => SetProperty(ref botshubBestRunTime, value);
    }

    public string BotshubAverageRunTime
    {
        get => botshubAverageRunTime;
        set => SetProperty(ref botshubAverageRunTime, value);
    }

    public string BotshubTimePerRun
    {
        get => botshubTimePerRun;
        set => SetProperty(ref botshubTimePerRun, value);
    }

    public string BotshubExperience
    {
        get => botshubExperience;
        set => SetProperty(ref botshubExperience, value);
    }

    public string BotshubChests
    {
        get => botshubChests;
        set => SetProperty(ref botshubChests, value);
    }

    public string BotshubGoldItems
    {
        get => botshubGoldItems;
        set => SetProperty(ref botshubGoldItems, value);
    }

    public string BotshubTitleSummary
    {
        get => botshubTitleSummary;
        set => SetProperty(ref botshubTitleSummary, value);
    }

    public string BotshubLootSummary
    {
        get => botshubLootSummary;
        set => SetProperty(ref botshubLootSummary, value);
    }

    public string BotshubMaterialSummary
    {
        get => botshubMaterialSummary;
        set => SetProperty(ref botshubMaterialSummary, value);
    }

    public string BotshubInventorySummary
    {
        get => botshubInventorySummary;
        set => SetProperty(ref botshubInventorySummary, value);
    }

    public string BotshubMaintenanceSummary
    {
        get => botshubMaintenanceSummary;
        set => SetProperty(ref botshubMaintenanceSummary, value);
    }

    public string BotshubStatusSource
    {
        get => botshubStatusSource;
        set => SetProperty(ref botshubStatusSource, value);
    }

    public string BotshubCommandQueue
    {
        get => botshubCommandQueue;
        set => SetProperty(ref botshubCommandQueue, value);
    }

    public string BotshubLastLog
    {
        get => botshubLastLog;
        set => SetProperty(ref botshubLastLog, value);
    }

    public string RuntimePreviousStep
    {
        get => py4GwPreviousStep;
        set => SetProperty(ref py4GwPreviousStep, value);
    }

    public string RuntimeCurrentStep
    {
        get => py4GwCurrentStep;
        set => SetProperty(ref py4GwCurrentStep, value);
    }

    public string RuntimeNextStep
    {
        get => py4GwNextStep;
        set => SetProperty(ref py4GwNextStep, value);
    }

    public string RuntimeStateMachine
    {
        get => py4GwStateMachine;
        set => SetProperty(ref py4GwStateMachine, value);
    }

    public string RuntimeMap
    {
        get => py4GwMap;
        set => SetProperty(ref py4GwMap, value);
    }

    public string RuntimeHealth
    {
        get => py4GwHealth;
        set => SetProperty(ref py4GwHealth, value);
    }

    public string RuntimePosition
    {
        get => py4GwPosition;
        set => SetProperty(ref py4GwPosition, value);
    }

    public string RuntimeTarget
    {
        get => py4GwTarget;
        set => SetProperty(ref py4GwTarget, value);
    }

    public string RuntimePathing
    {
        get => py4GwPathing;
        set => SetProperty(ref py4GwPathing, value);
    }

    public string RuntimeCasting
    {
        get => py4GwCasting;
        set => SetProperty(ref py4GwCasting, value);
    }

    public string RuntimeSkillbar
    {
        get => py4GwSkillbar;
        set => SetProperty(ref py4GwSkillbar, value);
    }

    public string RuntimeActionQueue
    {
        get => py4GwActionQueue;
        set => SetProperty(ref py4GwActionQueue, value);
    }

    public string RuntimeOverwatch
    {
        get => py4GwOverwatch;
        set => SetProperty(ref py4GwOverwatch, value);
    }

    public string RuntimeLastUpdate
    {
        get => py4GwLastUpdate;
        set => SetProperty(ref py4GwLastUpdate, value);
    }
}

public sealed class ValidationCheckViewModel
{
    public ValidationCheckViewModel(string name, bool passed, string detail)
        : this(name, passed ? "OK" : "WARN", detail)
    {
    }

    public ValidationCheckViewModel(string name, string state, string detail)
    {
        Name = name;
        State = state;
        Detail = detail;
    }

    public string Name { get; }

    public bool Passed => string.Equals(State, "OK", StringComparison.OrdinalIgnoreCase);

    public string State { get; }

    public string Detail { get; }
}

public sealed class HeroBuildViewModel
{
    public HeroBuildViewModel(string slot, string name, string template, string role, string behavior)
    {
        Slot = slot;
        Name = name;
        Template = template;
        Role = role;
        Behavior = behavior;
    }

    public string Slot { get; set; }

    public string Name { get; set; }

    public string Template { get; set; }

    public string Role { get; set; }

    public string Behavior { get; set; }
}

public sealed class InventoryPolicyViewModel
{
    public InventoryPolicyViewModel(string category, string rule, string decision, string notes)
    {
        Category = category;
        Rule = rule;
        Decision = decision;
        Notes = notes;
    }

    public string Category { get; set; }

    public string Rule { get; set; }

    public string Decision { get; set; }

    public string Notes { get; set; }
}

public sealed class UpgradeSalvageRuleViewModel
{
    private readonly HashSet<string> itemTypes;

    public UpgradeSalvageRuleViewModel(UpgradeSalvageRule rule)
    {
        Enabled = rule.Enabled;
        Group = string.IsNullOrWhiteSpace(rule.Group) ? "Custom" : rule.Group;
        Name = rule.Name;
        ModifierPattern = rule.ModifierPattern;
        SalvageIndex = rule.SalvageIndex;
        SalvageSlot = rule.SalvageIndex switch
        {
            0 => "Prefix",
            1 => "Suffix/Rune",
            2 => "Inscription",
            _ => $"Index {rule.SalvageIndex}"
        };
        Action = rule.Action.ToString();
        MinimumRarity = rule.MinimumRarity.ToString();
        itemTypes = new HashSet<string>(rule.ItemTypes ?? new List<string>(), StringComparer.OrdinalIgnoreCase);
        ItemTypesSummary = itemTypes.Count == 0
            ? "All"
            : string.Join(", ", itemTypes.Order(StringComparer.OrdinalIgnoreCase).Select(FormatItemType));
        AllTypes = itemTypes.Count == 0;
    }

    public bool Enabled { get; set; }

    public string Group { get; set; }

    public string Name { get; set; }

    public string ModifierPattern { get; set; }

    public int SalvageIndex { get; set; }

    public string SalvageSlot { get; set; }

    public string Action { get; set; }

    public string MinimumRarity { get; set; }

    public string ItemTypesSummary { get; set; }

    public bool AllTypes { get; set; }

    public bool Staff { get => Has("staff"); set => Set("staff", value); }

    public bool Wand { get => Has("wand"); set => Set("wand", value); }

    public bool Focus { get => Has("focus"); set => Set("focus", value); }

    public bool Shield { get => Has("shield"); set => Set("shield", value); }

    public bool Axe { get => Has("axe"); set => Set("axe", value); }

    public bool Bow { get => Has("bow"); set => Set("bow", value); }

    public bool Hammer { get => Has("hammer"); set => Set("hammer", value); }

    public bool Daggers { get => Has("daggers"); set => Set("daggers", value); }

    public bool Scythe { get => Has("scythe"); set => Set("scythe", value); }

    public bool Spear { get => Has("spear"); set => Set("spear", value); }

    public bool Sword { get => Has("sword"); set => Set("sword", value); }

    public bool RunesAndInsignias
    {
        get => Has("runesAndInsignias") || Has("runes") || Has("armor");
        set => Set("runesAndInsignias", value);
    }

    public UpgradeSalvageRule ToModel() =>
        new()
        {
            Enabled = Enabled,
            Group = Group,
            Name = Name,
            ModifierPattern = ModifierPattern,
            SalvageIndex = SalvageIndex,
            ItemTypes = AllTypes ? new List<string>() : itemTypes.Order(StringComparer.OrdinalIgnoreCase).ToList(),
            Action = Enum.TryParse<UpgradeSalvageAction>(Action, ignoreCase: true, out var action)
                ? action
                : UpgradeSalvageAction.Salvage,
            MinimumRarity = Enum.TryParse<ItemRarity>(MinimumRarity, ignoreCase: true, out var rarity)
                ? rarity
                : ItemRarity.Gold
        };

    private bool Has(string itemType) => itemTypes.Contains(itemType);

    private void Set(string itemType, bool enabled)
    {
        if (enabled)
        {
            itemTypes.Add(itemType);
            AllTypes = false;
        }
        else
        {
            itemTypes.Remove(itemType);
        }
    }

    private static string FormatItemType(string itemType) =>
        itemType switch
        {
            "runesAndInsignias" => "Runes & Insignias",
            "focus" => "Focus",
            _ when string.IsNullOrWhiteSpace(itemType) => "",
            _ => CultureInfo.InvariantCulture.TextInfo.ToTitleCase(itemType)
        };
}

public sealed class MaintenanceSettingViewModel
{
    public MaintenanceSettingViewModel(string name, string target, string threshold, bool enabled)
    {
        Name = name;
        Target = target;
        Threshold = threshold;
        Enabled = enabled;
    }

    public string Name { get; set; }

    public string Target { get; set; }

    public string Threshold { get; set; }

    public bool Enabled { get; set; }
}

public sealed class AllowedActionViewModel
{
    public AllowedActionViewModel(string name, bool enabled)
    {
        Name = name;
        Enabled = enabled;
    }

    public string Name { get; set; }

    public bool Enabled { get; set; }
}

public sealed class ChatMessageViewModel
{
    public ChatMessageViewModel(string speaker, string text, bool isOperator)
    {
        Speaker = speaker;
        Text = text;
        IsOperator = isOperator;
        Time = DateTime.Now.ToString("HH:mm:ss", CultureInfo.InvariantCulture);
    }

    public string Time { get; }

    public string Speaker { get; }

    public string Text { get; }

    public bool IsOperator { get; }
}

public sealed class LogEntryViewModel
{
    public LogEntryViewModel(DateTime timestamp, string source, string level, string message)
    {
        Timestamp = timestamp.ToString("HH:mm:ss", CultureInfo.InvariantCulture);
        Source = source;
        Level = level;
        Message = message;
    }

    public string Timestamp { get; }

    public string Source { get; }

    public string Level { get; }

    public string Message { get; }

    public string Line => $"{Timestamp} [{Level}] {Source}: {Message}";
}

public sealed class DiagnosticRowViewModel
{
    public DiagnosticRowViewModel(string name, string value, string status)
    {
        Name = name;
        Value = value;
        Status = status;
    }

    public string Name { get; }

    public string Value { get; }

    public string Status { get; }
}
