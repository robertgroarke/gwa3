#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== PreGame Login Trigger Test ===" & @CRLF)

; Use running GW
ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)

; Read PreGameContext
Local $pgPtr = MemoryRead($ph, $pre_game_address, 'dword')
ConsoleWrite("PreGameContext: 0x" & Hex($pgPtr) & @CRLF)

If $pgPtr <> 0 Then
    ; Dump more of PreGameContext to find login-related fields
    ConsoleWrite(@CRLF & "PreGameContext dump (first 256 bytes):" & @CRLF)
    For $off = 0 To 252 Step 4
        Local $val = MemoryRead($ph, $pgPtr + $off, 'dword')
        If $val <> 0 Then
            ConsoleWrite("  +0x" & Hex($off, 3) & ": 0x" & Hex($val, 8) & " (" & $val & ")" & @CRLF)
        EndIf
    Next

    ; Check index_1 and index_2 (from GWCA PreGameContext struct)
    ; +0x140 = index_1, +0x144 = index_2
    ConsoleWrite(@CRLF & "Index fields:" & @CRLF)
    ConsoleWrite("  index_1 (+0x140): " & MemoryRead($ph, $pgPtr + 0x140, 'dword') & @CRLF)
    ConsoleWrite("  index_2 (+0x144): " & MemoryRead($ph, $pgPtr + 0x144, 'dword') & @CRLF)
    ConsoleWrite("  chosen_char (+0x124): " & MemoryRead($ph, $pgPtr + 0x124, 'dword') & @CRLF)

    ; Try setting index_1 to trigger login
    ; The game might check index_1 > 0 to start loading
    ConsoleWrite(@CRLF & "Setting chosen_character_index = 0 (DISCOPANIC)..." & @CRLF)
    MemoryWrite($ph, $pgPtr + 0x124, 0, 'dword')

    ; Try setting index_1 = 1 (might mean "start login")
    ConsoleWrite("Setting index_1 = 1..." & @CRLF)
    MemoryWrite($ph, $pgPtr + 0x140, 1, 'dword')

    Sleep(5000)
    Local $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
    ConsoleWrite("StatusCode: " & $status & @CRLF)

    If $status = 0 Then
        ; Reset and try index_2
        MemoryWrite($ph, $pgPtr + 0x140, 0, 'dword')
        ConsoleWrite("Setting index_2 = 1..." & @CRLF)
        MemoryWrite($ph, $pgPtr + 0x144, 1, 'dword')
        Sleep(5000)
        $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
        ConsoleWrite("StatusCode: " & $status & @CRLF)
    EndIf
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
