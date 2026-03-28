#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Trigger ClickFrameButton to write shellcode
Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $pf[0] = 0 Then Exit

ConsoleWrite("Calling ClickFrameButton..." & @CRLF)
ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
Sleep(1000)

; Read shellcode bytes
If $g_FrameClick_ShellcodeAddr <> 0 Then
    Local $buf = DllStructCreate('byte[24]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $ph, 'ptr', Ptr($g_FrameClick_ShellcodeAddr), _
        'ptr', DllStructGetPtr($buf), 'ulong_ptr', 24, 'ulong_ptr*', 0)
    Local $hex = ""
    For $b = 1 To 20
        $hex &= Hex(DllStructGetData($buf, 1, $b), 2) & " "
    Next
    ConsoleWrite("Shellcode bytes: " & $hex & @CRLF)

    ; Decode: B9 <ECX> 6A 00 68 <dataAddr> 6A 31 E8 <rel32> C3
    ; ECX value (bytes 2-5)
    Local $ecxVal = 0
    For $b = 2 To 5
        $ecxVal += DllStructGetData($buf, 1, $b) * (256 ^ ($b - 2))
    Next
    ConsoleWrite("ECX (thisPtr): 0x" & Hex($ecxVal) & @CRLF)

    ; Call target (bytes 14-17 = rel32)
    Local $callOff = $g_FrameClick_ShellcodeAddr + 13  ; position of E8 byte
    Local $rel32 = 0
    For $b = 14 To 17
        $rel32 += DllStructGetData($buf, 1, $b) * (256 ^ ($b - 14))
    Next
    If $rel32 >= 0x80000000 Then $rel32 -= 0x100000000
    Local $callTarget = $callOff + 5 + $rel32
    ConsoleWrite("Call target: 0x" & Hex($callTarget) & @CRLF)
    ConsoleWrite("Expected SendFrameUIMsg: 0x" & Hex(Int(GetLabel('SendFrameUIMsg'))) & @CRLF)
    If $callTarget = Int(GetLabel('SendFrameUIMsg')) Then
        ConsoleWrite("MATCH!" & @CRLF)
    Else
        ConsoleWrite("MISMATCH! Call target wrong by " & ($callTarget - Int(GetLabel('SendFrameUIMsg'))) & " bytes" & @CRLF)
    EndIf
EndIf
