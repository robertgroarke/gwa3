param([int]$TargetPid, [string]$OutPath)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
if (-not $p) { Write-Host "DEAD"; exit 2 }
$h = $p.MainWindowHandle
if ($h -eq 0) { Write-Host "NO_HWND"; exit 3 }

$sig = @"
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int nCmdShow);
[StructLayout(LayoutKind.Sequential)]
public struct RECT { public int Left, Top, Right, Bottom; }
"@
$t = Add-Type -MemberDefinition $sig -Name W32 -Namespace S -PassThru
[S.W32]::ShowWindow($h, 5) | Out-Null   # SW_SHOW
[S.W32]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 500
$r = New-Object S.W32+RECT
[S.W32]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left
$hh = $r.Bottom - $r.Top
if ($w -le 0 -or $hh -le 0) { Write-Host "BAD_RECT $w x $hh"; exit 4 }
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $hh))
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "SAVED $OutPath ${w}x${hh}"
