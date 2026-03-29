#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Reconnect Dialog Test ===" & @CRLF)

; Wait for GW client
ConsoleWrite("Waiting for GW client..." & @CRLF)
For $wait = 1 To 30
    ScanAndUpdateGameClients()
    If $game_clients[0][0] > 0 Then ExitLoop
    Sleep(2000)
Next

If $game_clients[0][0] = 0 Then
    ConsoleWrite("No GW clients found" & @CRLF)
    Exit
EndIf

SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()
Local $hWnd = $game_clients[$game_clients[0][0]][2]

ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)

; Take screenshot to see current state
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_check.png', $hWnd)
ConsoleWrite("Screenshot saved to tests/reconnect_check.png" & @CRLF)

; Check for reconnect dialog
ConsoleWrite("Checking for reconnect dialog..." & @CRLF)
Local $reconnect = IsReconnectDialogShowing()
ConsoleWrite("IsReconnectDialogShowing: " & $reconnect & @CRLF)

; Also scan for the YES/NO frame hashes directly
Local $yesResult = GetFrameByHash($FRAME_HASH_RECONNECT_YES)
Local $noResult = GetFrameByHash($FRAME_HASH_RECONNECT_NO)
ConsoleWrite("Reconnect YES button: ptr=0x" & Hex(Int($yesResult[0])) & " id=" & $yesResult[1] & @CRLF)
ConsoleWrite("Reconnect NO button: ptr=0x" & Hex(Int($noResult[0])) & " id=" & $noResult[1] & @CRLF)

; Dump ALL visible frames to find the reconnect dialog
ConsoleWrite(@CRLF & "--- Dumping all frames ---" & @CRLF)
Local $frameArrayPtr = MemoryRead($ph, GetLabel('FrameArray'), 'dword')
Local $frameCount = MemoryRead($ph, $frameArrayPtr, 'dword')
Local $frameArrayBase = MemoryRead($ph, $frameArrayPtr + 4, 'dword')
ConsoleWrite("FrameArray: count=" & $frameCount & " base=0x" & Hex($frameArrayBase) & @CRLF)

; Scan first 200 frames for visible ones with interesting hashes
Local $count = $frameCount
If $count > 200 Then $count = 200
For $i = 0 To $count - 1
    Local $fptr = $frameArrayBase + ($i * 4)
    Local $fp = MemoryRead($ph, $fptr, 'dword')
    If $fp = 0 Then ContinueLoop

    Local $state = MemoryRead($ph, $fp + 0xC0, 'dword')
    If BitAND($state, 0x20000) = 0 Then ContinueLoop ; not visible

    Local $hash = MemoryRead($ph, $fp + 0x134, 'dword')
    Local $fid = MemoryRead($ph, $fp + 0xBC, 'dword')
    ConsoleWrite("  Frame " & $i & ": id=" & $fid & " hash=" & $hash & " state=0x" & Hex($state) & @CRLF)
Next

ConsoleWrite(@CRLF & "--- Attempting to click NO button ---" & @CRLF)
If $noResult[0] <> 0 Then
    ConsoleWrite("Clicking NO (hash=" & $FRAME_HASH_RECONNECT_NO & ")..." & @CRLF)
    Local $clickResult = ClickFrameButton($FRAME_HASH_RECONNECT_NO)
    ConsoleWrite("ClickFrameButton returned: " & $clickResult & @CRLF)
    Sleep(2000)
    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_after_no.png', $hWnd)
    ConsoleWrite("Screenshot after NO saved" & @CRLF)
Else
    ConsoleWrite("NO button not found — dialog may not be showing" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
