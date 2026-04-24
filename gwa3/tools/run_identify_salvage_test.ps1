param(
    [int]$AccountIndex = 3,
    [int]$LaunchTimeoutSeconds = 60,
    [int]$RunTimeoutSeconds = 240,
    [ValidateSet("full", "identify-only", "salvage-open-only", "single-salvage", "native-salvage", "native-salvage-enter", "legacy-botshub-start-only", "legacy-botshub-salvage", "legacy-botshub-salvage-enter", "legacy-botshub-salvage-done", "legacy-botshub-salvage-cancel", "legacy-botshub-tracked-chain")]
    [string]$Stage = "full",
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
$script:ScreenshotDir = Join-Path $script:BinDir "screenshots"
$script:CaptureScript = Join-Path $PSScriptRoot "capture_screen.ps1"
$script:AutoItPath = "C:\Program Files (x86)\AutoIt3\AutoIt3.exe"
$script:LauncherScriptsDir = Join-Path $script:RepoRoot "..\GWA Censured\debug_scripts"
$script:CharacterNames = @(
    "B E A S T R I T",
    "D I S C O P A N I C",
    "B L U M P K I N S",
    "Starvin M A R V I N",
    "L I L B I S C U I T"
)

Add-Type -AssemblyName System.Windows.Forms | Out-Null
Add-Type -AssemblyName System.Drawing | Out-Null

$user32 = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class GwWindowProbeIdentSalv {
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
    $script:found = $false
    $callback = [GwWindowProbeIdentSalv+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        if (-not [GwWindowProbeIdentSalv]::IsWindowVisible($hWnd)) { return $true }
        $cls = New-Object System.Text.StringBuilder 256
        [void][GwWindowProbeIdentSalv]::GetClassName($hWnd, $cls, $cls.Capacity)
        $ttl = New-Object System.Text.StringBuilder 512
        [void][GwWindowProbeIdentSalv]::GetWindowText($hWnd, $ttl, $ttl.Capacity)
        if ($cls.ToString() -eq "#32770" -and $ttl.ToString() -eq "Gw.exe") {
            $windowPid = 0
            [void][GwWindowProbeIdentSalv]::GetWindowThreadProcessId($hWnd, [ref]$windowPid)
            if ($TargetPid -eq 0 -or $windowPid -eq $TargetPid) {
                $script:found = $true
            }
        }
        return $true
    }
    [void][GwWindowProbeIdentSalv]::EnumWindows($callback, [IntPtr]::Zero)
    return $script:found
}

function Capture-RunScreenshot {
    param([string]$Tag)
    New-Item -ItemType Directory -Force -Path $script:ScreenshotDir | Out-Null
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $path = Join-Path $script:ScreenshotDir ("identsalv_{0}_{1}.png" -f $Tag, $stamp)
    & $script:CaptureScript -OutputPath $path | Out-Null
    return $path
}

function Get-LatestHarnessBlock {
    if (-not (Test-Path $script:LogPath)) { return @() }
    try { $lines = Get-Content $script:LogPath -ErrorAction Stop } catch { return @() }
    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match "IDENTIFY/SALVAGE ISOLATION TEST MODE") { $start = $i }
    }
    if ($start -lt 0) { return @() }
    return $lines[$start..($lines.Count - 1)]
}

function Launch-GwViaGWLauncher {
    $charName = $script:CharacterNames[$AccountIndex]
    $launcherScript = Join-Path $script:LauncherScriptsDir "launch_$($charName.ToLower().Replace(' ',''))_via_gwlauncher.au3"

    if (-not (Test-Path $launcherScript)) {
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

    $logName = "launch_$($charName.ToLower().Replace(' ',''))_via_gwlauncher.log"
    $launcherLog = Join-Path (Split-Path $launcherScript -Parent) $logName
    if (-not (Test-Path (Split-Path $launcherScript -Parent))) {
        $launcherLog = Join-Path $env:TEMP $logName
    }

    Remove-Item $launcherLog -Force -ErrorAction SilentlyContinue
    Write-Host "Launching $charName via GWLauncher: $launcherScript"
    Start-Process -FilePath $script:AutoItPath -ArgumentList "`"$launcherScript`""

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
    $pidMatch = [regex]::Match($logContent, "GWLAUNCHER_PID=(\d+)")
    if (-not $pidMatch.Success) {
        throw "Could not parse PID from GWLauncher log"
    }
    return [int]$pidMatch.Groups[1].Value
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

Stop-CharacterGwProcesses -CharacterName $script:CharacterNames[$AccountIndex]
Start-Sleep -Seconds 2

$gwPid = Launch-GwViaGWLauncher
Write-Host "Waiting for live GW process for $($script:CharacterNames[$AccountIndex])..."
Start-Sleep -Seconds $LaunchTimeoutSeconds
$gwPid = Resolve-LiveGwPid -CharacterName $script:CharacterNames[$AccountIndex] -LauncherPidHint $gwPid -TimeoutSeconds 45

$script:LogPath = Join-Path $script:BinDir "gwa3_log_$gwPid.txt"
if (Test-Path $script:LogPath) {
    Remove-Item -LiteralPath $script:LogPath -Force -ErrorAction SilentlyContinue
}
Write-Host "Log path: $($script:LogPath)"

$dllNameLeaf = Split-Path $script:DllPath -Leaf
$injectArgs = @("--pid", "$gwPid", "--dll", $dllNameLeaf, "--test-identsalvage")
if ($Stage -ne "full") {
    $injectArgs += @("--identsalvage-stage", $Stage)
}
Write-Host "Injecting identify/salvage harness into PID $gwPid (DLL: $dllNameLeaf, stage: $Stage)..."
& $script:InjectorPath @injectArgs
if ($LASTEXITCODE -ne 0) { throw "Injection failed." }

$crashDialogSeen = $false
$failureLine = $null
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

    $ourGw = Get-Process -Id $gwPid -ErrorAction SilentlyContinue
    if (-not $ourGw -or $ourGw.HasExited) {
        $loopExitReason = "gw-process-exited"
        $loopExitElapsed = [int](((Get-Date) - $startTime).TotalSeconds)
        Dump-GwProcessSnapshot -Label "tracked pid $gwPid exited"
        break
    }

    if (Test-Path $script:LogPath) {
        $block = Get-LatestHarnessBlock
        foreach ($line in $block) {
            if ($line -match "Identify/salvage isolation test complete: .* failures") {
                $failureLine = $line
            }
            if ($line -match "\[WATCHDOG\].*(CRASH DIALOG|WINDOW NOT RESPONDING|RENDER FROZEN)") {
                $crashDialogSeen = $true
            }
        }
        if ($failureLine) {
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
Stop-Process -Id $gwPid -Force -ErrorAction SilentlyContinue

$block = @()
for ($i = 0; $i -lt 5; $i++) {
    $block = Get-LatestHarnessBlock
    if ($block.Count -gt 0) { break }
    Start-Sleep -Seconds 1
}

if ($block.Count -eq 0) {
    Write-Host "No identify/salvage block found in log."
    exit 2
}

$block | ForEach-Object { $_ }

foreach ($line in $block) {
    if ($line -match "Identify/salvage isolation test complete: .* failures") {
        $failureLine = $line
    }
}

if ($crashDialogSeen) {
    Write-Error "Identify/salvage run failed: visible Gw.exe crash dialog detected."
    exit 10
}

if (-not $failureLine) {
    Write-Error "Identify/salvage run failed: no completion line found."
    exit 12
}

if ($failureLine -match "Identify/salvage isolation test complete: (\d+) failures") {
    exit ([int]$Matches[1])
}

exit 0
