using System;
using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;

namespace Gwa3.UI.App.Converters;

public sealed class StatusBrushConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return value?.ToString() switch
        {
            "Running" => BrushFrom("#2F855A"),
            "Bot Running" => BrushFrom("#2F855A"),
            "Injected" => BrushFrom("#2F855A"),
            "InTown" => BrushFrom("#2F855A"),
            "Traveling" => BrushFrom("#2B6CB0"),
            "InDungeon" => BrushFrom("#2B6CB0"),
            "Maintenance" => BrushFrom("#B7791F"),
            "Merchant" => BrushFrom("#B7791F"),
            "Launching" => BrushFrom("#2B6CB0"),
            "Health Gate" => BrushFrom("#B7791F"),
            "Injecting" => BrushFrom("#805AD5"),
            "Armed" => BrushFrom("#2C7A7B"),
            "Dry Run" => BrushFrom("#4A5568"),
            "Dry Run OK" => BrushFrom("#4A5568"),
            "Watchdog" => BrushFrom("#C53030"),
            "Error" => BrushFrom("#C53030"),
            "Failed" => BrushFrom("#C53030"),
            "Blocked" => BrushFrom("#C53030"),
            _ => BrushFrom("#718096")
        };
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return Binding.DoNothing;
    }

    private static SolidColorBrush BrushFrom(string color)
    {
        return new SolidColorBrush((Color)ColorConverter.ConvertFromString(color));
    }
}
