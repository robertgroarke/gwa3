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

$script:AutoItPath = "C:\Program Files (x86)\AutoIt3\AutoIt3.exe"
$script:LauncherScriptsDir = Join-Path $script:RepoRoot "..\GWA Censured\debug_scripts"

# Character names by account index (must match Accounts.json order)
$script:CharacterNames = @(
    "B E A S T R I T",
    "D I S C O P A N I C",
    "B L U M P K I N S",
    "Starvin M A R V I N",
    "L I L B I S C U I T"
)

function Launch-GwViaGWLauncher {
    # Use the GWLauncher AutoIt path to launch with multiclient patch.
    # Returns the PID of the launched GW process.
    $charName = $script:CharacterNames[$AccountIndex]
    $launcherScript = Join-Path $script:LauncherScriptsDir "launch_$($charName.ToLower().Replace(' ',''))_via_gwlauncher.au3"

    if (-not (Test-Path $launcherScript)) {
        # No per-character launcher script exists; create one dynamically
        $launcherScript = Join-Path $env:TEMP "gwa3_launch_temp.au3"
        $scriptContent = @"
#RequireAdmin
#include "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\lib\Froggy_Includes.au3"
Global Const `$ACCOUNTS_PATH = "$AccountsPath"
Global Const `$TARGET_CHARACTER = "$charName"
Local `$accounts = GWLauncher_LoadAccounts(`$ACCOUNTS_PATH)
Local `$idx = GWLauncher_FindAccountByCharacter(`$accounts, `$TARGET_CHARACTER)
If `$idx < 0 Then Exit 2
Local `$result = GWLauncher_LaunchAccount(`$accounts, `$idx)
If `$result = 0 Then Exit 3
ConsoleWrite("GWLAUNCHER_PID=" & `$result[0] & @CRLF)
Exit 0
"@
        Set-Content -Path $launcherScript -Value $scriptContent -Encoding UTF8
    }

    # Determine log path — use per-character log that the AutoIt script writes
    $logName = "launch_$($charName.ToLower().Replace(' ',''))_via_gwlauncher.log"
    $launcherLog = Join-Path (Split-Path $launcherScript -Parent) $logName
    if (-not (Test-Path (Split-Path $launcherScript -Parent))) {
        $launcherLog = Join-Path $env:TEMP $logName
    }

    # Clear old log
    Remove-Item $launcherLog -Force -ErrorAction SilentlyContinue

    Write-Host "Launching $charName via GWLauncher: $launcherScript"
    Start-Process -FilePath $script:AutoItPath -ArgumentList "`"$launcherScript`""

    # Poll for the log file to contain the PID (AutoIt is a GUI app — no stdout capture)
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $launcherLog) {
            $content = Get-Content $launcherLog -Raw -ErrorAction SilentlyContinue
            if ($content -and $content -match "GWLAUNCHER_PID=") { break }
        }
        Start-Sleep -Seconds 1
    }

    if (-not (Test-Path $launcherLog)) {
        throw "GWLauncher log not found: $launcherLog"
    }
    $logContent = Get-Content $launcherLog -Raw
    Write-Host "GWLauncher log: $logContent"
    $pidMatch = [regex]::Match($logContent, "GWLAUNCHER_PID=(\d+)")
    if (-not $pidMatch.Success) {
        throw "Could not parse PID from GWLauncher log"
    }
    $gwPid = [int]$pidMatch.Groups[1].Value
    Write-Host "GWLauncher started $charName with PID $gwPid"
    return $gwPid
}

function Wait-ForGwReady {
    param([int]$ProcessId, [int]$TimeoutSeconds)
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
        if ($proc -and $proc.MainWindowHandle -ne 0) {
            Write-Host "GW window ready for PID $ProcessId"
            return $ProcessId
        }
        Start-Sleep -Seconds 1
    }
    throw "Timed out waiting for GW window (PID $ProcessId)"
}

# === Main ===
# Do NOT kill other GW processes — another agent may be running

New-Item -ItemType File -Force -Path $script:FroggyFlagPath | Out-Null

# Launch via GWLauncher (multiclient-safe, returns exact PID)
$gwPid = Launch-GwViaGWLauncher

# Wait for GW window to be ready before injection
Write-Host "Waiting for GW window (PID $gwPid) to be ready..."
Start-Sleep -Seconds $LaunchTimeoutSeconds
Wait-ForGwReady -ProcessId $gwPid -TimeoutSeconds 30

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
    # Check if OUR GW process is still alive (don't check other agents' processes)
    $ourGw = Get-Process -Id $gwPid -ErrorAction SilentlyContinue
    if (-not $ourGw -or $ourGw.HasExited) { break }

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
# Only kill OUR GW process, not other agents' clients
Stop-Process -Id $gwPid -Force -ErrorAction SilentlyContinue

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

# Final parse after process shutdown. The run can complete and terminate GW
# before the live polling loop observes the summary/failure lines.
foreach ($line in $froggyBlock) {
    if ($line -match "FROGGY FEATURE TESTS COMPLETE") {
        $summarySeen = $true
    }
    if ($line -match "Froggy feature test complete: .* failures") {
        $failureLine = $line
    }
}

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
