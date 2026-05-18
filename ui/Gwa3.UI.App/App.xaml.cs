using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;

namespace Gwa3.UI.App;

public partial class App : Application
{
    public App()
    {
        RenderOptions.ProcessRenderMode = RenderMode.SoftwareOnly;
    }
}
