#RequireAdmin
#include <ScreenCapture.au3>

; Find BEASTRIT window
Local $hWnd = WinGetHandle('Guild Wars - B E A S T R I T')
If $hWnd = 0 Then $hWnd = WinGetHandle('Guild Wars')
If $hWnd = 0 Then
    ConsoleWrite("Window not found" & @CRLF)
    Exit 1
EndIf

ConsoleWrite("Found: " & WinGetTitle($hWnd) & @CRLF)

; Dismiss the error dialog - click "Don't send" button
; The dialog is a Windows error reporter
Local $errWnd = WinGetHandle('Guild Wars')
; Try finding the error dialog buttons
WinActivate($hWnd)
Sleep(500)

; Click "Don't send" button on the error dialog (right side of dialog)
Local $pos = WinGetPos($hWnd)
If IsArray($pos) Then
    ; Error dialog "Don't send" is roughly at 72% x, 52% y based on screenshot
    Local $dontSendX = $pos[0] + Int($pos[2] * 0.72)
    Local $dontSendY = $pos[1] + Int($pos[3] * 0.52)
    ConsoleWrite("Clicking Don't Send at " & $dontSendX & "," & $dontSendY & @CRLF)
    MouseClick('left', $dontSendX, $dontSendY, 1, 5)
    Sleep(2000)
EndIf

; Take screenshot after dismissing
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\beastrit_after_dismiss.png', $hWnd)
ConsoleWrite("Screenshot after dismiss saved" & @CRLF)

; Now click Play button (bottom center)
Sleep(1000)
WinActivate($hWnd)
Sleep(500)
$pos = WinGetPos($hWnd)
If IsArray($pos) Then
    ; Play button is at bottom center
    Local $playX = $pos[0] + Int($pos[2] * 0.78)
    Local $playY = $pos[1] + Int($pos[3] * 0.96)
    ConsoleWrite("Clicking Play at " & $playX & "," & $playY & @CRLF)
    MouseClick('left', $playX, $playY, 1, 5)
    Sleep(5000)
EndIf

; Take final screenshot
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\beastrit_after_play.png', $hWnd)
ConsoleWrite("Final screenshot saved" & @CRLF)
ConsoleWrite("Done" & @CRLF)
