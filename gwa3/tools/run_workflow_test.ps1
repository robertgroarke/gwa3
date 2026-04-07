param(
    [int]$AccountIndex = 0,
    [int]$LaunchTimeoutSeconds = 45,
    [int]$RunTimeoutSeconds = 180,
    [int]$PostSummaryObservationSeconds = 8,
    [string]$AccountsPath = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
)

$ErrorActionPreference = "Stop"

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:BinDir = Join-Path $script:RepoRoot "build\bin\Release"
$script:InjectorPath = Join-Path $script:BinDir "injector.exe"
$script:DllPath = Join-Path $script:BinDir "gwa3.dll"
$script:LogPath = Join-Path $script:BinDir "gwa3_log.txt"
$script:ReportPath = Join-Path $script:BinDir "gwa3_integration_report.txt"
$script:WorkflowFlagPath = Join-Path $script:BinDir "gwa3_test_workflow.flag"
$script:ScreenshotDir = Join-Path $script:BinDir "screenshots"
$script:CaptureScript = Join-Path $PSScriptRoot "capture_screen.ps1"

Add-Type -AssemblyName System.Windows.Forms | Out-Null
Add-Type -AssemblyName System.Drawing | Out-Null

$user32 = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class GwWindowProbe {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")] public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
}
"@
Add-Type -TypeDefinition $user32 | Out-Null

function Get-VisibleWindows {
    $windows = [System.Collections.Generic.List[object]]::new()
    $callback = [GwWindowProbe+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        if (-not [GwWindowProbe]::IsWindowVisible($hWnd)) {
            return $true
        }

        $titleBuilder = New-Object System.Text.StringBuilder 512
        [void][GwWindowProbe]::GetWindowText($hWnd, $titleBuilder, $titleBuilder.Capacity)
        $classBuilder = New-Object System.Text.StringBuilder 256
        [void][GwWindowProbe]::GetClassName($hWnd, $classBuilder, $classBuilder.Capacity)
        [uint32]$procId = 0
        [void][GwWindowProbe]::GetWindowThreadProcessId($hWnd, [ref]$procId)

        $windows.Add([pscustomobject]@{
            Handle = $hWnd
            Title = $titleBuilder.ToString()
            ClassName = $classBuilder.ToString()
            ProcessId = $procId
        })
        return $true
    }

    [void][GwWindowProbe]::EnumWindows($callback, [IntPtr]::Zero)
    return $windows
}

function Get-GwCrashDialog {
    $windows = Get-VisibleWindows
    foreach ($window in $windows) {
        if ($window.ClassName -eq "#32770" -and $window.Title -eq "Gw.exe") {
            return $window
        }
    }
    return $null
}

function Capture-RunScreenshot {
    param([string]$Tag)
    New-Item -ItemType Directory -Force -Path $script:ScreenshotDir | Out-Null
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $path = Join-Path $script:ScreenshotDir ("workflow_{0}_{1}.png" -f $Tag, $stamp)
    & $script:CaptureScript -OutputPath $path | Out-Null
    return $path
}

function Get-LatestWorkflowBlock {
    if (-not (Test-Path $script:LogPath)) {
        return @()
    }

    try {
        $lines = Get-Content $script:LogPath -ErrorAction Stop
    } catch {
        return @()
    }
    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match "Advanced Workflow Test Suite \(074-089\)") {
            $start = $i
        }
    }
    if ($start -lt 0) {
        return @()
    }

    return $lines[$start..($lines.Count - 1)]
}

function Wait-ForGwWindow {
    param([int]$TimeoutSeconds)

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $listing = & $script:InjectorPath --list 2>&1
        if ($LASTEXITCODE -eq 0 -and ($listing -match "PID")) {
            $gw = Get-Process Gw -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($gw) {
                return $gw.Id
            }
        }
        Start-Sleep -Seconds 1
    }

    throw "Timed out waiting for Guild Wars window."
}

function Launch-GwAccount {
    if (-not (Test-Path $AccountsPath)) {
        throw "Accounts file not found: $AccountsPath"
    }

    $accounts = Get-Content $AccountsPath | ConvertFrom-Json
    if ($AccountIndex -lt 0 -or $AccountIndex -ge $accounts.Count) {
        throw "AccountIndex $AccountIndex is out of range for $($accounts.Count) accounts."
    }

    $acct = $accounts[$AccountIndex]
    $gwPath = [string]$acct.gwpath
    $gwDir = Split-Path $gwPath -Parent

    New-Item -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Force | Out-Null
    Set-ItemProperty -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Name Path -Value $gwPath
    Set-ItemProperty -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Name Src -Value $gwDir

    $argString = '-email "{0}" -password "{1}" -character "{2}" {3}' -f `
        $acct.email, $acct.password, $acct.character, $acct.extraargs

    Start-Process -FilePath $gwPath -ArgumentList $argString | Out-Null
    Write-Host "GW launched for character: $($acct.character)"
}

Stop-Process -Name "Gw" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

Remove-Item $script:ReportPath -Force -ErrorAction SilentlyContinue
New-Item -ItemType File -Force -Path $script:WorkflowFlagPath | Out-Null

Launch-GwAccount
$gwPid = Wait-ForGwWindow -TimeoutSeconds $LaunchTimeoutSeconds

Write-Host "Injecting workflow test into PID $gwPid..."
& $script:InjectorPath --pid $gwPid --test-workflow
if ($LASTEXITCODE -ne 0) {
    throw "Injection failed."
}

$crashDialogSeen = $false
$disconnectSeen = $false
$summarySeen = $false
$summaryLine = $null
$failureLine = $null
$summarySeenAt = $null
$startTime = Get-Date

while (((Get-Date) - $startTime).TotalSeconds -lt $RunTimeoutSeconds) {
    if (Get-GwCrashDialog) {
        $crashDialogSeen = $true
        $shot = Capture-RunScreenshot -Tag "crash_dialog"
        Write-Host "CRASH_DIALOG_DETECTED: $shot"
        break
    }

    if (-not (Get-Process Gw -ErrorAction SilentlyContinue)) {
        break
    }

    if (Test-Path $script:LogPath) {
        $workflowLines = Get-LatestWorkflowBlock
        foreach ($line in $workflowLines) {
            if ($line -match "\[WATCHDOG\].*DISCONNECT DETECTED") {
                $disconnectSeen = $true
            }
            if ($line -match "=== SUMMARY:") {
                $summarySeen = $true
                $summaryLine = $line
                if (-not $summarySeenAt) {
                    $summarySeenAt = Get-Date
                }
            }
            if ($line -match "Workflow test complete: .* failures") {
                $failureLine = $line
            }
        }
        if ($summarySeen -and $failureLine -and $summarySeenAt -and (((Get-Date) - $summarySeenAt).TotalSeconds -ge $PostSummaryObservationSeconds)) {
            break
        }
    }

    Start-Sleep -Seconds 1
}

$endShot = Capture-RunScreenshot -Tag "end"
Write-Host "END_SCREENSHOT: $endShot"

Stop-Process -Name Gw -Force -ErrorAction SilentlyContinue

$workflowBlock = @()
for ($i = 0; $i -lt 5; $i++) {
    $workflowBlock = Get-LatestWorkflowBlock
    if ($workflowBlock.Count -gt 0) {
        break
    }
    Start-Sleep -Seconds 1
}
if ($workflowBlock.Count -eq 0) {
    Write-Host "No workflow block found in log."
    exit 2
}

foreach ($line in $workflowBlock) {
    if ($line -match "\[WATCHDOG\].*DISCONNECT DETECTED") {
        $disconnectSeen = $true
    }
    if ($line -match "=== SUMMARY:") {
        $summarySeen = $true
        $summaryLine = $line
    }
    if ($line -match "Workflow test complete: .* failures") {
        $failureLine = $line
    }
}

$workflowBlock | ForEach-Object { $_ }

if ($crashDialogSeen) {
    Write-Error "Workflow run failed: visible Gw.exe crash dialog detected."
    exit 10
}

if ($disconnectSeen) {
    Write-Error "Workflow run failed: disconnect detected by watchdog."
    exit 11
}

if (-not $summarySeen) {
    Write-Error "Workflow run failed: no summary line found; run is not trustworthy."
    exit 12
}

if ($failureLine -and $failureLine -match "Workflow test complete: (\d+) failures") {
    $failures = [int]$Matches[1]
    exit $failures
}

exit 0
