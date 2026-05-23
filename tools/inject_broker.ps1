# Inject broker — run this in an ELEVATED PowerShell window and leave it open.
# It watches a request directory for inject jobs from non-elevated callers
# (Codex, this Claude CLI, manual shims) and runs the requested injector
# binary at high integrity so OpenProcess against the GW client succeeds.
#
# Request file format (drop a file named <id>.req.json):
#   {
#     "pid":      30372,
#     "dll":      "C:\\path\\to\\gwa3_disco.dll",
#     "injector": "C:\\path\\to\\injector.exe",   // optional; default below
#     "extra":    ["--test-froggy"]               // optional extra args
#   }
#
# Broker writes:
#   <id>.req.json.exit  — process exit code
#   <id>.req.json.out   — combined stdout+stderr
# Then renames the request to <id>.done.json so the shim can poll for it.
#
# Security note: anything that can write into the request dir can ask this
# broker to inject any DLL into any PID at high integrity. The dir lives
# under your user profile so only your account (and Administrators) can
# write to it.

$ErrorActionPreference = 'Stop'

$Root = Join-Path $env:LOCALAPPDATA 'gwa3-inject-broker'
$ReqDir = Join-Path $Root 'requests'
New-Item -ItemType Directory -Force $ReqDir | Out-Null

$DefaultInjector = 'C:\Users\Robert\Documents\gwa3-private\build_disco\bin\Release\injector.exe'

Write-Host "[broker] watching $ReqDir (default injector: $DefaultInjector)" -ForegroundColor Cyan
Write-Host "[broker] press Ctrl+C to stop."

while ($true) {
    $jobs = Get-ChildItem "$ReqDir\*.req.json" -ErrorAction SilentlyContinue
    foreach ($job in $jobs) {
        $id = $job.BaseName
        try {
            $req = Get-Content $job -Raw | ConvertFrom-Json
            $injector = if ($req.injector) { $req.injector } else { $DefaultInjector }
            $argList = @('--pid', $req.pid, '--dll', $req.dll)
            if ($req.extra) { $argList += $req.extra }

            Write-Host "[broker] $id : $injector $($argList -join ' ')" -ForegroundColor Yellow
            $out = & $injector @argList 2>&1 | Out-String
            $exit = $LASTEXITCODE

            Set-Content -Path "$($job.FullName).out"  -Value $out -Encoding UTF8
            Set-Content -Path "$($job.FullName).exit" -Value $exit
            Rename-Item $job.FullName "$id.done.json"
            Write-Host "[broker] $id : exit=$exit" -ForegroundColor Green
        }
        catch {
            $err = $_ | Out-String
            Set-Content -Path "$($job.FullName).out"  -Value $err -Encoding UTF8
            Set-Content -Path "$($job.FullName).exit" -Value 99
            Rename-Item $job.FullName "$id.done.json"
            Write-Host "[broker] $id : broker error" -ForegroundColor Red
            Write-Host $err
        }
    }
    Start-Sleep -Milliseconds 300
}
