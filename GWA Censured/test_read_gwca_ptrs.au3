#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; gwca.dll base from injection output
Local $base = 0x70590000

; Read all interesting data section addresses
Local $offsets[][2] = [ _
    [0x8A39C, "orig SendFrameUIMsg (pre-hook)"], _
    [0x8A3A0, "hooked SendFrameUIMsg"], _
    [0x8A37C, "GetChildFrame func"], _
    [0x8A410, "RootFrame ptr"], _
    [0x8A3D0, "SetWindowVisible"], _
    [0x8A394, "SetWindowPosition"], _
    [0x8A354, "another UI func"], _
    [0x8A274, "DoAction"], _
    [0x880F0, "hash seed esi"], _
    [0x880F4, "hash seed eax"], _
    [0x880F8, "hash seed 2"], _
    [0x8A3B0, "frame hash table"] _
]

For $i = 0 To UBound($offsets) - 1
    Local $val = MemoryRead($ph, $base + $offsets[$i][0], 'dword')
    ConsoleWrite("+0x" & Hex($offsets[$i][0], 5) & " " & $offsets[$i][1] & " = 0x" & Hex($val) & @CRLF)
Next

; CRITICAL: The original SendFrameUIMsg (at 0x8A39C) is the REAL game function
; Compare with our scan result
ConsoleWrite(@CRLF & "Our scan found: 0x" & Hex(Int(GetLabel('SendFrameUIMsg'))) & @CRLF)
Local $realFunc = MemoryRead($ph, $base + 0x8A39C, 'dword')
ConsoleWrite("GWCA's original func: 0x" & Hex($realFunc) & @CRLF)
If $realFunc = Int(GetLabel('SendFrameUIMsg')) Then
    ConsoleWrite("MATCH! Our scan pattern found the correct function!" & @CRLF)
Else
    ConsoleWrite("MISMATCH! Our scan found wrong function. Real = 0x" & Hex($realFunc) & @CRLF)
EndIf
