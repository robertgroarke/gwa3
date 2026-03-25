#include "lib\GWCAConstants.au3"
#include "lib\Utils-Memory.au3"
#include "lib\Utils-Pointers.au3"
$out = ""
InitializeGWMemory()
Local $buyItemBaseAddr = GetScannedAddress('ScanBuyItemBase', 15)
Local $buyItemBase = MemoryRead($buyItemBaseAddr)
Local $f4val = MemoryRead($buyItemBase + 0xF4)
Local $f4ptr1 = $buyItemBase + ($f4val * 3) * 4
Local $f4ptr2 = $buyItemBase + ($f4val * 1) * 4

$out &= "F4Val: " & $f4val & @CRLF
$out &= "Ptr1 (12): " & $f4ptr1 & @CRLF
$out &= "Ptr2 (4): " & $f4ptr2 & @CRLF

Local $modelID = 24861 ; Grail of Might
Local $magic1 = MemoryRead($f4ptr1 + $modelID * 4)
Local $magic2 = MemoryRead($f4ptr2 + $modelID * 4)

$out &= "Magic1 at (" & $f4ptr1 & " + " & ($modelID*4) & "): " & $magic1 & @CRLF
$out &= "Magic2 at (" & $f4ptr2 & " + " & ($modelID*4) & "): " & $magic2 & @CRLF

ConsoleWrite($out)
