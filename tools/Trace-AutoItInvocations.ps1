param(
    [int]$Seconds = 120,
    [string]$LogPath = (Join-Path $env:TEMP ("gwa3-autoit-invocations-{0}.log" -f (Get-Date -Format "yyyyMMdd-HHmmss")))
)

$ErrorActionPreference = "Stop"

function Write-TraceLine {
    param([string]$Line)
    $timestamp = (Get-Date).ToUniversalTime().ToString("o")
    Add-Content -LiteralPath $LogPath -Value "$timestamp $Line"
}

New-Item -ItemType Directory -Path (Split-Path -Parent $LogPath) -Force | Out-Null
Write-TraceLine "trace-start seconds=$Seconds"

Get-CimInstance Win32_Process |
    Where-Object { $_.Name -match '^AutoIt3(_x64)?\.exe$' } |
    ForEach-Object {
        Write-TraceLine ("existing pid={0} name={1} commandLine={2}" -f $_.ProcessId, $_.Name, $_.CommandLine)
    }

$seen = @{}

$query = "SELECT * FROM Win32_ProcessStartTrace WHERE ProcessName = 'AutoIt3.exe' OR ProcessName = 'AutoIt3_x64.exe'"
$sourceId = "gwa3-autoit-trace-{0}" -f ([Guid]::NewGuid().ToString("N"))
$action = {
    $processId = $Event.SourceEventArgs.NewEvent.ProcessID
    $name = $Event.SourceEventArgs.NewEvent.ProcessName
    $line = "<unavailable>"
    try {
        $proc = Get-CimInstance Win32_Process -Filter "ProcessId = $processId"
        if ($proc -and $proc.CommandLine) {
            $line = $proc.CommandLine
        }
    } catch {
        $line = "<query-failed: $($_.Exception.Message)>"
    }
    $timestamp = (Get-Date).ToUniversalTime().ToString("o")
    Add-Content -LiteralPath $using:LogPath -Value ("{0} start pid={1} name={2} commandLine={3}" -f $timestamp, $processId, $name, $line)
}

$registration = Register-CimIndicationEvent -Query $query -SourceIdentifier $sourceId -Action $action
try {
    Write-Output "Tracing AutoIt invocations for $Seconds seconds."
    Write-Output "Log: $LogPath"
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        Get-CimInstance Win32_Process |
            Where-Object { $_.Name -match '^AutoIt3(_x64)?\.exe$' } |
            ForEach-Object {
                $key = "{0}:{1}" -f $_.ProcessId, $_.CommandLine
                if (-not $seen.ContainsKey($key)) {
                    $seen[$key] = $true
                    Write-TraceLine ("poll pid={0} name={1} commandLine={2}" -f $_.ProcessId, $_.Name, $_.CommandLine)
                }
            }
        Start-Sleep -Milliseconds 50
    }
} finally {
    Unregister-Event -SourceIdentifier $sourceId -ErrorAction SilentlyContinue
    $registration | Remove-Job -Force -ErrorAction SilentlyContinue
    Write-TraceLine "trace-stop"
    Write-Output "Trace complete: $LogPath"
}
