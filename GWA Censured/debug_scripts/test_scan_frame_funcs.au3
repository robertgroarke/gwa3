#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Scan for Frame Functions ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected" & @CRLF)

Local $processHandle = GetProcessHandle()

; The BotsHub framework can scan for assertion strings
; Let's find frame-related functions via their assertion strings
; from P:\Code\Engine\Frame\ source files

; The framework's AddScanPattern with assertion already found 'FrameArray'
; via 'P:\Code\Engine\Frame\FrMsg.cpp' + 'frame'
; Let's look at what other assertion strings exist near it

; Read the raw scan result for FrameArray
Local $frameArrayScan = $scan_results['FrameArray']
ConsoleWrite("FrameArray scan result: 0x" & Hex($frameArrayScan) & @CRLF)

; The assertion string "frame" from FrMsg.cpp was found at some address
; Let's search for other assertion strings in the game binary
; that might lead us to GetFrameByLabel or ButtonClick

; Search for known assertion file paths related to frames
; We can read the game code section and search for these strings
Local $baseAddr = MemoryRead($processHandle, GetLabel('BasePointer'), 'dword')
ConsoleWrite("BasePointer: 0x" & Hex($baseAddr) & @CRLF)

; Let's try a different approach: use the existing framework to scan
; for new patterns. The FrameArray assertion scanner found the FrMsg.cpp assertion.
; Let's search for L"Play" (wide string) in the data sections

; Read PE sections to find the .rdata section (where strings are)
; Actually, let's just scan the process for the wchar string "Play"
; by reading memory in chunks

ConsoleWrite(@CRLF & "=== Searching for L'Play' in game memory ===" & @CRLF)

; The game's code+data is typically 0x401000 to ~0xFFF000
; Let's scan the .rdata section where string literals are stored
; PE sections are already parsed by the framework
ConsoleWrite("PE .text: 0x" & Hex($pe_sections_ranges[0][0]) & " - 0x" & Hex($pe_sections_ranges[0][1]) & @CRLF)
ConsoleWrite("PE .rdata: 0x" & Hex($pe_sections_ranges[1][0]) & " - 0x" & Hex($pe_sections_ranges[1][1]) & @CRLF)
ConsoleWrite("PE .data: 0x" & Hex($pe_sections_ranges[2][0]) & " - 0x" & Hex($pe_sections_ranges[2][1]) & @CRLF)

; Search .rdata for the wchar string "Play" (50 00 6C 00 61 00 79 00 00 00)
Local $searchBytes = Binary("0x500006C006100790000") ; L"Play\0"
; Actually, let's search byte by byte
; L"Play" = 0x50,0x00,0x6C,0x00,0x61,0x00,0x79,0x00,0x00,0x00

Local $rdataStart = $pe_sections_ranges[1][0]
Local $rdataEnd = $pe_sections_ranges[1][1]
Local $rdataSize = $rdataEnd - $rdataStart

ConsoleWrite("Scanning .rdata (" & $rdataSize & " bytes) for L'Play'..." & @CRLF)

; Read in 64KB chunks
Local $chunkSize = 65536
Local $found = 0
For $offset = 0 To $rdataSize - 10 Step $chunkSize
    Local $readSize = $chunkSize + 10
    If $offset + $readSize > $rdataSize Then $readSize = $rdataSize - $offset

    Local $chunk = DllStructCreate('byte[' & $readSize & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($rdataStart + $offset), _
        'ptr', DllStructGetPtr($chunk), 'ulong_ptr', $readSize, 'ulong_ptr*', 0)

    ; Search for "Play" wchar pattern: 50 00 6C 00 61 00 79 00 00 00
    For $j = 1 To $readSize - 9
        If DllStructGetData($chunk, 1, $j) = 0x50 And _
           DllStructGetData($chunk, 1, $j+1) = 0x00 And _
           DllStructGetData($chunk, 1, $j+2) = 0x6C And _
           DllStructGetData($chunk, 1, $j+3) = 0x00 And _
           DllStructGetData($chunk, 1, $j+4) = 0x61 And _
           DllStructGetData($chunk, 1, $j+5) = 0x00 And _
           DllStructGetData($chunk, 1, $j+6) = 0x79 And _
           DllStructGetData($chunk, 1, $j+7) = 0x00 And _
           DllStructGetData($chunk, 1, $j+8) = 0x00 And _
           DllStructGetData($chunk, 1, $j+9) = 0x00 Then

            Local $addr = $rdataStart + $offset + ($j - 1)
            ConsoleWrite('  Found L"Play" at 0x' & Hex($addr) & @CRLF)
            $found += 1
        EndIf
    Next
Next

ConsoleWrite("Found " & $found & ' instances of L"Play"' & @CRLF)

; Also search for "Yes" and "No" (reconnect dialog buttons)
ConsoleWrite(@CRLF & "=== Searching for L'Yes' and L'No' ===" & @CRLF)
For $offset = 0 To $rdataSize - 8 Step $chunkSize
    Local $readSize2 = $chunkSize + 8
    If $offset + $readSize2 > $rdataSize Then $readSize2 = $rdataSize - $offset

    Local $chunk2 = DllStructCreate('byte[' & $readSize2 & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($rdataStart + $offset), _
        'ptr', DllStructGetPtr($chunk2), 'ulong_ptr', $readSize2, 'ulong_ptr*', 0)

    ; Search for "Yes" wchar: 59 00 65 00 73 00 00 00
    For $j = 1 To $readSize2 - 7
        If DllStructGetData($chunk2, 1, $j) = 0x59 And _
           DllStructGetData($chunk2, 1, $j+1) = 0x00 And _
           DllStructGetData($chunk2, 1, $j+2) = 0x65 And _
           DllStructGetData($chunk2, 1, $j+3) = 0x00 And _
           DllStructGetData($chunk2, 1, $j+4) = 0x73 And _
           DllStructGetData($chunk2, 1, $j+5) = 0x00 And _
           DllStructGetData($chunk2, 1, $j+6) = 0x00 And _
           DllStructGetData($chunk2, 1, $j+7) = 0x00 Then
            ConsoleWrite('  Found L"Yes" at 0x' & Hex($rdataStart + $offset + ($j - 1)) & @CRLF)
        EndIf
    Next
Next

ConsoleWrite("=== DONE ===" & @CRLF)
