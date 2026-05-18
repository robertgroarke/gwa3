param(
    [string[]]$Path = @("build\bin\Release\gwa3.dll", "build\bin\Release\injector.exe"),
    [string]$PfxBase64 = $env:GWA3_CODESIGN_PFX_BASE64,
    [string]$PfxPassword = $env:GWA3_CODESIGN_PASSWORD,
    [switch]$AllowTemporaryCertificate,
    [switch]$RequireTrusted
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Resolve-SignPath([string]$InputPath) {
    $resolved = if ([System.IO.Path]::IsPathRooted($InputPath)) {
        $InputPath
    } else {
        Join-Path $repoRoot $InputPath
    }
    if (-not (Test-Path -LiteralPath $resolved)) {
        throw "Cannot sign missing file: $InputPath"
    }
    return (Resolve-Path -LiteralPath $resolved).Path
}

function New-SigningCertificate {
    if (-not [string]::IsNullOrWhiteSpace($PfxBase64)) {
        $pfxPath = Join-Path ([System.IO.Path]::GetTempPath()) "gwa3-codesign.pfx"
        try {
            [System.IO.File]::WriteAllBytes($pfxPath, [Convert]::FromBase64String($PfxBase64))
            $securePassword = ConvertTo-SecureString $PfxPassword -AsPlainText -Force
            return Import-PfxCertificate -FilePath $pfxPath -CertStoreLocation Cert:\CurrentUser\My -Password $securePassword
        } catch {
            if (-not $AllowTemporaryCertificate) {
                throw
            }
            Write-Warning "Provided GWA3_CODESIGN_PFX_BASE64 was invalid; falling back to a temporary CI certificate."
        }
    }

    if ($AllowTemporaryCertificate) {
        Write-Warning "Using a temporary self-signed CI code-signing certificate. Release builds should use GWA3_CODESIGN_* secrets."
        return New-SelfSignedCertificate `
            -Type CodeSigningCert `
            -Subject "CN=GWA3 Temporary CI Code Signing" `
            -CertStoreLocation Cert:\CurrentUser\My `
            -KeyExportPolicy Exportable `
            -HashAlgorithm SHA256 `
            -KeyLength 2048
    }

    throw "No code-signing certificate was provided. Set GWA3_CODESIGN_PFX_BASE64 and GWA3_CODESIGN_PASSWORD."
}

$certificate = New-SigningCertificate
$signTargets = @($Path | ForEach-Object { Resolve-SignPath $_ })

foreach ($target in $signTargets) {
    $signature = Set-AuthenticodeSignature `
        -FilePath $target `
        -Certificate $certificate `
        -HashAlgorithm SHA256 `
        -TimestampServer "http://timestamp.digicert.com"

    if ($null -eq $signature.SignerCertificate) {
        throw "Signing did not attach a certificate to $target"
    }
    if ($RequireTrusted -and $signature.Status -ne "Valid") {
        throw "Signature for $target is not trusted: $($signature.Status) $($signature.StatusMessage)"
    }
    if ($signature.Status -eq "NotSigned") {
        throw "Signature failed for $target"
    }

    Write-Host "Signed $target ($($signature.SignerCertificate.Subject)); status=$($signature.Status)"
}
