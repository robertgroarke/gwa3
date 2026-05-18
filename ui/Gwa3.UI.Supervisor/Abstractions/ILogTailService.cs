using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface ILogTailService
{
    IAsyncEnumerable<LogLine> TailAsync(
        string path,
        string source,
        bool readFromEnd = true,
        CancellationToken cancellationToken = default);
}
