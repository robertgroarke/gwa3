#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Reconnect Dialog Frame Test ===" & @CRLF)

; Launch DISCO PANIC (should get reconnect dialog)
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
GWLauncher_LaunchAccount($accounts, $idx)
ConsoleWrite("Waiting 25s for char select + reconnect..." & @CRLF)
Sleep(25000)

ScanAndUpdateGameClients()
Local $targetIdx = $game_clients[0][0]
SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()
Local $hWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Connected" & @CRLF)

; Screenshot
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_test.png', $hWnd)

; Check frame states
ConsoleWrite(@CRLF & "=== Frame Detection ===" & @CRLF)
ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("Reconnect showing: " & IsReconnectDialogShowing() & @CRLF)
ConsoleWrite("Play visible: " & IsFrameVisible($FRAME_HASH_PLAY_BUTTON) & @CRLF)
ConsoleWrite("Play greyed: " & IsFrameVisible($FRAME_HASH_PLAY_GREYED) & @CRLF)
ConsoleWrite("Reconnect YES: " & IsFrameVisible($FRAME_HASH_RECONNECT_YES) & @CRLF)
ConsoleWrite("Reconnect NO: " & IsFrameVisible($FRAME_HASH_RECONNECT_NO) & @CRLF)

; If reconnect is showing, try clicking NO
If IsReconnectDialogShowing() Then
    ConsoleWrite(@CRLF & "=== Clicking NO on reconnect ===" & @CRLF)
    DismissReconnectDialog('no')
    Sleep(3000)

    ; Check state after dismissing
    ConsoleWrite("After NO:" & @CRLF)
    ConsoleWrite("  Reconnect showing: " & IsReconnectDialogShowing() & @CRLF)
    ConsoleWrite("  Play visible: " & IsFrameVisible($FRAME_HASH_PLAY_BUTTON) & @CRLF)

    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_after_no.png', $hWnd)

    ; Now click Play
    If IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Then
        ConsoleWrite(@CRLF & "=== Clicking Play ===" & @CRLF)
        PressPlayButton()
        Sleep(10000)
        Local $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
        ConsoleWrite("StatusCode: " & $status & @CRLF)
        _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_after_play.png', $hWnd)
    EndIf
Else
    ConsoleWrite("No reconnect dialog — pressing Play directly" & @CRLF)
    If IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Then
        PressPlayButton()
        Sleep(10000)
    EndIf
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
