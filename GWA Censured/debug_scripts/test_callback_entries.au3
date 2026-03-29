#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Dump Callback Array Entries ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Get Play button frame
Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
Local $fp = Int($result[0])
ConsoleWrite("Play frame: 0x" & Hex($fp) & @CRLF)

; Read callbacks array header (buffer, size, capacity)
Local $cbBuf = MemoryRead($ph, $fp + 0xA8, 'dword')
Local $cbSize = MemoryRead($ph, $fp + 0xAC, 'dword')
Local $cbCap = MemoryRead($ph, $fp + 0xB0, 'dword')
ConsoleWrite("Callbacks: buf=0x" & Hex($cbBuf) & " size=" & $cbSize & " cap=" & $cbCap & @CRLF)

; Each entry is 12 bytes (3 dwords) based on the function analysis
; The function checks: entry[0] != 0 AND entry[8] < 0 (signed)
ConsoleWrite(@CRLF & "Callback entries (12 bytes each):" & @CRLF)
For $i = 0 To $cbSize - 1
    Local $e0 = MemoryRead($ph, $cbBuf + ($i * 12), 'dword')
    Local $e4 = MemoryRead($ph, $cbBuf + ($i * 12) + 4, 'dword')
    Local $e8 = MemoryRead($ph, $cbBuf + ($i * 12) + 8, 'int')  ; signed!
    ConsoleWrite("  [" & $i & "] func=0x" & Hex($e0) & " data=0x" & Hex($e4) & " flag=" & $e8 & @CRLF)
    If $e0 <> 0 And $e8 < 0 Then
        ConsoleWrite("       ^^^ THIS entry would be dispatched (func!=0 AND flag<0)" & @CRLF)
    EndIf
Next

ConsoleWrite("=== DONE ===" & @CRLF)
