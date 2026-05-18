using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface IProcessHealthGate
{
    Task<HealthGateResult> WaitForHealthyAsync(
        LaunchPlan plan,
        int guildWarsProcessId,
        CancellationToken cancellationToken = default);
}
