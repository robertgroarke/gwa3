#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

Local $func = Int(GetLabel('SendFrameUIMsg'))
Local $inner = $func + 0x70  ; inner handler called at +0x61 with E8 0A 00 00 00

ConsoleWrite("Inner handler at 0x" & Hex($inner) & @CRLF)

Local $buf = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($inner), _
    'ptr', DllStructGetPtr($buf), 'ulong_ptr', 128, 'ulong_ptr*', 0)

For $row = 0 To 7
    Local $hex = "  +" & Hex($row * 16, 2) & ": "
    For $col = 1 To 16
        $hex &= Hex(DllStructGetData($buf, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hex & @CRLF)
Next
