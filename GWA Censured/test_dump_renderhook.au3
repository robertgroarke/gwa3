#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Find RenderingModProc by following the JMP at RenderingMod
Local $rendMod = Int(GetLabel('RenderingMod'))
ConsoleWrite("RenderingMod hook at 0x" & Hex($rendMod) & @CRLF)

; Read JMP at RenderingMod
Local $relBuf = DllStructCreate('int')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($rendMod + 1), _
    'ptr', DllStructGetPtr($relBuf), 'ulong_ptr', 4, 'ulong_ptr*', 0)
Local $rendProc = $rendMod + 5 + DllStructGetData($relBuf, 1)
ConsoleWrite("RenderingModProc at 0x" & Hex($rendProc) & @CRLF)

; Dump 128 bytes of RenderingModProc
Local $buf = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($rendProc), _
    'ptr', DllStructGetPtr($buf), 'ulong_ptr', 128, 'ulong_ptr*', 0)

ConsoleWrite("RenderingModProc bytes:" & @CRLF)
For $row = 0 To 7
    Local $hex = "  +" & Hex($row*16, 2) & ": "
    For $col = 1 To 16
        $hex &= Hex(DllStructGetData($buf, 1, $row*16+$col), 2) & " "
    Next
    ConsoleWrite($hex & @CRLF)
Next

; Look for FF D3 (call ebx) in the bytes
For $i = 1 To 126
    If DllStructGetData($buf, 1, $i) = 0xFF And DllStructGetData($buf, 1, $i+1) = 0xD3 Then
        ConsoleWrite("  Found FFD3 (call ebx) at +" & ($i-1) & @CRLF)
    EndIf
Next
