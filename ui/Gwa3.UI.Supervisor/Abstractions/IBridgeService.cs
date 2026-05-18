using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface IBridgeService
{
    ProcessCommand BuildCommand(LaunchPlan plan);
    Task<BridgeResult> StartAsync(LaunchPlan plan, CancellationToken cancellationToken = default);
}
