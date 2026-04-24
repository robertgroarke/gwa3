param()

$ErrorActionPreference = "Stop"

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$env:GWA3_BUILD_DIR = Join-Path $script:RepoRoot "build_marvin"
$env:GWA3_DLL_NAME = "gwa3_marvin.dll"
$env:GWA3_PIPE_NAME = "\\.\pipe\gwa3_llm_marvin"

Push-Location $script:RepoRoot
try {
    python -m bridge.tests --marvin-smoke
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
