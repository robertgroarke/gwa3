using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface ILauncherService
{
    ProcessCommand BuildCommand(LaunchPlan plan);
    Task<LauncherResult> LaunchAsync(LaunchPlan plan, CancellationToken cancellationToken = default);
}
