#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Trace SendFrameUIMsg Call Site ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()

; The call site in the game is at 0x007972CC (E8 instruction)
; The full context showed: 8D 45 EC 6A 00 50 6A 2B 83 C1 DC E8 FF 13 00 00
; Let's read 128 bytes BEFORE the call to see the full function

Local $callSite = 0x007972CC  ; address in the game for THIS instance
; Actually the address varies per instance due to ASLR/relocation
; Let me re-find it
ConsoleWrite("Searching for call site pattern in game..." & @CRLF)

Local $textStart = $pe_sections_ranges[0][0]
Local $textEnd = $pe_sections_ranges[0][1]
Local $textSize = $textEnd - $textStart

Local $chunkSize = 65536
Local $foundAddr = 0

For $offset = 0 To $textSize - 8 Step $chunkSize
    Local $readSize = $chunkSize + 8
    If $offset + $readSize > $textSize Then $readSize = $textSize - $offset

    Local $chunk = DllStructCreate('byte[' & $readSize & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
        'ptr', DllStructGetPtr($chunk), 'ulong_ptr', $readSize, 'ulong_ptr*', 0)

    For $j = 1 To $readSize - 7
        If DllStructGetData($chunk, 1, $j) = 0x83 And _
           DllStructGetData($chunk, 1, $j+1) = 0xC1 And _
           DllStructGetData($chunk, 1, $j+2) = 0xDC And _
           DllStructGetData($chunk, 1, $j+3) = 0xE8 Then
            $foundAddr = $textStart + $offset + ($j - 1)
            ExitLoop 2
        EndIf
    Next
Next

If $foundAddr = 0 Then
    ConsoleWrite("Pattern not found!" & @CRLF)
    Exit
EndIf

ConsoleWrite("Call site at 0x" & Hex($foundAddr) & @CRLF)

; Read 128 bytes before and 32 after the call site
Local $dumpStart = $foundAddr - 128
Local $dumpBuf = DllStructCreate('byte[192]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($dumpStart), _
    'ptr', DllStructGetPtr($dumpBuf), 'ulong_ptr', 192, 'ulong_ptr*', 0)

ConsoleWrite(@CRLF & "=== Code around call site ===" & @CRLF)
For $row = 0 To 11
    Local $addr = $dumpStart + $row * 16
    Local $hex = Hex($addr, 8) & ": "
    For $col = 1 To 16
        $hex &= Hex(DllStructGetData($dumpBuf, 1, $row * 16 + $col), 2) & " "
    Next
    ; Mark the call site
    If $addr <= $foundAddr And $foundAddr < $addr + 16 Then $hex &= " <-- PATTERN HERE"
    ConsoleWrite("  " & $hex & @CRLF)
Next

; Also find the function start (look for common prologue: 55 8B EC or CC CC)
ConsoleWrite(@CRLF & "=== Looking for function start ===" & @CRLF)
For $back = 128 To 1 Step -1
    Local $b1 = DllStructGetData($dumpBuf, 1, 128 - $back + 1)
    Local $b2 = DllStructGetData($dumpBuf, 1, 128 - $back + 2)
    Local $b3 = DllStructGetData($dumpBuf, 1, 128 - $back + 3)
    If $b1 = 0x55 And $b2 = 0x8B And $b3 = 0xEC Then
        ConsoleWrite("  Function prologue (push ebp; mov ebp,esp) at -" & $back & " bytes = 0x" & _
            Hex($foundAddr - $back) & @CRLF)
    EndIf
Next

ConsoleWrite("=== DONE ===" & @CRLF)
