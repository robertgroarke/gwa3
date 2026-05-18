using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Abstractions;

public interface IProcessProbe
{
    ProcessSample Sample(int processId);
}
