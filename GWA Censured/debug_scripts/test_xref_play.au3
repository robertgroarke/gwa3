#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Find code references to L'Play' ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)

Local $processHandle = GetProcessHandle()

; L"Play" is at 0x00BA89B0
; Search .text section for references to this address (little-endian: B0 89 BA 00)
Local $targetAddr = 0x00BA89B0
Local $targetLE = Binary("0xB089BA00")

Local $textStart = $pe_sections_ranges[0][0]
Local $textEnd = $pe_sections_ranges[0][1]
Local $textSize = $textEnd - $textStart
ConsoleWrite("Scanning .text (" & $textSize & " bytes) for refs to 0x" & Hex($targetAddr) & @CRLF)

; Read in 256KB chunks
Local $chunkSize = 262144
Local $refs = 0

For $offset = 0 To $textSize - 4 Step $chunkSize
    Local $readSize = $chunkSize + 4
    If $offset + $readSize > $textSize Then $readSize = $textSize - $offset

    Local $chunk = DllStructCreate('byte[' & $readSize & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
        'ptr', DllStructGetPtr($chunk), 'ulong_ptr', $readSize, 'ulong_ptr*', 0)

    For $j = 1 To $readSize - 3
        If DllStructGetData($chunk, 1, $j) = 0xB0 And _
           DllStructGetData($chunk, 1, $j+1) = 0x89 And _
           DllStructGetData($chunk, 1, $j+2) = 0xBA And _
           DllStructGetData($chunk, 1, $j+3) = 0x00 Then

            Local $codeAddr = $textStart + $offset + ($j - 1)
            ; Read surrounding context (the instruction that references this address)
            Local $ctx = DllStructCreate('byte[16]')
            DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
                'handle', $processHandle, 'ptr', Ptr($codeAddr - 4), _
                'ptr', DllStructGetPtr($ctx), 'ulong_ptr', 16, 'ulong_ptr*', 0)

            Local $ctxHex = ""
            For $b = 1 To 16
                $ctxHex &= Hex(DllStructGetData($ctx, 1, $b), 2) & " "
            Next

            ConsoleWrite("  XREF at 0x" & Hex($codeAddr) & " context: " & $ctxHex & @CRLF)
            $refs += 1
        EndIf
    Next
Next

ConsoleWrite("Found " & $refs & " code references to L'Play'" & @CRLF)

; Also search for references to L"Yes" at 0x00AA440A
ConsoleWrite(@CRLF & "=== References to L'Yes' (0x00AA440A) ===" & @CRLF)
For $offset = 0 To $textSize - 4 Step $chunkSize
    Local $readSize2 = $chunkSize + 4
    If $offset + $readSize2 > $textSize Then $readSize2 = $textSize - $offset

    Local $chunk2 = DllStructCreate('byte[' & $readSize2 & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
        'ptr', DllStructGetPtr($chunk2), 'ulong_ptr', $readSize2, 'ulong_ptr*', 0)

    For $j = 1 To $readSize2 - 3
        If DllStructGetData($chunk2, 1, $j) = 0x0A And _
           DllStructGetData($chunk2, 1, $j+1) = 0x44 And _
           DllStructGetData($chunk2, 1, $j+2) = 0xAA And _
           DllStructGetData($chunk2, 1, $j+3) = 0x00 Then
            Local $codeAddr2 = $textStart + $offset + ($j - 1)
            ConsoleWrite("  XREF at 0x" & Hex($codeAddr2) & @CRLF)
        EndIf
    Next
Next

ConsoleWrite("=== DONE ===" & @CRLF)
