# Non-elevated shim — call this from any caller (Codex, scripts) that needs
# to inject a DLL but cannot OpenProcess itself. Drops a request file into
# the broker's watch directory and polls for the result.
#
# Usage:
#   .\inject_via_broker.ps1 -Pid 12345 -Dll C:\...\gwa3_disco.dll
#   .\inject_via_broker.ps1 -Pid 12345 -Dll C:\...\gwa3_disco.dll -Injector C:\custom\injector.exe -Extra '--test-froggy'
#   .\inject_via_broker.ps1 ... -TimeoutSeconds 60
#
# Exits with the broker'd injector's exit code (0 = success). stdout is
# the captured injector output.

param(
    [Parameter(Mandatory=$true)][Alias('Pid')][int]$TargetPid,
    [Parameter(Mandatory=$true)][string]$Dll,
    [string]$Injector = '',
    [string[]]$Extra = @(),
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'

$Root = Join-Path $env:LOCALAPPDATA 'gwa3-inject-broker'
$ReqDir = Join-Path $Root 'requests'

if (-not (Test-Path $ReqDir)) {
    [Console]::Error.WriteLine("Broker not running (request dir missing: $ReqDir). Start tools\inject_broker.ps1 in an elevated PowerShell window first.")
    exit 100
}

if ($TargetPid -le 0) {
    [Console]::Error.WriteLine("Broker request requires a positive PID.")
    exit 64
}

if ([string]::IsNullOrWhiteSpace($Dll)) {
    [Console]::Error.WriteLine("Broker request requires a non-empty DLL path or DLL name.")
    exit 64
}

$id = [Guid]::NewGuid().ToString('N').Substring(0, 12)
$req = [ordered]@{
    action = 'inject'
    pid = $TargetPid
    dll = $Dll
    source = 'tools.inject_via_broker.ps1'
}
if ($Injector) { $req.injector = $Injector }
if ($Extra.Count -gt 0) { $req.extra = $Extra }

$reqPath = Join-Path $ReqDir "$id.req.json"
$tempPath = "$reqPath.tmp"
$donePath = Join-Path $ReqDir "$id.done.json"
$outPath = "$reqPath.out"
$exitPath = "$reqPath.exit"

$req | ConvertTo-Json | Set-Content -Path $tempPath -Encoding UTF8
Move-Item -LiteralPath $tempPath -Destination $reqPath -Force

$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
while ((Get-Date) -lt $deadline) {
    if (Test-Path $donePath) { break }
    Start-Sleep -Milliseconds 200
}

if (-not (Test-Path $donePath)) {
    [Console]::Error.WriteLine("Broker did not respond within ${TimeoutSeconds}s. Is tools\inject_broker.ps1 running in an elevated window?")
    Remove-Item $reqPath, $tempPath -ErrorAction SilentlyContinue
    exit 101
}

if (Test-Path $outPath)  { Get-Content $outPath -Raw }
$exitCode = if (Test-Path $exitPath) { [int](Get-Content $exitPath -Raw).Trim() } else { 102 }

# Clean up
Remove-Item $donePath, $outPath, $exitPath -ErrorAction SilentlyContinue

exit $exitCode
