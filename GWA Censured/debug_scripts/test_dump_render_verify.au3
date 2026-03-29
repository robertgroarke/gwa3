#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Follow JMP at RenderingMod to find RenderingModProc
Local $rendMod = Int(GetLabel('RenderingMod'))
Local $relBuf = DllStructCreate('int')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($rendMod + 1), _
    'ptr', DllStructGetPtr($relBuf), 'ulong_ptr', 4, 'ulong_ptr*', 0)
Local $rendProc = $rendMod + 5 + DllStructGetData($relBuf, 1)
ConsoleWrite("RenderingModProc at 0x" & Hex($rendProc) & @CRLF)

; Dump 128 bytes
Local $buf = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($rendProc), _
    'ptr', DllStructGetPtr($buf), 'ulong_ptr', 128, 'ulong_ptr*', 0)

For $row = 0 To 7
    Local $hex = "  +" & Hex($row*16, 2) & ": "
    For $col = 1 To 16
        $hex &= Hex(DllStructGetData($buf, 1, $row*16+$col), 2) & " "
    Next
    ConsoleWrite($hex & @CRLF)
Next

; Also check if MapIsLoaded is at the right address
ConsoleWrite(@CRLF & "MapIsLoaded label: 0x" & Hex(Int(GetLabel('MapIsLoaded'))) & @CRLF)
ConsoleWrite("MapIsLoaded value: " & MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword') & @CRLF)
ConsoleWrite("RenderCmdPtr label: 0x" & Hex(Int(GetLabel('RenderCmdPtr'))) & @CRLF)
ConsoleWrite("QueueCounter label: 0x" & Hex(Int(GetLabel('QueueCounter'))) & @CRLF)
ConsoleWrite("QueueBase label: 0x" & Hex(Int(GetLabel('QueueBase'))) & @CRLF)
