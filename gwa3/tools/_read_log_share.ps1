param([string]$Path, [int]$Tail = 30)
$fs = [System.IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
$sr = New-Object System.IO.StreamReader($fs)
$c = $sr.ReadToEnd()
$sr.Close()
$fs.Close()
$lines = $c -split "`n"
$start = [Math]::Max(0, $lines.Length - $Tail)
for ($i = $start; $i -lt $lines.Length; $i++) { $lines[$i] }
