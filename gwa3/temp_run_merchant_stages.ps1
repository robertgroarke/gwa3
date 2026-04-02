$ErrorActionPreference = 'Stop'

function Launch-GW {
    Stop-Process -Name 'Gw' -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2

    $accts = Get-Content 'C:/Users/Robert/Downloads/GWLauncher/Accounts.json' | ConvertFrom-Json
    $a = $accts[0]
    $gwDir = Split-Path $a.gwpath
    New-ItemProperty -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Name 'Path' -Value $a.gwpath -PropertyType String -Force | Out-Null
    New-ItemProperty -Path 'HKCU:\Software\ArenaNet\Guild Wars' -Name 'Src' -Value $gwDir -PropertyType String -Force | Out-Null

    $argString = "-email `"$($a.email)`" -password `"$($a.password)`" -character `"$($a.character)`" $($a.extraargs)"
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $a.gwpath
    $psi.Arguments = $argString
    $psi.UseShellExecute = $false
    $proc = [System.Diagnostics.Process]::Start($psi)
    return $proc.Id
}

function Save-DesktopShot([string]$Path) {
    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName System.Windows.Forms
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
}

function Run-MerchantStage([string]$Stage, [string]$Variant, [string]$Prefix) {
    $pid = Launch-GW
    Write-Host "Launched $Stage/$Variant PID=$pid"
    Start-Sleep -Seconds 50

    Remove-Item 'C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/gwa3_log.txt' -ErrorAction SilentlyContinue
    Remove-Item 'C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/gwa3_integration_report.txt' -ErrorAction SilentlyContinue

    $args = @('--test-merchant', '--merchant-stage', $Stage)
    if ($Variant -ne '') {
        $args += @('--merchant-variant', $Variant)
    }

    & 'C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/injector.exe' @args
    Start-Sleep -Seconds 55

    $shot = "C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/$Prefix.png"
    Save-DesktopShot $shot

    Stop-Process -Name 'Gw' -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 3

    Copy-Item 'C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/gwa3_integration_report.txt' "C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/$Prefix.txt" -Force
    Copy-Item 'C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/gwa3_log.txt' "C:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/build/bin/Release/$Prefix.log" -Force
}

Run-MerchantStage 'travel-only' '' 'merchant_stage_travel_only'
Run-MerchantStage 'interact-only' '' 'merchant_stage_interact_only'
Run-MerchantStage 'full' 'legacy-id' 'merchant_stage_full_legacy_id'
