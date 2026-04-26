param([int]$TargetPid, [string]$OutPath)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Find all top-level windows belonging to TargetPid and pick the one whose
# title is NOT "gwa3 live log" (that's our DLL overlay).
$sig = @"
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
[DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr hWnd);
[DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
[StructLayout(LayoutKind.Sequential)]
public struct RECT { public int Left, Top, Right, Bottom; }
public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
"@
$t = Add-Type -MemberDefinition $sig -Name W32e -Namespace S2 -PassThru

$found = @()
$enum = [S2.W32e+EnumWindowsProc]{
    param($hWnd, $lParam)
    if (-not [S2.W32e]::IsWindowVisible($hWnd)) { return $true }
    [uint32]$procId = 0
    [S2.W32e]::GetWindowThreadProcessId($hWnd, [ref]$procId) | Out-Null
    if ($procId -ne $TargetPid) { return $true }
    $len = [S2.W32e]::GetWindowTextLength($hWnd)
    $sb = New-Object System.Text.StringBuilder ($len + 1)
    [S2.W32e]::GetWindowText($hWnd, $sb, $sb.Capacity) | Out-Null
    $title = $sb.ToString()
    $r = New-Object S2.W32e+RECT
    [S2.W32e]::GetWindowRect($hWnd, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left
    $h = $r.Bottom - $r.Top
    Write-Host "  hwnd=$hWnd pid=$procId title='$title' size=${w}x${h}"
    $script:found += [PSCustomObject]@{ Handle=$hWnd; Title=$title; W=$w; H=$h; R=$r }
    return $true
}
[S2.W32e]::EnumWindows($enum, [IntPtr]::Zero) | Out-Null

$game = $found | Where-Object { $_.Title -ne 'gwa3 live log' -and $_.W -gt 200 -and $_.H -gt 200 } | Select-Object -First 1
if (-not $game) {
    Write-Host "NO_GAME_WINDOW"
    exit 2
}
Write-Host "Capturing GAME window: hwnd=$($game.Handle) title='$($game.Title)' ${($game.W)}x${($game.H)}"
$bmp = New-Object System.Drawing.Bitmap $game.W, $game.H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($game.R.Left, $game.R.Top, 0, 0, (New-Object System.Drawing.Size $game.W, $game.H))
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "SAVED $OutPath"
