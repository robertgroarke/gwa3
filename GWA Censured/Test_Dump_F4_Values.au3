#RequireAdmin
#include "lib\GWA2.au3"

InitializeGWMemory()
Local $merchantItemsBase = GetMerchantItemsBase()
Local $merchantItemsSize = GetMerchantItemsSize()
Local $out = ""

Local $buyItemBaseAddr = GetScannedAddress('ScanBuyItemBase', 15)
Local $buyItemBase = MemoryRead($buyItemBaseAddr)

Local $f4val = MemoryRead($buyItemBase + 0xF4)
Local $f4ptr = $buyItemBase + ($f4val * 3) * 4

$out &= "Value at calculated ptr         [F4*12+0]: " & MemoryRead($f4ptr) & @CRLF
$out &= "Value at calculated ptr + 4     [F4*12+4]: " & MemoryRead($f4ptr + 4) & @CRLF
$out &= "Value at calculated ptr + 8     [F4*12+8]: " & MemoryRead($f4ptr + 8) & @CRLF
$out &= "Value at calculated ptr + 12    [F4*12+12]: " & MemoryRead($f4ptr + 12) & @CRLF
$out &= "Value at calculated ptr + 16    [F4*12+16]: " & MemoryRead($f4ptr + 16) & @CRLF
$out &= "Value at calculated ptr + 20    [F4*12+20]: " & MemoryRead($f4ptr + 20) & @CRLF
$out &= "Value at calculated ptr + 24    [F4*12+24]: " & MemoryRead($f4ptr + 24) & @CRLF

FileWrite("f4dump.txt", $out)
Exit
