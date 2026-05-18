namespace Gwa3.UI.Supervisor.Models;

public sealed record LogLine
{
    public string Source { get; init; } = string.Empty;
    public string Path { get; init; } = string.Empty;
    public string Text { get; init; } = string.Empty;
    public DateTimeOffset Timestamp { get; init; } = DateTimeOffset.UtcNow;
}
