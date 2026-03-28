#RequireAdmin
#include "lib\Froggy_Includes.au3"
ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()
Local $bp = MemoryRead($ph, GetLabel('BasePointer'), 'dword')
ConsoleWrite("BasePointer = 0x" & Hex($bp) & @CRLF)
If $bp <> 0 Then
    Local $bp1 = MemoryRead($ph, $bp, 'dword')
    ConsoleWrite("  [BasePointer] = 0x" & Hex($bp1) & @CRLF)
Else
    ConsoleWrite("BasePointer is NULL — command queue won't process at char select!" & @CRLF)
EndIf
