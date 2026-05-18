namespace Gwa3.UI.Supervisor.Models;

public sealed record HealthGateOptions
{
    public TimeSpan Timeout { get; init; } = TimeSpan.FromSeconds(30);
    public TimeSpan PollInterval { get; init; } = TimeSpan.FromMilliseconds(500);
    public long MinimumWorkingSetBytes { get; init; } = 100_000L * 1024L;
    public TimeSpan PostHealthySettle { get; init; } = TimeSpan.FromSeconds(8);
    public bool TreatDryRunAsHealthy { get; init; } = true;
}
