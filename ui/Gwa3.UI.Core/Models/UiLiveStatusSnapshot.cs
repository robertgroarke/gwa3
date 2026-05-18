using System.Text.Json.Serialization;

namespace Gwa3.UI.Core.Models;

public sealed record UiLiveStatusSnapshot
{
    public DateTimeOffset Timestamp { get; init; }

    [JsonPropertyName("event")]
    public string EventName { get; init; } = "";

    public string Profile { get; init; } = "";
    public string Character { get; init; } = "";
    public string Lane { get; init; } = "";
    public string Bot { get; init; } = "";
    public string Status { get; init; } = "";
    public string BotPhase { get; init; } = "";
    public string Pid { get; init; } = "";
    public string Health { get; init; } = "";
    public string LastEvent { get; init; } = "";
    public string RunCount { get; init; } = "";
    public string SuccessCount { get; init; } = "";
    public string FailureCount { get; init; } = "";
    public string FailureRatio { get; init; } = "";
    public string Gold { get; init; } = "";
    public string Heartbeat { get; init; } = "";
    public DateTimeOffset? RuntimeStartedAt { get; init; }
    public DateTimeOffset? CurrentRunStartedAt { get; init; }
    public string TotalRuntime { get; init; } = "";
    public string CurrentRunTime { get; init; } = "";
    public string BestRunTime { get; init; } = "";
    public string AverageRunTime { get; init; } = "";
    public string DungeonLevel { get; init; } = "";
    public int CompletionPercent { get; init; }
    public string CompletionText { get; init; } = "";
    public string ProgressDetail { get; init; } = "";
    public string DeldrimorPoints { get; init; } = "";
    public string AsuraPoints { get; init; } = "";
    public string NornPoints { get; init; } = "";
    public string VanguardPoints { get; init; } = "";
    public string Lockpicks { get; init; } = "";
    public string Wipes { get; init; } = "";
    public string RareSkins { get; init; } = "";
    public string GoldItems { get; init; } = "";
    public string DroppedLockpicks { get; init; } = "";
    public string ChestsOpened { get; init; } = "";
    public string BlackDyes { get; init; } = "";
    public string Tomes { get; init; } = "";
    public string BotshubIpcStatus { get; init; } = "";
    public string BotshubScript { get; init; } = "";
    public string BotshubPid { get; init; } = "";
    public string BotshubState { get; init; } = "";
    public string BotshubMapId { get; init; } = "";
    public string BotshubRunning { get; init; } = "";
    public string BotshubUptime { get; init; } = "";
    public string BotshubGold { get; init; } = "";
    public string BotshubSettings { get; init; } = "";
    public string BotshubRunStats { get; init; } = "";
    public string BotshubRuns { get; init; } = "";
    public string BotshubSuccesses { get; init; } = "";
    public string BotshubFailures { get; init; } = "";
    public string BotshubSuccessRatio { get; init; } = "";
    public string BotshubCurrentRunTime { get; init; } = "";
    public string BotshubTotalTime { get; init; } = "";
    public string BotshubBestRunTime { get; init; } = "";
    public string BotshubAverageRunTime { get; init; } = "";
    public string BotshubTimePerRun { get; init; } = "";
    public string BotshubExperience { get; init; } = "";
    public string BotshubChests { get; init; } = "";
    public string BotshubGoldItems { get; init; } = "";
    public string BotshubTitleSummary { get; init; } = "";
    public string BotshubLootSummary { get; init; } = "";
    public string BotshubMaterialSummary { get; init; } = "";
    public string BotshubInventorySummary { get; init; } = "";
    public string BotshubMaintenanceSummary { get; init; } = "";
    public string BotshubStatusSource { get; init; } = "";
    public string BotshubCommandQueue { get; init; } = "";
    public string BotshubLastLog { get; init; } = "";
    public string Py4GwPreviousStep { get; init; } = "";
    public string Py4GwCurrentStep { get; init; } = "";
    public string Py4GwNextStep { get; init; } = "";
    public string Py4GwStateMachine { get; init; } = "";
    public string Py4GwMap { get; init; } = "";
    public string Py4GwHealth { get; init; } = "";
    public string Py4GwPosition { get; init; } = "";
    public string Py4GwTarget { get; init; } = "";
    public string Py4GwPathing { get; init; } = "";
    public string Py4GwCasting { get; init; } = "";
    public string Py4GwSkillbar { get; init; } = "";
    public string Py4GwActionQueue { get; init; } = "";
    public string Py4GwOverwatch { get; init; } = "";
    public string Py4GwLastUpdate { get; init; } = "";
    public string Detail { get; init; } = "";
}
