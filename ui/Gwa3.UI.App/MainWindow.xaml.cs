using System;
using System.Windows;
using Gwa3.UI.App.ViewModels;

namespace Gwa3.UI.App;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        var viewModel = MainWindowViewModel.CreateShellPreview();
        DataContext = viewModel;

        Loaded += (_, _) =>
        {
            if (string.Equals(
                    Environment.GetEnvironmentVariable("GWA3_UI_AUTO_LAUNCH"),
                    "1",
                    StringComparison.OrdinalIgnoreCase))
            {
                viewModel.LaunchCommand.Execute(null);
            }
        };
    }
}
