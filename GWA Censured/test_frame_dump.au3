#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Frame Tree Explorer ===" & @CRLF)

; Connect to a running client
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("No clients" & @CRLF)
    Exit
EndIf

SelectClient(1)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected to: " & $game_clients[1][3] & @CRLF)

Local $processHandle = GetProcessHandle()

; FrameArray is a GW::Array<Frame*> — buffer ptr, size, capacity
; The label 'FrameArray' points to the array base address
Local $frameArrayAddr = GetLabel('FrameArray')
ConsoleWrite("FrameArray addr: 0x" & Hex($frameArrayAddr) & @CRLF)

; Read the array header: buffer_ptr (4 bytes), size (4 bytes), capacity (4 bytes)
Local $bufferPtr = MemoryRead($processHandle, $frameArrayAddr, 'dword')
Local $arraySize = MemoryRead($processHandle, $frameArrayAddr + 4, 'dword')
Local $arrayCap = MemoryRead($processHandle, $frameArrayAddr + 8, 'dword')
ConsoleWrite("Frame array: buffer=0x" & Hex($bufferPtr) & " size=" & $arraySize & " cap=" & $arrayCap & @CRLF)

If $arraySize > 10000 Or $arraySize <= 0 Then
    ConsoleWrite("Invalid array size, trying direct pointer interpretation..." & @CRLF)
    ; Maybe FrameArray IS the buffer pointer, not the array struct
    ; Read the first few entries as pointers
    For $i = 0 To 4
        Local $fptr = MemoryRead($processHandle, $frameArrayAddr + ($i * 4), 'dword')
        ConsoleWrite("  [" & $i & "] = 0x" & Hex($fptr) & @CRLF)
    Next
    Exit
EndIf

; Read first 20 frame pointers and dump their key fields
ConsoleWrite(@CRLF & "=== First 20 frames ===" & @CRLF)
Local $maxFrames = 20
If $arraySize < $maxFrames Then $maxFrames = $arraySize

For $i = 0 To $maxFrames - 1
    ; Each entry is a pointer to a Frame struct
    Local $framePtr = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
    If $framePtr = 0 Or $framePtr < 0x10000 Then ContinueLoop

    ; Read key Frame fields
    ; 0xB8 = child_offset_id
    ; 0xBC = frame_id
    ; 0x128 = relation (FrameRelation)
    ; 0x18C = frame_state (with bitfield for visibility)
    Local $childOffsetId = MemoryRead($processHandle, $framePtr + 0xB8, 'dword')
    Local $frameId = MemoryRead($processHandle, $framePtr + 0xBC, 'dword')
    Local $frameState = MemoryRead($processHandle, $framePtr + 0x18C, 'dword')
    Local $isCreated = BitAND($frameState, 0x4) <> 0
    Local $isHidden = BitAND($frameState, 0x200) <> 0

    ; Try to find where labels might be stored
    ; The CreateUIComponent has component_label as the last parameter
    ; Labels might be stored as wchar_t* somewhere in the Frame
    ; Let me check several likely offsets for a string pointer
    Local $labelStr = ""
    ; Try reading wchar strings from various offsets that might be label pointers
    Local $labelOffsets[] = [0x1B4, 0x1B8, 0x1BC, 0x1C0, 0x1C4]
    For $off In $labelOffsets
        Local $strPtr = MemoryRead($processHandle, $framePtr + $off, 'dword')
        If $strPtr > 0x10000 And $strPtr < 0x7FFFFFFF Then
            Local $testStr = MemoryRead($processHandle, $strPtr, 'wchar[32]')
            If StringLen($testStr) > 0 And StringLen($testStr) < 30 Then
                $labelStr = "[0x" & Hex($off, 2) & "]=" & $testStr
                ExitLoop
            EndIf
        EndIf
    Next

    ConsoleWrite("  Frame[" & $i & "] ptr=0x" & Hex($framePtr) & " id=" & $frameId & _
        " child_off=" & $childOffsetId & " state=0x" & Hex($frameState) & _
        " created=" & $isCreated & " hidden=" & $isHidden & " " & $labelStr & @CRLF)
Next

ConsoleWrite(@CRLF & "=== DONE ===" & @CRLF)
