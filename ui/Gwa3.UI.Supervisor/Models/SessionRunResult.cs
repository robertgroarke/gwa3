namespace Gwa3.UI.Supervisor.Models;

public sealed record SessionRunResult
{
    public string SessionId { get; init; } = string.Empty;
    public SessionStage FinalStage { get; init; } = SessionStage.Idle;
    public bool Succeeded { get; init; }
    public bool DryRun { get; init; }
    public int? GuildWarsProcessId { get; init; }
    public LauncherResult? Launcher { get; init; }
    public HealthGateResult? HealthGate { get; init; }
    public InjectionResult? Injection { get; init; }
    public BridgeResult? Bridge { get; init; }
    public string Message { get; init; } = string.Empty;
}
