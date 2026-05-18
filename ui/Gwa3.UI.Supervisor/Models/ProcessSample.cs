namespace Gwa3.UI.Supervisor.Models;

public sealed record ProcessSample
{
    public int ProcessId { get; init; }
    public bool Exists { get; init; }
    public bool HasExited { get; init; }
    public long WorkingSetBytes { get; init; }
    public DateTimeOffset Timestamp { get; init; } = DateTimeOffset.UtcNow;
    public string? ProcessName { get; init; }
    public string? Error { get; init; }
}
