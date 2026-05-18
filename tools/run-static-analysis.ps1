param(
    [switch]$CppOnly,
    [switch]$CSharpOnly,
    [string]$BuildDir = "build-analysis"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

function Section($Name) {
    Write-Host "`n===== $Name =====" -ForegroundColor Cyan
}

if (-not $CSharpOnly) {
    Section "C++ clang-tidy"
    if (-not (Get-Command clang-tidy -ErrorAction SilentlyContinue)) {
        throw "clang-tidy is not on PATH. Install LLVM 18+."
    }

    cmake -S . -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build $BuildDir --target gwa3 2>&1 | Tee-Object "$BuildDir/cmake-build.log" | Out-Null

    $sources = Get-ChildItem -Path src,bots -Recurse -Include *.cpp |
        Where-Object { $_.FullName -notmatch '\\third_party\\' } |
        Select-Object -ExpandProperty FullName

    & clang-tidy -p $BuildDir --quiet $sources 2>&1 |
        Tee-Object "$BuildDir/clang-tidy-report.txt" |
        Out-Null

    $errors = (Select-String -Path "$BuildDir/clang-tidy-report.txt" -Pattern ' error: ' -SimpleMatch).Count
    if ($errors -gt 0) {
        throw "clang-tidy reported $errors errors."
    }
}

if (-not $CppOnly) {
    Section "C# Roslyn analyzers"
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
        throw "dotnet SDK is not on PATH."
    }

    dotnet restore ui\Gwa3.UI.sln
    dotnet build ui\Gwa3.UI.sln -c Release `
        /warnaserror `
        /p:EnableGwa3StaticAnalysis=true `
        /p:RunAnalyzersDuringBuild=true `
        2>&1 |
        Tee-Object "$BuildDir/dotnet-analyzer-report.txt" |
        Out-Null
}

Section "Summary"
$summaryPath = Join-Path $BuildDir "static-analysis-summary.md"
@"
# Static Analysis Summary

Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

- clang-tidy report: $(if (Test-Path "$BuildDir/clang-tidy-report.txt") { "$BuildDir/clang-tidy-report.txt" } else { "skipped" })
- Roslyn report: $(if (Test-Path "$BuildDir/dotnet-analyzer-report.txt") { "$BuildDir/dotnet-analyzer-report.txt" } else { "skipped" })
"@ | Set-Content -Encoding utf8 $summaryPath

Write-Host "Summary written to $summaryPath" -ForegroundColor Green
