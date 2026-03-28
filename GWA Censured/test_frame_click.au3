#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Test Programmatic Play Button ===" & @CRLF)

; Launch DISCO PANIC if needed
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("Launching DISCO PANIC..." & @CRLF)
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    GWLauncher_LaunchAccount($accounts, $idx)
    ConsoleWrite("Waiting 20s..." & @CRLF)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf

Local $targetIdx = $game_clients[0][0]
SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected to: " & $game_clients[$targetIdx][3] & @CRLF)

Local $hWnd = $game_clients[$targetIdx][2]

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\frame_click_before.png', $hWnd)

; Check frame states
ConsoleWrite(@CRLF & "=== Frame Detection ===" & @CRLF)
ConsoleWrite("Play button visible: " & IsFrameVisible($FRAME_HASH_PLAY_BUTTON) & @CRLF)
ConsoleWrite("Play greyed out: " & IsFrameVisible($FRAME_HASH_PLAY_GREYED) & @CRLF)
ConsoleWrite("Reconnect YES: " & IsFrameVisible($FRAME_HASH_RECONNECT_YES) & @CRLF)
ConsoleWrite("Reconnect NO: " & IsFrameVisible($FRAME_HASH_RECONNECT_NO) & @CRLF)
ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("Reconnect showing: " & IsReconnectDialogShowing() & @CRLF)

; If reconnect dialog, dismiss it
If IsReconnectDialogShowing() Then
    ConsoleWrite(@CRLF & "=== Dismissing Reconnect (No) ===" & @CRLF)
    DismissReconnectDialog('no')
    Sleep(3000)
    ConsoleWrite("After dismiss - Play visible: " & IsFrameVisible($FRAME_HASH_PLAY_BUTTON) & @CRLF)
EndIf

; Press Play
If IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Then
    ConsoleWrite(@CRLF & "=== Pressing Play ===" & @CRLF)
    Local $playResult = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
    ConsoleWrite("Play frame: ptr=0x" & Hex($playResult[0]) & " id=" & $playResult[1] & @CRLF)
    PressPlayButton()
    Sleep(5000)
    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\frame_click_after.png', $hWnd)

    ; Check if we started loading
    Local $mapStatus = MemoryRead(GetProcessHandle(), GetLabel('StatusCode'), 'dword')
    ConsoleWrite("StatusCode after play: " & $mapStatus & @CRLF)

    ; Wait for map load
    If $mapStatus = 0 Then
        ConsoleWrite("Waiting 20s for map load..." & @CRLF)
        Sleep(20000)
        $mapStatus = MemoryRead(GetProcessHandle(), GetLabel('StatusCode'), 'dword')
        ConsoleWrite("StatusCode: " & $mapStatus & @CRLF)
    EndIf
Else
    ConsoleWrite("Play button not visible - cannot proceed" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
