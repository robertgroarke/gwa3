param([int]$TargetPid)
$p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
if (-not $p) { Write-Host "DEAD"; exit 2 }
$mb = [math]::Round($p.WorkingSet/1MB,0)
Write-Host "pid=$TargetPid mem=${mb}MB responding=$($p.Responding) title='$($p.MainWindowTitle)' handle=$($p.MainWindowHandle)"
