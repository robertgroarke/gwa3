#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Read SavedIndex
Local $si = MemoryRead($ph, GetLabel('SavedIndex'), 'dword')
ConsoleWrite("SavedIndex = 0x" & Hex($si) & @CRLF)

If $si > 0x10000 Then
    ; Dump bytes at that address
    Local $buf = DllStructCreate('byte[16]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $ph, 'ptr', Ptr($si), _
        'ptr', DllStructGetPtr($buf), 'ulong_ptr', 16, 'ulong_ptr*', 0)
    Local $hex = ""
    For $b = 1 To 16
        $hex &= Hex(DllStructGetData($buf, 1, $b), 2) & " "
    Next
    ConsoleWrite("Bytes at shellcode: " & $hex & @CRLF)

    ; Check if flag address in shellcode matches
    ; Shellcode: C7 05 <flagAddr_LE> EF BE AD DE C3
    ; flagAddr is at bytes 3-6
    Local $scFlag = 0
    For $b = 3 To 6
        $scFlag += DllStructGetData($buf, 1, $b) * (256 ^ ($b - 3))
    Next
    ConsoleWrite("Flag addr in shellcode: 0x" & Hex($scFlag) & @CRLF)

    ; Read the flag
    Local $flag = MemoryRead($ph, $scFlag, 'dword')
    ConsoleWrite("Flag value: 0x" & Hex($flag) & @CRLF)
EndIf
