#RequireAdmin
#include <ScreenCapture.au3>

; List ALL windows to find the error dialog
Local $winList = WinList()
For $i = 1 To $winList[0][0]
    If $winList[$i][0] <> '' And BitAND(WinGetState($winList[$i][1]), 2) Then
        If StringInStr($winList[$i][0], 'Error') Or StringInStr($winList[$i][0], 'error') Or _
           StringInStr($winList[$i][0], 'Report') Or StringInStr($winList[$i][0], 'Gw') Or _
           StringInStr($winList[$i][0], 'Guild') Or StringInStr($winList[$i][0], 'crash') Then
            ConsoleWrite("Window: '" & $winList[$i][0] & "' handle=" & $winList[$i][1] & @CRLF)
        EndIf
    EndIf
Next

; Also check for dialog-class windows
Local $dialogWnd = WinGetHandle("[CLASS:#32770]")
If $dialogWnd Then
    ConsoleWrite("Found dialog window: " & $dialogWnd & " title='" & WinGetTitle($dialogWnd) & "'" & @CRLF)
    ; Try clicking Don't Send within this dialog
    Local $dPos = WinGetPos($dialogWnd)
    If IsArray($dPos) Then
        ConsoleWrite("Dialog pos: " & $dPos[0] & "," & $dPos[1] & " " & $dPos[2] & "x" & $dPos[3] & @CRLF)
        ; "Don't send" is typically the rightmost button
        Local $btnX = $dPos[0] + Int($dPos[2] * 0.88)
        Local $btnY = $dPos[1] + Int($dPos[3] * 0.45)
        ConsoleWrite("Clicking button at " & $btnX & "," & $btnY & @CRLF)
        MouseClick('left', $btnX, $btnY, 1, 3)
        Sleep(1000)
    EndIf
EndIf

; Try ControlClick on known button text
ControlClick("", "", "[TEXT:Don't send]")
Sleep(500)
ControlClick("", "", "[TEXT:Don't Send]")
Sleep(500)

; Screenshot result
Local $hWnd = WinGetHandle('Guild Wars - B E A S T R I T')
If $hWnd Then
    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\beastrit_status.png', $hWnd)
EndIf
ConsoleWrite("Done" & @CRLF)
