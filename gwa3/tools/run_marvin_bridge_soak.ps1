param(
    [int]$Iterations = 3,
    [double]$CooldownSeconds = 5.0
)

$ErrorActionPreference = "Stop"

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$env:GWA3_BUILD_DIR = Join-Path $script:RepoRoot "build_marvin"
$env:GWA3_DLL_NAME = "gwa3_marvin.dll"
$env:GWA3_PIPE_NAME = "\\.\pipe\gwa3_llm_marvin"

Push-Location $script:RepoRoot
try {
    python -m bridge.tests --marvin-soak --iterations $Iterations --cooldown-seconds $CooldownSeconds
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
