namespace Gwa3.UI.Supervisor.Models;

public sealed record LauncherResult
{
    public bool Succeeded { get; init; }
    public bool DryRun { get; init; }
    public int? GuildWarsProcessId { get; init; }
    public ProcessCommandResult Process { get; init; } = new();
    public string Message { get; init; } = string.Empty;

    public static LauncherResult Failed(string message, ProcessCommandResult? process = null) =>
        new()
        {
            Succeeded = false,
            Message = message,
            Process = process ?? new ProcessCommandResult()
        };
}
