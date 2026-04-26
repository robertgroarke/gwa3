param([int]$TargetPid)
for ($i=0; $i -lt 15; $i++) {
    $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
    if (-not $p) { Write-Host "DEAD"; exit 2 }
    $mb = [math]::Round($p.WorkingSet/1MB,0)
    Write-Host "[$i] pid=$TargetPid mem=${mb}MB"
    if ($mb -ge 100) { Write-Host "HEALTHY"; exit 0 }
    Start-Sleep -Seconds 2
}
exit 1
