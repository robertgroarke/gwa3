namespace Gwa3.UI.Supervisor.Models;

public sealed record LaunchPlan
{
    public string SessionId { get; init; } = Guid.NewGuid().ToString("N");
    public string ProfileName { get; init; } = string.Empty;
    public bool DryRun { get; init; } = true;
    public LaunchMode Mode { get; init; } = LaunchMode.Bot;

    public int AccountIndex { get; init; }
    public string CharacterName { get; init; } = string.Empty;
    public string LaneTag { get; init; } = string.Empty;

    public string AutoItExecutablePath { get; init; } = string.Empty;
    public string LauncherScriptPath { get; init; } = string.Empty;
    public string? LauncherWorkingDirectory { get; init; }

    public string BuildDirectory { get; init; } = string.Empty;
    public string InjectorPath { get; init; } = string.Empty;
    public string DllPath { get; init; } = string.Empty;
    public string DllName { get; init; } = string.Empty;
    public string? BotModule { get; init; }
    public string? ResolvedProfilePath { get; init; }

    public BridgePipeMetadata Bridge { get; init; } = new();
    public HealthGateOptions HealthGate { get; init; } = new();
    public IReadOnlyList<string> LogPaths { get; init; } = Array.Empty<string>();

    public string DisplayName =>
        string.IsNullOrWhiteSpace(ProfileName)
            ? $"{CharacterName} / {LaneTag}"
            : ProfileName;
}
