#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
Local $fp = Int($pf[0])
ConsoleWrite("Frame: 0x" & Hex($fp) & @CRLF)

; Get context
Local $ctx = _GetFrameContext($fp)
ConsoleWrite("Context: 0x" & Hex($ctx) & @CRLF)

If $ctx <> 0 Then
    ; Read context+0xA8 area (what ECX points to after adding 0xA8)
    Local $thisPtr = $ctx + 0xA8
    ConsoleWrite("ECX would be: 0x" & Hex($thisPtr) & @CRLF)

    ; Read first few dwords at the this pointer
    ConsoleWrite("Memory at context+0xA8:" & @CRLF)
    For $off = 0 To 20 Step 4
        Local $val = MemoryRead($ph, $thisPtr + $off, 'dword')
        ConsoleWrite("  +0x" & Hex($off, 2) & ": 0x" & Hex($val) & @CRLF)
    Next

    ; Compare with frame+0xA8 (what we were using before)
    ConsoleWrite(@CRLF & "Memory at frame+0xA8 (old incorrect approach):" & @CRLF)
    For $off = 0 To 20 Step 4
        Local $val2 = MemoryRead($ph, $fp + 0xA8 + $off, 'dword')
        ConsoleWrite("  +0x" & Hex($off, 2) & ": 0x" & Hex($val2) & @CRLF)
    Next
EndIf
