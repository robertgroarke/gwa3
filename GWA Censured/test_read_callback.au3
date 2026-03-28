#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Read Play Button Frame Callback ===" & @CRLF)

; Launch if needed
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    GWLauncher_LaunchAccount($accounts, $idx)
    ConsoleWrite("Waiting 20s..." & @CRLF)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf

SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected" & @CRLF)

Local $processHandle = GetProcessHandle()

; Find Play button frame
Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $result[0] = 0 Then
    ConsoleWrite("Play button not found" & @CRLF)
    Exit
EndIf

Local $framePtr = Int($result[0])
Local $frameId = $result[1]
ConsoleWrite("Play frame: ptr=0x" & Hex($framePtr) & " id=" & $frameId & @CRLF)

; Dump key Frame fields
ConsoleWrite(@CRLF & "=== Frame structure dump ===" & @CRLF)
Local $offsets[][2] = [ _
    [0x00, "field1"], _
    [0x04, "field2"], _
    [0x08, "frame_layout"], _
    [0x18, "visibility_flags"], _
    [0x20, "type"], _
    [0x24, "template_type"], _
    [0xA8, "frame_callbacks.buffer"], _
    [0xAC, "frame_callbacks.size"], _
    [0xB0, "frame_callbacks.capacity"], _
    [0xB8, "child_offset_id"], _
    [0xBC, "frame_id"], _
    [0xC0, "field40"], _
    [0xC4, "field41 (tooltip/context?)"], _
    [0x128, "relation.parent"], _
    [0x134, "relation.hash_id"], _
    [0x18C, "frame_state"] _
]

For $i = 0 To UBound($offsets) - 1
    Local $off = $offsets[$i][0]
    Local $name = $offsets[$i][1]
    Local $val = MemoryRead($processHandle, $framePtr + $off, 'dword')
    ConsoleWrite("  +0x" & Hex($off, 3) & " " & $name & " = 0x" & Hex($val) & " (" & $val & ")" & @CRLF)
Next

; Read the frame_callbacks array
Local $cbBuffer = MemoryRead($processHandle, $framePtr + 0xA8, 'dword')
Local $cbSize = MemoryRead($processHandle, $framePtr + 0xAC, 'dword')
ConsoleWrite(@CRLF & "Callbacks: " & $cbSize & " entries at 0x" & Hex($cbBuffer) & @CRLF)

If $cbSize > 0 And $cbSize < 100 And $cbBuffer > 0x10000 Then
    ; Each callback entry is a function pointer (FrameInteractionCallback)
    ; typedef void(__cdecl* UIInteractionCallback)(InteractionMessage*, void*, void*)
    For $c = 0 To $cbSize - 1
        Local $cbAddr = MemoryRead($processHandle, $cbBuffer + ($c * 4), 'dword')
        ConsoleWrite("  callback[" & $c & "] = 0x" & Hex($cbAddr) & @CRLF)
    Next
EndIf

; Also read what's at offset 0xC4 (might be a context pointer used by SendFrameUIMessage)
Local $field41 = MemoryRead($processHandle, $framePtr + 0xC4, 'dword')
ConsoleWrite(@CRLF & "field41 (0xC4) = 0x" & Hex($field41) & @CRLF)
If $field41 > 0x10000 Then
    ; Read what it points to
    For $off = 0 To 16 Step 4
        Local $v = MemoryRead($processHandle, $field41 + $off, 'dword')
        ConsoleWrite("  [0x" & Hex($field41 + $off) & "] = 0x" & Hex($v) & @CRLF)
    Next
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
