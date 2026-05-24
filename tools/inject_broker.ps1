# Inject broker — run this in an ELEVATED PowerShell window and leave it open.
# It watches a request directory for inject jobs from non-elevated callers
# (Codex, this Claude CLI, manual shims) and runs the requested injector
# binary at high integrity so OpenProcess against the GW client succeeds.
#
# Request file format (drop a file named <id>.req.json):
#   {
#     "action":   "inject",
#     "pid":      30372,
#     "dll":      "C:\\path\\to\\gwa3_disco.dll",
#     "injector": "C:\\path\\to\\injector.exe",   // optional; default below
#     "extra":    ["--test-froggy"],              // optional extra args
#     "source":   "call.site.name"                // optional producer/call-site label
#   }
#
# Broker health probes may use:
#   { "action": "ping", "source": "call.site.name" }
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
$CapabilityPath = Join-Path $Root 'broker.capabilities.json'
$BrokerStartedUtc = (Get-Date).ToUniversalTime().ToString('o')
New-Item -ItemType Directory -Force $ReqDir | Out-Null

$DefaultInjector = 'C:\Users\Robert\Documents\gwa3-private\build_disco\bin\Release\injector.exe'

Write-Host "[broker] watching $ReqDir (default injector: $DefaultInjector)" -ForegroundColor Cyan
Write-Host "[broker] press Ctrl+C to stop."

function Write-BrokerCapabilities {
    $capabilities = [ordered]@{
        schemaVersion = 2
        brokerPid = $PID
        scriptPath = $PSCommandPath
        startedUtc = $BrokerStartedUtc
        refreshedUtc = (Get-Date).ToUniversalTime().ToString('o')
        actions = @('ping', 'inject', 'terminate')
    }

    $capabilities | ConvertTo-Json | Set-Content -Path $CapabilityPath -Encoding UTF8
}

function Complete-Request {
    param(
        [Parameter(Mandatory=$true)]$Job,
        [Parameter(Mandatory=$true)][string]$Id,
        [Parameter(Mandatory=$true)][AllowEmptyString()][string]$Output,
        [Parameter(Mandatory=$true)][int]$ExitCode,
        [Parameter(Mandatory=$true)][ConsoleColor]$Color,
        [Parameter(Mandatory=$true)][string]$Message
    )

    Set-Content -Path "$($Job.FullName).out"  -Value $Output -Encoding UTF8
    Set-Content -Path "$($Job.FullName).exit" -Value $ExitCode
    Rename-Item $Job.FullName "$Id.done.json"
    Write-Host "[broker] $Id : $Message" -ForegroundColor $Color
}

function Reject-Request {
    param(
        [Parameter(Mandatory=$true)]$Job,
        [Parameter(Mandatory=$true)][string]$Id,
        [Parameter(Mandatory=$true)][string]$Reason
    )

    Complete-Request -Job $Job -Id $Id -Output $Reason -ExitCode 64 -Color Red -Message "rejected malformed request"
    Write-Host $Reason
}

Write-BrokerCapabilities
$nextCapabilityRefresh = (Get-Date).AddSeconds(5)

while ($true) {
    if ((Get-Date) -ge $nextCapabilityRefresh) {
        Write-BrokerCapabilities
        $nextCapabilityRefresh = (Get-Date).AddSeconds(5)
    }

    $jobs = Get-ChildItem "$ReqDir\*.req.json" -ErrorAction SilentlyContinue
    foreach ($job in $jobs) {
        # $job.BaseName for "abc.req.json" is "abc.req" — strip the trailing
        # ".req" so the response filename matches the shim's expectation of
        # "<id>.done.json" (not "<id>.req.done.json").
        $id = $job.BaseName -replace '\.req$', ''
        if (((Get-Date).ToUniversalTime() - $job.LastWriteTimeUtc) -lt [TimeSpan]::FromMilliseconds(500)) {
            continue
        }

        try {
            $req = Get-Content $job -Raw | ConvertFrom-Json
            $pidText = [string]$req.pid
            $dll = [string]$req.dll
            $action = [string]$req.action
            $source = [string]$req.source
            if ([string]::IsNullOrWhiteSpace($source)) {
                $source = 'unknown source'
            }
            if ([string]::IsNullOrWhiteSpace($action) -and [string]$req.type -ieq 'ping') {
                $action = 'ping'
            }
            if ([string]::IsNullOrWhiteSpace($action) -and $dll -ieq 'registry-cleanup' -and ($req.injector -or $req.terminator)) {
                $action = 'terminate'
            }
            if ([string]::IsNullOrWhiteSpace($action) -and -not [string]::IsNullOrWhiteSpace($dll)) {
                $action = 'inject'
            }
            if ([string]::IsNullOrWhiteSpace($action)) {
                Reject-Request -Job $job -Id $id -Reason "Rejected broker request ${id} from ${source}: missing action and dll; refusing to infer inject."
                continue
            }
            $action = $action.ToLowerInvariant()

            if ($action -eq 'ping') {
                $pong = [ordered]@{
                    type = 'pong'
                    requestId = $id
                    brokerPid = $PID
                    source = $source
                    receivedUtc = (Get-Date).ToUniversalTime().ToString('o')
                }
                $pong | ConvertTo-Json | Set-Content -Path $job.FullName -Encoding UTF8
                Complete-Request -Job $job -Id $id -Output '' -ExitCode 0 -Color Green -Message "pong source=$source"
                continue
            }

            $pidValue = 0
            if ([string]::IsNullOrWhiteSpace($pidText) -or -not [int]::TryParse($pidText, [ref]$pidValue) -or $pidValue -le 0) {
                Reject-Request -Job $job -Id $id -Reason "Rejected broker request ${id} from ${source}: missing or invalid pid '$pidText'."
                continue
            }

            if ($action -eq 'terminate' -or $action -eq 'kill' -or $action -eq 'cleanup') {
                $terminator = if ($req.terminator) { [string]$req.terminator } elseif ($req.injector) { [string]$req.injector } else { '' }
                if ([string]::IsNullOrWhiteSpace($terminator)) {
                    Reject-Request -Job $job -Id $id -Reason "Rejected broker request ${id} from ${source}: missing terminator."
                    continue
                }

                $argList = @('--pid', $pidValue.ToString())
                if ($req.extra) { $argList += $req.extra }

                Write-Host "[broker] $id : source=$source terminate via $terminator $($argList -join ' ')" -ForegroundColor Yellow
                $out = & $terminator @argList 2>&1 | Out-String
                $exit = $LASTEXITCODE

                Complete-Request -Job $job -Id $id -Output $out -ExitCode $exit -Color Green -Message "terminate exit=$exit"
                continue
            }

            if ($action -ne 'inject') {
                Reject-Request -Job $job -Id $id -Reason "Rejected broker request ${id} from ${source}: unknown action '$action'."
                continue
            }

            if ([string]::IsNullOrWhiteSpace($dll)) {
                Reject-Request -Job $job -Id $id -Reason "Rejected broker request ${id} from ${source}: missing dll."
                continue
            }

            $injector = if ($req.injector) { $req.injector } else { $DefaultInjector }
            $argList = @('--pid', $pidValue.ToString(), '--dll', $dll)
            if ($req.extra) { $argList += $req.extra }

            Write-Host "[broker] $id : source=$source $injector $($argList -join ' ')" -ForegroundColor Yellow
            $out = & $injector @argList 2>&1 | Out-String
            $exit = $LASTEXITCODE

            Complete-Request -Job $job -Id $id -Output $out -ExitCode $exit -Color Green -Message "exit=$exit"
        }
        catch [System.IO.IOException] {
            Write-Host "[broker] $id : request file is still busy; retrying" -ForegroundColor DarkYellow
        }
        catch {
            $err = $_ | Out-String
            Complete-Request -Job $job -Id $id -Output $err -ExitCode 99 -Color Red -Message "broker error"
            Write-Host $err
        }
    }
    Start-Sleep -Milliseconds 300
}
