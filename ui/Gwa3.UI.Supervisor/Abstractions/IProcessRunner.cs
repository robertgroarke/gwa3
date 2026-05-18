using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface IProcessRunner
{
    Task<ProcessCommandResult> RunAsync(ProcessCommand command, CancellationToken cancellationToken = default);
    Task<ProcessCommandResult> StartAsync(ProcessCommand command, CancellationToken cancellationToken = default);
}
