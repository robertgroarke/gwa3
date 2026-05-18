using System.Runtime.CompilerServices;
using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class FileLogTailService : ILogTailService
{
    private static readonly TimeSpan PollInterval = TimeSpan.FromMilliseconds(500);

    public async IAsyncEnumerable<LogLine> TailAsync(
        string path,
        string source,
        bool readFromEnd = true,
        [EnumeratorCancellation] CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);

        var position = 0L;
        while (!File.Exists(path) && !cancellationToken.IsCancellationRequested)
        {
            await Task.Delay(PollInterval, cancellationToken).ConfigureAwait(false);
        }

        if (readFromEnd && File.Exists(path))
        {
            position = new FileInfo(path).Length;
        }

        while (!cancellationToken.IsCancellationRequested)
        {
            if (!File.Exists(path))
            {
                position = 0;
                await Task.Delay(PollInterval, cancellationToken).ConfigureAwait(false);
                continue;
            }

            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            if (stream.Length < position)
            {
                position = 0;
            }

            stream.Seek(position, SeekOrigin.Begin);
            using var reader = new StreamReader(stream, leaveOpen: true);
            while (!reader.EndOfStream)
            {
                var line = await reader.ReadLineAsync(cancellationToken).ConfigureAwait(false);
                if (line is not null)
                {
                    yield return new LogLine
                    {
                        Source = source,
                        Path = path,
                        Text = line
                    };
                }
            }

            position = stream.Position;
            await Task.Delay(PollInterval, cancellationToken).ConfigureAwait(false);
        }
    }
}
