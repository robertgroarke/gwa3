using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface IInjectorService
{
    ProcessCommand BuildCommand(LaunchPlan plan, int guildWarsProcessId);
    Task<InjectionResult> InjectAsync(LaunchPlan plan, int guildWarsProcessId, CancellationToken cancellationToken = default);
}
