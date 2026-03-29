#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Trace Frame Functions ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)

Local $processHandle = GetProcessHandle()

; From the FrameCache context at 0x007A5F2D:
; A1 C8 54 D4 00        mov eax, [0x00D454C8]   ; load FrameCache ptr
; 68 00 10 00 00        push 0x1000              ; array size or something
; 8B 1C 98              mov ebx, [eax+ebx*4]     ; index into frame array
; 8D ...                lea ...
; 8B 8C 01 00 00        mov ecx, [ecx+eax+0]     ; offset into frame
; E8 FC 86 01 00        call 0x007BE638           ; some function

; Let me compute the call target more carefully
; The E8 is at offset +22 from the context start (0x007A5F2D + 22 = 0x007A5F43)
; Actually let me just read from the known position
Local $callAddr = 0x007A5F43  ; approximate
; Read bytes around to find the E8
Local $scan = DllStructCreate('byte[64]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr(0x007A5F2D), _
    'ptr', DllStructGetPtr($scan), 'ulong_ptr', 64, 'ulong_ptr*', 0)

; Find E8 bytes and compute targets
ConsoleWrite("Bytes at 0x007A5F2D:" & @CRLF)
Local $hexAll = ""
For $i = 1 To 64
    $hexAll &= Hex(DllStructGetData($scan, 1, $i), 2) & " "
    If Mod($i, 16) = 0 Then
        ConsoleWrite("  " & Hex(0x007A5F2D + $i - 16, 8) & ": " & $hexAll & @CRLF)
        $hexAll = ""
    EndIf
Next

; Find all E8 (call) instructions and compute targets
ConsoleWrite(@CRLF & "Call targets:" & @CRLF)
For $i = 1 To 60
    If DllStructGetData($scan, 1, $i) = 0xE8 Then
        Local $rel = 0
        For $b = 1 To 4
            $rel += DllStructGetData($scan, 1, $i + $b) * (256 ^ ($b - 1))
        Next
        If $rel >= 0x80000000 Then $rel -= 0x100000000
        Local $codePos = 0x007A5F2D + ($i - 1)
        Local $callTarget = $codePos + 5 + $rel
        ConsoleWrite("  E8 at 0x" & Hex($codePos) & " -> 0x" & Hex($callTarget) & @CRLF)
    EndIf
Next

; Now look at CreateUIComponent at 0x0079D300
; Read the first 128 bytes of this function
ConsoleWrite(@CRLF & "=== CreateUIComponent at 0x0079D300 ===" & @CRLF)
Local $cuic = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr(0x0079D300), _
    'ptr', DllStructGetPtr($cuic), 'ulong_ptr', 128, 'ulong_ptr*', 0)

For $row = 0 To 7
    Local $hexLine = "  " & Hex(0x0079D300 + $row * 16, 8) & ": "
    For $col = 1 To 16
        $hexLine &= Hex(DllStructGetData($cuic, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hexLine & @CRLF)
Next

; Find calls within CreateUIComponent
ConsoleWrite(@CRLF & "CreateUIComponent internal calls:" & @CRLF)
For $i = 1 To 124
    If DllStructGetData($cuic, 1, $i) = 0xE8 Then
        Local $rel2 = 0
        For $b = 1 To 4
            $rel2 += DllStructGetData($cuic, 1, $i + $b) * (256 ^ ($b - 1))
        Next
        If $rel2 >= 0x80000000 Then $rel2 -= 0x100000000
        Local $cp2 = 0x0079D300 + ($i - 1)
        Local $ct2 = $cp2 + 5 + $rel2
        ConsoleWrite("  call at 0x" & Hex($cp2) & " -> 0x" & Hex($ct2) & @CRLF)
    EndIf
Next

; Key: look for SendFrameUIMessage
; From old GWCA: SendUIMessage_Func pattern "\xE8\x00\x00\x00\x00\x5D\xC3\x89\x45\x08\x5D\xE9"
; Let's also check if our UIMessage scan result matches SendUIMessage
ConsoleWrite(@CRLF & "UIMessage (SendUIMessage) = 0x" & Hex($scan_results['UIMessage']) & @CRLF)

; The FrameArray assertion was from FrMsg.cpp with assertion text "frame"
; Let's look at what other functions are near the FrameArray scan result
ConsoleWrite("FrameArray scan result = 0x" & Hex($scan_results['FrameArray']) & @CRLF)

; Read 256 bytes around the FrameArray scan result for context
ConsoleWrite(@CRLF & "=== Code near FrameArray assertion ===" & @CRLF)
Local $faCtx = DllStructCreate('byte[256]')
Local $faAddr = $scan_results['FrameArray']
; Handle the case where faAddr might be a 64-bit value
If $faAddr > 0xFFFFFFFF Then
    $faAddr = BitAND($faAddr, 0xFFFFFFFF)
EndIf
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($faAddr - 64), _
    'ptr', DllStructGetPtr($faCtx), 'ulong_ptr', 256, 'ulong_ptr*', 0)

For $row = 0 To 15
    Local $hexLine2 = "  " & Hex($faAddr - 64 + $row * 16, 8) & ": "
    For $col = 1 To 16
        $hexLine2 &= Hex(DllStructGetData($faCtx, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hexLine2 & @CRLF)
Next

ConsoleWrite("=== DONE ===" & @CRLF)
