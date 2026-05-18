using System.Diagnostics;
using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class ProcessRunner : IProcessRunner
{
    private readonly bool _allowLiveProcessExecution;

    public ProcessRunner(bool allowLiveProcessExecution = false)
    {
        _allowLiveProcessExecution = allowLiveProcessExecution;
    }

    public async Task<ProcessCommandResult> RunAsync(ProcessCommand command, CancellationToken cancellationToken = default)
    {
        if (!_allowLiveProcessExecution)
        {
            return ProcessCommandResult.DryRunSuccess(command);
        }

        var startInfo = new ProcessStartInfo
        {
            FileName = command.FileName,
            WorkingDirectory = command.WorkingDirectory ?? string.Empty,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };

        foreach (var argument in command.Arguments)
        {
            startInfo.ArgumentList.Add(argument);
        }

        foreach (var (key, value) in command.Environment)
        {
            startInfo.Environment[key] = value;
        }

        using var process = Process.Start(startInfo);
        if (process is null)
        {
            return new ProcessCommandResult
            {
                Succeeded = false,
                Command = command,
                StandardError = "Process.Start returned null."
            };
        }

        var stdoutTask = process.StandardOutput.ReadToEndAsync(cancellationToken);
        var stderrTask = process.StandardError.ReadToEndAsync(cancellationToken);
        await process.WaitForExitAsync(cancellationToken).ConfigureAwait(false);

        return new ProcessCommandResult
        {
            Succeeded = process.ExitCode == 0,
            ExitCode = process.ExitCode,
            ProcessId = process.Id,
            StandardOutput = await stdoutTask.ConfigureAwait(false),
            StandardError = await stderrTask.ConfigureAwait(false),
            Command = command
        };
    }

    public Task<ProcessCommandResult> StartAsync(ProcessCommand command, CancellationToken cancellationToken = default)
    {
        if (!_allowLiveProcessExecution)
        {
            return Task.FromResult(ProcessCommandResult.DryRunSuccess(command));
        }

        cancellationToken.ThrowIfCancellationRequested();

        var startInfo = new ProcessStartInfo
        {
            FileName = command.FileName,
            WorkingDirectory = command.WorkingDirectory ?? string.Empty,
            UseShellExecute = false,
            RedirectStandardOutput = false,
            RedirectStandardError = false
        };

        foreach (var argument in command.Arguments)
        {
            startInfo.ArgumentList.Add(argument);
        }

        foreach (var (key, value) in command.Environment)
        {
            startInfo.Environment[key] = value;
        }

        using var process = Process.Start(startInfo);
        if (process is null)
        {
            return Task.FromResult(new ProcessCommandResult
            {
                Succeeded = false,
                Command = command,
                StandardError = "Process.Start returned null."
            });
        }

        return Task.FromResult(new ProcessCommandResult
        {
            Succeeded = true,
            ProcessId = process.Id,
            Command = command
        });
    }
}
