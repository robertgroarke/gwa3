using System.Diagnostics;
using Gwa3.UI.Supervisor.Abstractions;

namespace Gwa3.UI.Supervisor.Services;

public sealed class ProcessTerminator : IProcessTerminator
{
    public async Task TerminateAsync(int processId, CancellationToken cancellationToken = default)
    {
        if (processId <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(processId), "A positive process ID is required.");
        }

        using var process = Process.GetProcessById(processId);
        if (process.HasExited)
        {
            return;
        }

        process.Kill(entireProcessTree: true);
        await process.WaitForExitAsync(cancellationToken).ConfigureAwait(false);
    }
}
