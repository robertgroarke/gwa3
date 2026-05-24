$ErrorActionPreference = 'Stop'

function Assert-True {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Wait-ForPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$BrokerProcess,
        [int]$TimeoutSeconds = 10
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $Path) {
            return
        }

        if ($BrokerProcess.HasExited) {
            throw "Broker exited before expected path appeared: $Path"
        }

        Start-Sleep -Milliseconds 100
    }

    throw "Timed out waiting for path: $Path"
}

function Start-PowerShell {
    param(
        [Parameter(Mandatory = $true)][string]$Arguments,
        [Parameter(Mandatory = $true)][string]$LocalAppData,
        [string]$FakeArgsPath = '',
        [bool]$Redirect = $false
    )

    $powershell = (Get-Command powershell.exe).Source
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $powershell
    $startInfo.Arguments = $Arguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $Redirect
    $startInfo.RedirectStandardError = $Redirect
    $startInfo.EnvironmentVariables['LOCALAPPDATA'] = $LocalAppData
    if (-not [string]::IsNullOrWhiteSpace($FakeArgsPath)) {
        $startInfo.EnvironmentVariables['FAKE_INJECTOR_ARGS'] = $FakeArgsPath
    }

    return [System.Diagnostics.Process]::Start($startInfo)
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$brokerScript = Join-Path $repoRoot 'tools\inject_broker.ps1'
$shimScript = Join-Path $repoRoot 'tools\inject_via_broker.ps1'

Assert-True (Test-Path -LiteralPath $brokerScript) "Missing broker script: $brokerScript"
Assert-True (Test-Path -LiteralPath $shimScript) "Missing broker shim: $shimScript"

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("gwa3-broker-smoke-" + [Guid]::NewGuid().ToString('N'))
$localAppData = Join-Path $tempRoot 'LocalAppData'
$fakeArgsPath = Join-Path $tempRoot 'fake-injector.args.txt'
$fakeInjector = Join-Path $tempRoot 'fake-injector.cmd'
$brokerProcess = $null

try {
    New-Item -ItemType Directory -Force -Path $localAppData | Out-Null
    Set-Content -Path $fakeInjector -Encoding ASCII -Value @"
@echo off
echo fake injector saw %*
echo %*>>"%FAKE_INJECTOR_ARGS%"
exit /b 17
"@

    $brokerProcess = Start-PowerShell `
        -Arguments "-NoProfile -ExecutionPolicy Bypass -File `"$brokerScript`"" `
        -LocalAppData $localAppData `
        -FakeArgsPath $fakeArgsPath

    $root = Join-Path $localAppData 'gwa3-inject-broker'
    $requestDirectory = Join-Path $root 'requests'
    $capabilityPath = Join-Path $root 'broker.capabilities.json'
    Wait-ForPath -Path $capabilityPath -BrokerProcess $brokerProcess
    Wait-ForPath -Path $requestDirectory -BrokerProcess $brokerProcess

    $shimProcess = Start-PowerShell `
        -Arguments "-NoProfile -ExecutionPolicy Bypass -File `"$shimScript`" -TargetPid 4242 -Dll gwa3_fake.dll -Injector `"$fakeInjector`" -TimeoutSeconds 10" `
        -LocalAppData $localAppData `
        -FakeArgsPath $fakeArgsPath `
        -Redirect $true
    $shimProcess.WaitForExit(15000) | Out-Null
    $shimOutput = $shimProcess.StandardOutput.ReadToEnd() + $shimProcess.StandardError.ReadToEnd()
    Assert-True $shimProcess.HasExited 'Broker shim did not exit.'
    Assert-True ($shimProcess.ExitCode -eq 17) "Expected fake injector exit code 17 through shim, got $($shimProcess.ExitCode). Output: $shimOutput"
    Assert-True ($shimOutput -match 'fake injector saw') "Shim output did not include fake injector output. Output: $shimOutput"

    $fakeArgs = Get-Content -Path $fakeArgsPath -Raw
    Assert-True ($fakeArgs -match '--pid 4242') "Fake injector did not receive the expected PID args. Args: $fakeArgs"
    Assert-True ($fakeArgs -match '--dll gwa3_fake\.dll') "Fake injector did not receive the expected DLL args. Args: $fakeArgs"

    $malformedId = 'malformed' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $malformedRequestPath = Join-Path $requestDirectory "$malformedId.req.json"
    $malformedDonePath = Join-Path $requestDirectory "$malformedId.done.json"
    $malformedExitPath = "$malformedRequestPath.exit"
    $malformedOutPath = "$malformedRequestPath.out"
    $malformedRequest = [ordered]@{
        source = 'tools.tests.inject_broker.malformed'
        pid = ''
        dll = ''
    }
    $malformedRequest | ConvertTo-Json | Set-Content -Path "$malformedRequestPath.tmp" -Encoding UTF8
    Move-Item -LiteralPath "$malformedRequestPath.tmp" -Destination $malformedRequestPath -Force

    Wait-ForPath -Path $malformedDonePath -BrokerProcess $brokerProcess
    $malformedExit = [int](Get-Content -Path $malformedExitPath -Raw).Trim()
    $malformedOut = Get-Content -Path $malformedOutPath -Raw
    Assert-True ($malformedExit -eq 64) "Expected malformed request exit 64, got $malformedExit. Output: $malformedOut"
    Assert-True ($malformedOut -match 'missing action and dll') "Malformed request was not rejected at the broker parser. Output: $malformedOut"

    $fakeArgsAfterMalformed = Get-Content -Path $fakeArgsPath -Raw
    $fakeInvocationCount = ([regex]::Matches($fakeArgsAfterMalformed, '--pid')).Count
    Assert-True ($fakeInvocationCount -eq 1) "Malformed request invoked the fake injector. Args: $fakeArgsAfterMalformed"

    Write-Output 'inject_broker smoke passed'
}
finally {
    if ($brokerProcess -and -not $brokerProcess.HasExited) {
        $brokerProcess.Kill()
        $brokerProcess.WaitForExit(5000) | Out-Null
    }

    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
