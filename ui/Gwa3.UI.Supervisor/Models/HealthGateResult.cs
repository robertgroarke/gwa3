namespace Gwa3.UI.Supervisor.Models;

public sealed record HealthGateResult
{
    public bool Succeeded { get; init; }
    public bool DryRun { get; init; }
    public int ProcessId { get; init; }
    public long RequiredWorkingSetBytes { get; init; }
    public ProcessSample? LastSample { get; init; }
    public string Message { get; init; } = string.Empty;

    public static HealthGateResult Failed(int processId, long requiredWorkingSetBytes, string message, ProcessSample? sample = null) =>
        new()
        {
            Succeeded = false,
            ProcessId = processId,
            RequiredWorkingSetBytes = requiredWorkingSetBytes,
            Message = message,
            LastSample = sample
        };
}
