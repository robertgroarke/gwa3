using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class InMemorySessionStatusSink : ISessionStatusSink
{
    private readonly List<SessionStatusEvent> _events = new();
    private readonly object _gate = new();

    public IReadOnlyList<SessionStatusEvent> Events
    {
        get
        {
            lock (_gate)
            {
                return _events.ToArray();
            }
        }
    }

    public event EventHandler<SessionStatusEvent>? StatusPublished;

    public ValueTask PublishAsync(SessionStatusEvent statusEvent, CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();

        lock (_gate)
        {
            _events.Add(statusEvent);
        }

        StatusPublished?.Invoke(this, statusEvent);
        return ValueTask.CompletedTask;
    }
}
