#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Disassemble Play button references ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)

Local $processHandle = GetProcessHandle()

; XREF 1: 0x00760290
; Read 64 bytes before and 64 bytes after
ConsoleWrite(@CRLF & "=== XREF 1: 0x00760290 ===" & @CRLF)
Local $addr1 = 0x00760290
Local $buf1 = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($addr1 - 32), _
    'ptr', DllStructGetPtr($buf1), 'ulong_ptr', 128, 'ulong_ptr*', 0)

For $row = 0 To 7
    Local $hexLine = "  " & Hex($addr1 - 32 + $row * 16, 8) & ": "
    For $col = 1 To 16
        $hexLine &= Hex(DllStructGetData($buf1, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hexLine & @CRLF)
Next

; XREF 2: 0x009D9370
ConsoleWrite(@CRLF & "=== XREF 2: 0x009D9370 ===" & @CRLF)
Local $addr2 = 0x009D9370
Local $buf2 = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($addr2 - 32), _
    'ptr', DllStructGetPtr($buf2), 'ulong_ptr', 128, 'ulong_ptr*', 0)

For $row = 0 To 7
    Local $hexLine2 = "  " & Hex($addr2 - 32 + $row * 16, 8) & ": "
    For $col = 1 To 16
        $hexLine2 &= Hex(DllStructGetData($buf2, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hexLine2 & @CRLF)
Next

; The call at XREF 1 is: E8 13 F6 FF FF (relative call)
; Target = 0x00760290 + 5 + 8 + FFFFFFF613 ... wait let me compute properly
; At 0x00760294 (after the push): 51 53 8B C8 E8 13 F6 FF
; The E8 is at 0x00760298: E8 13 F6 FF FF
; Call target = 0x00760298 + 5 + 0xFFFFF613 = 0x00760298 + 5 - 0x9ED = 0x0075F8B0
ConsoleWrite(@CRLF & "=== Call target from XREF 1 ===" & @CRLF)
; Read the actual bytes
Local $callBytes = DllStructCreate('byte[5]')
Local $callAddr = $addr1 + 8  ; push + more instructions before call
; Actually let me find the E8 byte precisely
For $i = 1 To 96
    If DllStructGetData($buf1, 1, 32 + $i) = 0xE8 Then
        Local $codeOffset = $addr1 + ($i - 1)
        Local $rel = 0
        For $b = 1 To 4
            $rel += DllStructGetData($buf1, 1, 32 + $i + $b) * (256 ^ ($b - 1))
        Next
        If $rel >= 0x80000000 Then $rel -= 0x100000000
        Local $target = $codeOffset + 5 + $rel
        ConsoleWrite("  E8 call at 0x" & Hex($codeOffset) & " -> target 0x" & Hex($target) & @CRLF)
    EndIf
Next

ConsoleWrite("=== DONE ===" & @CRLF)
