param([int]$TargetPid)
$python = "python"
$fireScript = "c:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/tools/_fire_set_window.py"
$snapScript = "c:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/tools/_snap_game_window.ps1"
$outDir = "c:/Users/Robert/Documents/GWA Censured X BotsHub/gwa3/tools/sweep"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# WindowIDs to try: around the known Quest Log position, plus a few
# known landmarks for sanity.
$ids = @(0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A)
foreach ($id in $ids) {
    $hex = "{0:X2}" -f $id
    Write-Host "=== Trying 0x$hex ==="
    & $python $fireScript "0x$hex" 1 | Out-Null
    Start-Sleep -Seconds 2
    & powershell -ExecutionPolicy Bypass -File $snapScript -TargetPid $TargetPid -OutPath "$outDir/window_0x$hex.png" 2>&1 | Out-Null
    # Close it again so we can tell which ID opened what
    & $python $fireScript "0x$hex" 0 | Out-Null
    Start-Sleep -Seconds 1
}
Write-Host "Screenshots saved to $outDir"
