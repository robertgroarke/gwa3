namespace Gwa3.UI.Supervisor.Models;

public sealed record BridgeResult
{
    public bool Enabled { get; init; }
    public bool Succeeded { get; init; }
    public bool DryRun { get; init; }
    public ProcessCommandResult Process { get; init; } = new();
    public string Message { get; init; } = string.Empty;

    public static BridgeResult Disabled(string message = "Bridge is not required for this launch mode.") =>
        new()
        {
            Enabled = false,
            Succeeded = true,
            Message = message
        };

    public static BridgeResult Failed(string message, ProcessCommandResult? process = null) =>
        new()
        {
            Enabled = true,
            Succeeded = false,
            Message = message,
            Process = process ?? new ProcessCommandResult()
        };
}
