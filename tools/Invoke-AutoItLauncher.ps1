param(
    [Parameter(Mandatory = $true)]
    [string]$LauncherScript,

    [string]$AutoItPath = "C:\Program Files (x86)\AutoIt3\AutoIt3.exe",

    [switch]$Hidden
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $AutoItPath -PathType Leaf)) {
    throw "AutoIt executable not found: $AutoItPath"
}

if (-not (Test-Path -LiteralPath $LauncherScript -PathType Leaf)) {
    throw "Launcher script not found: $LauncherScript"
}

$quotedLauncher = '"' + $LauncherScript + '"'
$windowStyle = if ($Hidden) { "Hidden" } else { "Normal" }

Write-Output ("AUTOIT_INVOCATION filePath={0} argumentList={1} windowStyle={2}" -f $AutoItPath, $quotedLauncher, $windowStyle)

$startProcessArgs = @{
    FilePath = $AutoItPath
    ArgumentList = $quotedLauncher
    Wait = $true
    PassThru = $true
}

if ($Hidden) {
    $startProcessArgs.WindowStyle = "Hidden"
}

$process = Start-Process @startProcessArgs
exit $process.ExitCode
