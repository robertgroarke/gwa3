using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface ISessionStatusSink
{
    ValueTask PublishAsync(SessionStatusEvent statusEvent, CancellationToken cancellationToken = default);
}
