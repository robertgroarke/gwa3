#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Find Play Button Frame ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected to: " & $game_clients[1][3] & @CRLF)

Local $processHandle = GetProcessHandle()
Local $frameArrayAddr = GetLabel('FrameArray')

; The FrameArray is stored differently than expected
; Let me read it as a raw pointer first
ConsoleWrite("FrameArray label = 0x" & Hex($frameArrayAddr) & @CRLF)

; Read what's at FrameArray - it might be a pointer to the actual array
Local $val0 = MemoryRead($processHandle, $frameArrayAddr, 'dword')
Local $val1 = MemoryRead($processHandle, $frameArrayAddr + 4, 'dword')
Local $val2 = MemoryRead($processHandle, $frameArrayAddr + 8, 'dword')
ConsoleWrite("FrameArray[0]: 0x" & Hex($val0) & @CRLF)
ConsoleWrite("FrameArray[4]: 0x" & Hex($val1) & " (might be size)" & @CRLF)
ConsoleWrite("FrameArray[8]: 0x" & Hex($val2) & " (might be capacity)" & @CRLF)

; Scan through frames looking for one with "Play" label
; Frame struct is 0x1C8 bytes
; Try dumping raw bytes around offset where labels might be stored
; Let's check a few frames and read all potential string pointers

Local $bufferPtr = $val0
If $bufferPtr < 0x10000 Then
    ConsoleWrite("Buffer pointer invalid, trying FrameArray as direct array" & @CRLF)
    $bufferPtr = $frameArrayAddr
EndIf

; Read frame 0 and dump its contents to find label storage
ConsoleWrite(@CRLF & "=== Dumping Frame[1] structure ===" & @CRLF)
Local $fp = MemoryRead($processHandle, $bufferPtr + 4, 'dword')  ; Frame[1]
ConsoleWrite("Frame[1] ptr = 0x" & Hex($fp) & @CRLF)

If $fp > 0x10000 Then
    ; Read the full frame and look for string pointers
    ; A Frame is 0x1C8 bytes. Read it as dwords and check each for string pointers
    For $off = 0 To 0x1C4 Step 4
        Local $dw = MemoryRead($processHandle, $fp + $off, 'dword')
        ; Check if this looks like a valid pointer to a wchar string
        If $dw > 0x10000 And $dw < 0x7FFFFFFF Then
            Local $testStr = MemoryRead($processHandle, $dw, 'wchar[20]')
            If StringLen($testStr) >= 2 And StringLen($testStr) < 18 Then
                ; Check if it's printable ASCII in wide char
                Local $isPrintable = True
                For $ci = 1 To StringLen($testStr)
                    Local $ch = AscW(StringMid($testStr, $ci, 1))
                    If $ch < 32 Or $ch > 126 Then
                        $isPrintable = False
                        ExitLoop
                    EndIf
                Next
                If $isPrintable Then
                    ConsoleWrite("  +0x" & Hex($off, 3) & ": 0x" & Hex($dw, 8) & ' -> "' & $testStr & '"' & @CRLF)
                EndIf
            EndIf
        EndIf
    Next
EndIf

; Now scan ALL frames for any with "Play" in a string field
ConsoleWrite(@CRLF & "=== Scanning all frames for 'Play' label ===" & @CRLF)
Local $maxScan = $val1
If $maxScan > 3072 Then $maxScan = 3072

Local $foundPlay = False
For $i = 0 To $maxScan - 1
    Local $fptr = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
    If $fptr = 0 Or $fptr < 0x10000 Then ContinueLoop

    ; Check a few offsets that might contain the frame label
    ; Based on CreateUIComponent, the label is stored somewhere
    ; Try offsets: 0x1B4, 0x1B8, 0x1BC, 0x1C0, 0x1C4 (end of struct)
    ; Also try offsets in the 0x40-0x70 range
    Local $checkOffsets[] = [0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, _
        0x60, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x7C, _
        0x1B0, 0x1B4, 0x1B8, 0x1BC, 0x1C0, 0x1C4]

    For $off In $checkOffsets
        Local $strPtr = MemoryRead($processHandle, $fptr + $off, 'dword')
        If $strPtr > 0x10000 And $strPtr < 0x7FFFFFFF Then
            Local $str = MemoryRead($processHandle, $strPtr, 'wchar[20]')
            If StringInStr($str, "Play") Or StringInStr($str, "Yes") Or StringInStr($str, "No") Or _
               StringInStr($str, "Game") Or StringInStr($str, "Chat") Or StringInStr($str, "Party") Then
                Local $fid = MemoryRead($processHandle, $fptr + 0xBC, 'dword')
                Local $fstate = MemoryRead($processHandle, $fptr + 0x18C, 'dword')
                ConsoleWrite("  FOUND Frame[" & $i & "] id=" & $fid & " +0x" & Hex($off, 3) & ' -> "' & $str & _
                    '" state=0x' & Hex($fstate) & ' ptr=0x' & Hex($fptr) & @CRLF)
                If StringInStr($str, "Play") Then $foundPlay = True
            EndIf
        EndIf
    Next
Next

If Not $foundPlay Then
    ConsoleWrite("'Play' not found in checked offsets. Trying full struct scan on first 200 frames..." & @CRLF)
    For $i = 0 To 199
        Local $fptr2 = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
        If $fptr2 = 0 Or $fptr2 < 0x10000 Then ContinueLoop
        For $off = 0 To 0x1C4 Step 4
            Local $sp = MemoryRead($processHandle, $fptr2 + $off, 'dword')
            If $sp > 0x10000 And $sp < 0x7FFFFFFF Then
                Local $s = MemoryRead($processHandle, $sp, 'wchar[8]')
                If $s = "Play" Then
                    ConsoleWrite("  FOUND 'Play' at Frame[" & $i & "] +0x" & Hex($off, 3) & _
                        " ptr=0x" & Hex($fptr2) & @CRLF)
                    $foundPlay = True
                EndIf
            EndIf
        Next
        If $foundPlay Then ExitLoop
    Next
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
