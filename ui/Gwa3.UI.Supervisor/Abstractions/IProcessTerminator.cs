namespace Gwa3.UI.Supervisor.Abstractions;

public interface IProcessTerminator
{
    Task TerminateAsync(int processId, CancellationToken cancellationToken = default);
}
