namespace Gwa3.UI.Supervisor.Models;

public sealed record ProcessCommandResult
{
    public bool DryRun { get; init; }
    public bool Succeeded { get; init; }
    public int? ExitCode { get; init; }
    public int? ProcessId { get; init; }
    public string StandardOutput { get; init; } = string.Empty;
    public string StandardError { get; init; } = string.Empty;
    public ProcessCommand? Command { get; init; }

    public static ProcessCommandResult DryRunSuccess(ProcessCommand command, int? simulatedProcessId = null) =>
        new()
        {
            DryRun = true,
            Succeeded = true,
            ProcessId = simulatedProcessId,
            Command = command
        };
}
