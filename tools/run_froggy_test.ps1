param(
    [int]$AccountIndex = 0,
    [int]$LaunchTimeoutSeconds = 45,
    [int]$RunTimeoutSeconds = 180,
    [string]$AccountsPath = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
)

$ErrorActionPreference = "Stop"

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:BinDir = Join-Path $script:RepoRoot "build\bin\Release"
$script:InjectorPath = Join-Path $script:BinDir "injector.exe"
$script:DllPath = Join-Path $script:BinDir "gwa3.dll"
$script:LogPath = Join-Path $script:BinDir "gwa3_log.txt"
$script:FroggyFlagPath = Join-Path $script:BinDir "gwa3_test_froggy.flag"
$script:ScreenshotDir = Join-Path $script:BinDir "screenshots"
$script:CaptureScript = Join-Path $PSScriptRoot "capture_screen.ps1"

Add-Type -AssemblyName System.Windows.Forms | Out-Null
Add-Type -AssemblyName System.Drawing | Out-Null

$user32 = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class GwWindowProbeFroggy {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")] public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
}
"@
Add-Type -TypeDefinition $user32 | Out-Null

function Get-GwCrashDialog {
    $found = $false
    $callback = [GwWindowProbeFroggy+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        if (-not [GwWindowProbeFroggy]::IsWindowVisible($hWnd)) { return $true }
        $cls = New-Object System.Text.StringBuilder 256
        [void][GwWindowProbeFroggy]::GetClassName($hWnd, $cls, $cls.Capacity)
        $ttl = New-Object System.Text.StringBuilder 512
        [void][GwWindowProbeFroggy]::GetWindowText($hWnd, $ttl, $ttl.Capacity)
        if ($cls.ToString() -eq "#32770" -and $ttl.ToString() -eq "Gw.exe") {
            $script:found = $true
        }
        return $true
    }
    [void][GwWindowProbeFroggy]::EnumWindows($callback, [IntPtr]::Zero)
    return $script:found
}

function Capture-RunScreenshot {
    param([string]$Tag)
    New-Item -ItemType Directory -Force -Path $script:ScreenshotDir | Out-Null
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $path = Join-Path $script:ScreenshotDir ("froggy_{0}_{1}.png" -f $Tag, $stamp)
    & $script:CaptureScript -OutputPath $path | Out-Null
    return $path
}

function Get-LatestFroggyBlock {
    if (-not (Test-Path $script:LogPath)) { return @() }
    try { $lines = Get-Content $script:LogPath -ErrorAction Stop } catch { return @() }
    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match "FROGGY FEATURE TEST MODE") { $start = $i }
    }
    if ($start -lt 0) { return @() }
    return $lines[$start..($lines.Count - 1)]
}

function Launch-GwAccount {
    if (-not (Test-Path $AccountsPath)) { throw "Accounts file not found: $AccountsPath" }
    $accounts = Get-Content $AccountsPath | ConvertFrom-Json
    if ($AccountIndex -lt 0 -or $AccountIndex -ge $accounts.Count) {
        throw "AccountIndex $AccountIndex out of range"
    }
    $acct = $accounts[$AccountIndex]
    $gwPath = [string]$acct.gwpath
    New-Item -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Force | Out-Null
    Set-ItemProperty -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Name Path -Value $gwPath
    Set-ItemProperty -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Name Src -Value (Split-Path $gwPath -Parent)
    $argString = '-email "{0}" -password "{1}" -character "{2}" {3}' -f $acct.email, $acct.password, $acct.character, $acct.extraargs
    Start-Process -FilePath $gwPath -ArgumentList $argString | Out-Null
    Write-Host "GW launched for character: $($acct.character)"
}

function Wait-ForGwWindow {
    param([int]$TimeoutSeconds)
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $listing = & $script:InjectorPath --list 2>&1
        if ($LASTEXITCODE -eq 0 -and ($listing -match "PID")) {
            $gw = Get-Process Gw -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($gw) { return $gw.Id }
        }
        Start-Sleep -Seconds 1
    }
    throw "Timed out waiting for Guild Wars window."
}

# === Main ===
Stop-Process -Name "Gw" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

New-Item -ItemType File -Force -Path $script:FroggyFlagPath | Out-Null

Launch-GwAccount
$gwPid = Wait-ForGwWindow -TimeoutSeconds $LaunchTimeoutSeconds

Write-Host "Injecting froggy test into PID $gwPid..."
& $script:InjectorPath --pid $gwPid --test-froggy
if ($LASTEXITCODE -ne 0) { throw "Injection failed." }

$crashDialogSeen = $false
$summarySeen = $false
$summaryLine = $null
$failureLine = $null
$summarySeenAt = $null
$merchantShot = $null
$startTime = Get-Date

while (((Get-Date) - $startTime).TotalSeconds -lt $RunTimeoutSeconds) {
    if (Get-GwCrashDialog) {
        $crashDialogSeen = $true
        $shot = Capture-RunScreenshot -Tag "crash_dialog"
        Write-Host "CRASH_DIALOG_DETECTED: $shot"
        break
    }
    if (-not (Get-Process Gw -ErrorAction SilentlyContinue)) { break }

    if (-not $merchantShot -and (Test-Path $script:LogPath)) {
        $merchantMarkerSeen = Select-String -Path $script:LogPath -Pattern "MERCHANT_SCREENSHOT_NOW" -Quiet -ErrorAction SilentlyContinue
        if ($merchantMarkerSeen) {
            $merchantShot = Capture-RunScreenshot -Tag "merchant_open"
            Write-Host "MERCHANT_SCREENSHOT: $merchantShot"
        }
    }

    if (Test-Path $script:LogPath) {
        $froggyLines = Get-LatestFroggyBlock
        foreach ($line in $froggyLines) {
            if ($line -match "FROGGY FEATURE TESTS COMPLETE") {
                $summarySeen = $true
                if (-not $summarySeenAt) { $summarySeenAt = Get-Date }
            }
            if ($line -match "Froggy feature test complete: .* failures") {
                $failureLine = $line
            }
        }
        if ($summarySeen -and $failureLine -and $summarySeenAt -and (((Get-Date) - $summarySeenAt).TotalSeconds -ge 5)) {
            break
        }
    }
    Start-Sleep -Seconds 1
}

$endShot = Capture-RunScreenshot -Tag "end"
Write-Host "END_SCREENSHOT: $endShot"
Stop-Process -Name Gw -Force -ErrorAction SilentlyContinue

$froggyBlock = @()
for ($i = 0; $i -lt 5; $i++) {
    $froggyBlock = Get-LatestFroggyBlock
    if ($froggyBlock.Count -gt 0) { break }
    Start-Sleep -Seconds 1
}

if ($froggyBlock.Count -eq 0) {
    Write-Host "No froggy block found in log."
    exit 2
}

$froggyBlock | ForEach-Object { $_ }

if ($crashDialogSeen) {
    Write-Error "Froggy run failed: visible Gw.exe crash dialog detected."
    exit 10
}

if (-not $summarySeen) {
    Write-Error "Froggy run failed: no summary line found."
    exit 12
}

if ($failureLine -and $failureLine -match "Froggy feature test complete: (\d+) failures") {
    $failures = [int]$Matches[1]
    exit $failures
}

exit 0
