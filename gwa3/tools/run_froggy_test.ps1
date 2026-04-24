param(
    [int]$AccountIndex = 0,
    [int]$LaunchTimeoutSeconds = 60,
    [int]$RunTimeoutSeconds = 600,
    [switch]$SparkflyOnly,
    [string]$AccountsPath = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json",
    [string]$BuildDir = $env:GWA3_BUILD_DIR,
    [string]$DllName = $env:GWA3_DLL_NAME
)

$ErrorActionPreference = "Stop"

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = if ($BuildDir) { $BuildDir } else { Join-Path $script:RepoRoot "build" }
$resolvedDllName = if ($DllName) { $DllName } else { "gwa3.dll" }
$script:BinDir = Join-Path $resolvedBuildDir "bin\Release"
$script:InjectorPath = Join-Path $script:BinDir "injector.exe"
$script:DllPath = Join-Path $script:BinDir $resolvedDllName
$script:LogPath = Join-Path $script:BinDir "gwa3_log.txt"
$script:FroggyFlagPath = Join-Path $script:BinDir "gwa3_test_froggy.flag"
$script:FroggySparkflyFlagPath = Join-Path $script:BinDir "gwa3_test_froggy_sparkfly.flag"
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
    param([uint32]$TargetPid = 0)
    $found = $false
    $callback = [GwWindowProbeFroggy+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        if (-not [GwWindowProbeFroggy]::IsWindowVisible($hWnd)) { return $true }
        $cls = New-Object System.Text.StringBuilder 256
        [void][GwWindowProbeFroggy]::GetClassName($hWnd, $cls, $cls.Capacity)
        $ttl = New-Object System.Text.StringBuilder 512
        [void][GwWindowProbeFroggy]::GetWindowText($hWnd, $ttl, $ttl.Capacity)
        if ($cls.ToString() -eq "#32770" -and $ttl.ToString() -eq "Gw.exe") {
            $windowPid = 0
            [void][GwWindowProbeFroggy]::GetWindowThreadProcessId($hWnd, [ref]$windowPid)
            if ($TargetPid -eq 0 -or $windowPid -eq $TargetPid) {
                $script:found = $true
            }
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
        if ($lines[$i] -match "FROGGY FEATURE TEST MODE" -or $lines[$i] -match "FROGGY SPARKFLY ROUTE TEST MODE") { $start = $i }
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

function Get-CharacterGwProcesses {
    param([string]$CharacterName)
    $rows = Get-CimInstance Win32_Process -Filter "name='Gw.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine -match [regex]::Escape($CharacterName) }
    if (-not $rows) { return @() }
    return @($rows | Select-Object ProcessId, CommandLine)
}

function Stop-CharacterGwProcesses {
    param([string]$CharacterName)
    foreach ($row in (Get-CharacterGwProcesses -CharacterName $CharacterName)) {
        $candidatePid = [int]$row.ProcessId
        if ($candidatePid -le 0) { continue }
        Stop-Process -Id $candidatePid -Force -ErrorAction SilentlyContinue
    }
}

function Get-PidMemoryKb {
    param([int]$ProcessId)
    if ($ProcessId -le 0) { return 0 }
    $line = tasklist /FI "PID eq $ProcessId" /FO CSV /NH 2>$null
    if (-not $line -or $line -match "No tasks are running") { return 0 }
    $row = $line | ConvertFrom-Csv -Header ImageName,PID,SessionName,SessionNum,MemUsage | Select-Object -First 1
    if (-not $row) { return 0 }
    $mem = (($row.MemUsage -replace ',', '') -replace '\s*K', '') -replace '\s+', ''
    $value = 0
    [void][int]::TryParse($mem, [ref]$value)
    return $value
}

function Resolve-LiveGwPid {
    param(
        [string]$CharacterName,
        [int]$LauncherPidHint,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $healthyThresholdKb = 100000
    while ((Get-Date) -lt $deadline) {
        $candidates = @()
        foreach ($row in (Get-CharacterGwProcesses -CharacterName $CharacterName)) {
            $candidatePid = [int]$row.ProcessId
            if ($candidatePid -le 0) { continue }
            $proc = Get-Process -Id $candidatePid -ErrorAction SilentlyContinue
            if (-not $proc -or $proc.MainWindowHandle -eq 0) { continue }
            $memKb = Get-PidMemoryKb -ProcessId $candidatePid
            if ($memKb -lt $healthyThresholdKb) { continue }
            $candidates += [pscustomobject]@{
                ProcessId = $candidatePid
                MemoryKb = $memKb
            }
        }
        if ($candidates.Count -gt 0) {
            $best = $candidates | Sort-Object MemoryKb, ProcessId | Select-Object -Last 1
            Write-Host "Resolved live GW PID for ${CharacterName}: $($best.ProcessId) (launcher hint $LauncherPidHint, mem $($best.MemoryKb) KB)"
            return [int]$best.ProcessId
        }
        Start-Sleep -Milliseconds 500
    }

    throw "Timed out resolving live GW PID for $CharacterName (launcher hint $LauncherPidHint)"
}

function Dump-GwProcessSnapshot {
    param([string]$Label)
    Write-Host "GW_PROCESS_SNAPSHOT: $Label"
    try {
        $rows = Get-CimInstance Win32_Process -Filter "name='Gw.exe'" -ErrorAction Stop |
            Select-Object ProcessId, CommandLine
        if (-not $rows) {
            Write-Host "GW_PROCESS_SNAPSHOT: none"
            return
        }
        foreach ($row in $rows) {
            Write-Host ("GW_PROCESS_SNAPSHOT: pid={0} cmd={1}" -f $row.ProcessId, $row.CommandLine)
        }
    } catch {
        Write-Host ("GW_PROCESS_SNAPSHOT: failed to enumerate Gw.exe ({0})" -f $_.Exception.Message)
    }
}

# === Main ===
# Do NOT kill other GW processes — another agent may be running

if ($SparkflyOnly) {
    New-Item -ItemType File -Force -Path $script:FroggySparkflyFlagPath | Out-Null
} else {
    New-Item -ItemType File -Force -Path $script:FroggyFlagPath | Out-Null
}

# Only clean up the MARVIN character lane before launch; do not touch other agents.
Stop-CharacterGwProcesses -CharacterName $script:CharacterNames[$AccountIndex]
Start-Sleep -Seconds 2

# Launch via GWLauncher (multiclient-safe, returns exact PID)
$gwPid = Launch-GwViaGWLauncher

# Resolve the actual live client PID before injection. GWLauncher can return
# a short-lived bootstrap PID rather than the lasting windowed Gw.exe.
Write-Host "Waiting for live GW process for $($script:CharacterNames[$AccountIndex])..."
Start-Sleep -Seconds $LaunchTimeoutSeconds
$gwPid = Resolve-LiveGwPid -CharacterName $script:CharacterNames[$AccountIndex] -LauncherPidHint $gwPid -TimeoutSeconds 45

# Update log path to PID-specific file (avoids contention with other agents)
$script:LogPath = Join-Path $script:BinDir "gwa3_log_$gwPid.txt"
if (Test-Path $script:LogPath) {
    Remove-Item -LiteralPath $script:LogPath -Force -ErrorAction SilentlyContinue
}
Write-Host "Log path: $($script:LogPath)"

$dllName = Split-Path $script:DllPath -Leaf
if ($SparkflyOnly) {
    Write-Host "Injecting Froggy Sparkfly test into PID $gwPid (DLL: $dllName)..."
    & $script:InjectorPath --pid $gwPid --dll $dllName --test-froggy-sparkfly
} else {
    Write-Host "Injecting froggy test into PID $gwPid (DLL: $dllName)..."
    & $script:InjectorPath --pid $gwPid --dll $dllName --test-froggy
}
if ($LASTEXITCODE -ne 0) { throw "Injection failed." }

$crashDialogSeen = $false
$summarySeen = $false
$summaryLine = $null
$failureLine = $null
$summarySeenAt = $null
$merchantShot = $null
$startTime = Get-Date
$loopExitReason = "timeout"
$loopExitElapsed = 0

while (((Get-Date) - $startTime).TotalSeconds -lt $RunTimeoutSeconds) {
    if (Get-GwCrashDialog -TargetPid $gwPid) {
        $crashDialogSeen = $true
        $loopExitReason = "visible-crash-dialog"
        $loopExitElapsed = [int](((Get-Date) - $startTime).TotalSeconds)
        $shot = Capture-RunScreenshot -Tag "crash_dialog"
        Write-Host "CRASH_DIALOG_DETECTED: $shot"
        break
    }
    # Check if OUR GW process is still alive (don't check other agents' processes)
    $ourGw = Get-Process -Id $gwPid -ErrorAction SilentlyContinue
    if (-not $ourGw -or $ourGw.HasExited) {
        $loopExitReason = "gw-process-exited"
        $loopExitElapsed = [int](((Get-Date) - $startTime).TotalSeconds)
        Dump-GwProcessSnapshot -Label "tracked pid $gwPid exited"
        break
    }

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
            if ($line -match "\[WATCHDOG\].*CRASH DIALOG" -or $line -match "\[WATCHDOG\].*WINDOW NOT RESPONDING" -or $line -match "\[WATCHDOG\].*RENDER FROZEN") {
                $crashDialogSeen = $true
            }
            if ($line -match "FROGGY FEATURE TESTS COMPLETE") {
                $summarySeen = $true
                if (-not $summarySeenAt) { $summarySeenAt = Get-Date }
            }
            if ($line -match "Froggy feature test complete: .* failures" -or $line -match "Froggy Sparkfly route test complete: .* failures") {
                $failureLine = $line
            }
        }
        if ($summarySeen -and $failureLine -and $summarySeenAt -and (((Get-Date) - $summarySeenAt).TotalSeconds -ge 5)) {
            $loopExitReason = "summary-observed"
            $loopExitElapsed = [int](((Get-Date) - $startTime).TotalSeconds)
            break
        }
    }
    Start-Sleep -Seconds 1
}

if ($loopExitElapsed -eq 0) {
    $loopExitElapsed = [int](((Get-Date) - $startTime).TotalSeconds)
}

Write-Host ("RUN_LOOP_EXIT: reason={0} elapsed={1}s pid={2}" -f $loopExitReason, $loopExitElapsed, $gwPid)

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
    Write-Host "No Froggy test block found in log."
    exit 2
}

$froggyBlock | ForEach-Object { $_ }

# Final parse after process shutdown. The run can complete and terminate GW
# before the live polling loop observes the summary/failure lines.
foreach ($line in $froggyBlock) {
    if ($line -match "\[WATCHDOG\].*CRASH DIALOG" -or $line -match "\[WATCHDOG\].*WINDOW NOT RESPONDING" -or $line -match "\[WATCHDOG\].*RENDER FROZEN") {
        $crashDialogSeen = $true
    }
    if ($line -match "FROGGY FEATURE TESTS COMPLETE" -or $line -match "FROGGY SPARKFLY TEST COMPLETE") {
        $summarySeen = $true
    }
    if ($line -match "Froggy feature test complete: .* failures" -or $line -match "Froggy Sparkfly route test complete: .* failures") {
        $failureLine = $line
    }
}

if (-not $crashDialogSeen) {
    $latestBlock = Get-LatestFroggyBlock
    foreach ($line in $latestBlock) {
        if ($line -match "\[WATCHDOG\].*(CRASH DIALOG|WINDOW NOT RESPONDING|RENDER FROZEN)") {
            $crashDialogSeen = $true
            break
        }
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

if ($failureLine -and $failureLine -match "(?:Froggy feature test complete|Froggy Sparkfly route test complete): (\d+) failures") {
    $failures = [int]$Matches[1]
    exit $failures
}

exit 0
