namespace Gwa3.UI.Supervisor.Models;

public sealed record SessionStatusEvent
{
    public string SessionId { get; init; } = string.Empty;
    public SessionStage Stage { get; init; } = SessionStage.Idle;
    public StatusSeverity Severity { get; init; } = StatusSeverity.Info;
    public string Message { get; init; } = string.Empty;
    public DateTimeOffset Timestamp { get; init; } = DateTimeOffset.UtcNow;
    public IReadOnlyDictionary<string, string> Details { get; init; } =
        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
}
