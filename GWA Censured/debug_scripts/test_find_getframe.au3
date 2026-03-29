#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Find GetFrameByLabel function ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)

Local $processHandle = GetProcessHandle()

; From old GWCA UIMgr.cpp:
; FrameCache_addr = Scanner::Find("\x68\x00\x10\x00\x00\x8B\x1C\x98\x8D", "xxxxxxxxx", -4);
; This finds the frame cache array pointer

; Let's search for this pattern in the .text section
Local $textStart = $pe_sections_ranges[0][0]
Local $textEnd = $pe_sections_ranges[0][1]
Local $textSize = $textEnd - $textStart

; Pattern: 68 00 10 00 00 8B 1C 98 8D
ConsoleWrite("Searching for FrameCache pattern..." & @CRLF)

Local $chunkSize = 262144
For $offset = 0 To $textSize - 9 Step $chunkSize
    Local $readSize = $chunkSize + 9
    If $offset + $readSize > $textSize Then $readSize = $textSize - $offset

    Local $chunk = DllStructCreate('byte[' & $readSize & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
        'ptr', DllStructGetPtr($chunk), 'ulong_ptr', $readSize, 'ulong_ptr*', 0)

    For $j = 1 To $readSize - 8
        If DllStructGetData($chunk, 1, $j) = 0x68 And _
           DllStructGetData($chunk, 1, $j+1) = 0x00 And _
           DllStructGetData($chunk, 1, $j+2) = 0x10 And _
           DllStructGetData($chunk, 1, $j+3) = 0x00 And _
           DllStructGetData($chunk, 1, $j+4) = 0x00 And _
           DllStructGetData($chunk, 1, $j+5) = 0x8B And _
           DllStructGetData($chunk, 1, $j+6) = 0x1C And _
           DllStructGetData($chunk, 1, $j+7) = 0x98 And _
           DllStructGetData($chunk, 1, $j+8) = 0x8D Then
            Local $matchAddr = $textStart + $offset + ($j - 1)
            ; FrameCache_addr is at match - 4
            Local $frameCacheAddr = $matchAddr - 4
            ; Read the dword at frameCacheAddr
            Local $frameCacheVal = MemoryRead($processHandle, $frameCacheAddr, 'dword')
            ConsoleWrite("  FrameCache pattern at 0x" & Hex($matchAddr) & " -> addr=0x" & Hex($frameCacheAddr) & " val=0x" & Hex($frameCacheVal) & @CRLF)

            ; Read more context
            Local $ctx = DllStructCreate('byte[32]')
            DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
                'handle', $processHandle, 'ptr', Ptr($matchAddr - 8), _
                'ptr', DllStructGetPtr($ctx), 'ulong_ptr', 32, 'ulong_ptr*', 0)
            Local $ctxHex = ""
            For $b = 1 To 32
                $ctxHex &= Hex(DllStructGetData($ctx, 1, $b), 2) & " "
            Next
            ConsoleWrite("  Context: " & $ctxHex & @CRLF)
        EndIf
    Next
Next

; Also search for the CreateUIComponent pattern from old GWCA:
; "\x33\xd2\x89\x45\x08\xb9\xac\x01\x00\x00"
ConsoleWrite(@CRLF & "Searching for CreateUIComponent pattern..." & @CRLF)
For $offset = 0 To $textSize - 10 Step $chunkSize
    Local $readSize2 = $chunkSize + 10
    If $offset + $readSize2 > $textSize Then $readSize2 = $textSize - $offset

    Local $chunk2 = DllStructCreate('byte[' & $readSize2 & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
        'ptr', DllStructGetPtr($chunk2), 'ulong_ptr', $readSize2, 'ulong_ptr*', 0)

    For $j = 1 To $readSize2 - 9
        If DllStructGetData($chunk2, 1, $j) = 0x33 And _
           DllStructGetData($chunk2, 1, $j+1) = 0xD2 And _
           DllStructGetData($chunk2, 1, $j+2) = 0x89 And _
           DllStructGetData($chunk2, 1, $j+3) = 0x45 And _
           DllStructGetData($chunk2, 1, $j+4) = 0x08 And _
           DllStructGetData($chunk2, 1, $j+5) = 0xB9 Then
            ; Check if next bytes are close to 0x1C8 (Frame size)
            Local $frameSize = DllStructGetData($chunk2, 1, $j+6) + _
                DllStructGetData($chunk2, 1, $j+7) * 256
            Local $funcAddr = $textStart + $offset + ($j - 1) - 0x27
            ConsoleWrite("  CreateUIComponent candidate at 0x" & Hex($funcAddr) & _
                " (frame_size=0x" & Hex($frameSize) & ")" & @CRLF)
        EndIf
    Next
Next

ConsoleWrite("=== DONE ===" & @CRLF)
