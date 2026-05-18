using System;
using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;

namespace Gwa3.UI.App.Converters;

public sealed class LogLevelBrushConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return value?.ToString()?.ToUpperInvariant() switch
        {
            "WARN" => BrushFrom("#B7791F"),
            "ERROR" => BrushFrom("#C53030"),
            "DEBUG" => BrushFrom("#4A5568"),
            "ACTION" => BrushFrom("#2B6CB0"),
            _ => BrushFrom("#2F855A")
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
