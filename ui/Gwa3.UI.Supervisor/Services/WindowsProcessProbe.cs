using System.Diagnostics;
using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class WindowsProcessProbe : IProcessProbe
{
    public ProcessSample Sample(int processId)
    {
        try
        {
            using var process = Process.GetProcessById(processId);
            return new ProcessSample
            {
                ProcessId = processId,
                Exists = true,
                HasExited = process.HasExited,
                WorkingSetBytes = process.HasExited ? 0 : process.WorkingSet64,
                ProcessName = process.ProcessName
            };
        }
        catch (ArgumentException ex)
        {
            return Missing(processId, ex.Message);
        }
        catch (InvalidOperationException ex)
        {
            return Missing(processId, ex.Message);
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            return new ProcessSample
            {
                ProcessId = processId,
                Exists = true,
                HasExited = false,
                Error = ex.Message
            };
        }
    }

    private static ProcessSample Missing(int processId, string error) =>
        new()
        {
            ProcessId = processId,
            Exists = false,
            HasExited = true,
            Error = error
        };
}
