namespace Gwa3.UI.Core.Models;

public sealed class LaunchSettings
{
    public LaunchMode LaunchMode { get; set; } = LaunchMode.Bot;

    public string LaneTag { get; set; } = "";

    public string BuildDirectory { get; set; } = "";

    public string DllName { get; set; } = "";

    public string PipeName { get; set; } = "";

    public string LauncherScriptPath { get; set; } = "";

    public string InjectorPath { get; set; } = "";

    public bool FreshClientRequired { get; set; } = true;

    public bool PassProfilePathToInjector { get; set; } = true;

    public HealthGateSettings HealthGate { get; set; } = new();

    public List<string> ExtraInjectorArguments { get; set; } = new();
}

public sealed class HealthGateSettings
{
    public int TimeoutSeconds { get; set; } = 30;

    public int PollIntervalMilliseconds { get; set; } = 500;

    public long RequiredWorkingSetKilobytes { get; set; } = 100_000;

    public int PostHealthySettleMilliseconds { get; set; } = 8000;
}

public enum LaunchMode
{
    Bot,
    Llm,
    Advisory
}
