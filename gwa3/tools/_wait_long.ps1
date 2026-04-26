param([int]$TargetPid, [int]$Iterations=20, [int]$SleepSec=3)
for ($i=0; $i -lt $Iterations; $i++) {
    $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
    if (-not $p) { Write-Host "DEAD"; exit 2 }
    $mb = [math]::Round($p.WorkingSet/1MB,0)
    Write-Host ("[{0}] mem={1}MB title='{2}'" -f $i,$mb,$p.MainWindowTitle)
    if ($mb -ge 100) { Write-Host "HEALTHY"; exit 0 }
    Start-Sleep -Seconds $SleepSec
}
exit 1
