#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Find SendFrameUIMessage (proper resolution) ===" & @CRLF)

; Use the existing GW process (PID 15440)
ScanAndUpdateGameClients()
SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected" & @CRLF)

Local $processHandle = GetProcessHandle()
Local $textStart = $pe_sections_ranges[0][0]
Local $textEnd = $pe_sections_ranges[0][1]
Local $textSize = $textEnd - $textStart

; Search for ALL occurrences of 83 C1 DC E8 in .text
ConsoleWrite("Scanning for '83 C1 DC E8' in .text..." & @CRLF)
Local $chunkSize = 65536
Local $matches = 0

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

            Local $matchAddr = $textStart + $offset + ($j - 1)

            ; The E8 is at matchAddr + 3. Resolve the call target.
            Local $callAddr = $matchAddr + 3
            Local $relBuf = DllStructCreate('int')
            DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
                'handle', $processHandle, 'ptr', Ptr($callAddr + 1), _
                'ptr', DllStructGetPtr($relBuf), 'ulong_ptr', 4, 'ulong_ptr*', 0)
            Local $rel = DllStructGetData($relBuf, 1)
            Local $targetFunc = Int($callAddr) + 5 + $rel

            ; Read context (8 bytes before and after)
            Local $ctx = DllStructCreate('byte[24]')
            DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
                'handle', $processHandle, 'ptr', Ptr($matchAddr - 8), _
                'ptr', DllStructGetPtr($ctx), 'ulong_ptr', 24, 'ulong_ptr*', 0)
            Local $ctxHex = ""
            For $b = 1 To 24
                $ctxHex &= Hex(DllStructGetData($ctx, 1, $b), 2) & " "
            Next

            ConsoleWrite("  Match #" & $matches & " at 0x" & Hex($matchAddr) & _
                " -> call target 0x" & Hex($targetFunc) & @CRLF)
            ConsoleWrite("    Context: " & $ctxHex & @CRLF)

            $matches += 1
        EndIf
    Next
Next

ConsoleWrite("Found " & $matches & " matches" & @CRLF)
ConsoleWrite("=== DONE ===" & @CRLF)
