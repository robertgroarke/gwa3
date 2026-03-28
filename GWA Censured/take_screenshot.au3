#RequireAdmin
#include <ScreenCapture.au3>

Local $charName = "D I S C O P A N I C"
If $CmdLine[0] > 0 Then $charName = $CmdLine[1]

Local $hWnd = WinGetHandle('Guild Wars - ' & $charName)
If $hWnd = 0 Then
    $hWnd = WinGetHandle('Guild Wars')
EndIf
If $hWnd = 0 Then
    ConsoleWrite("Window not found for " & $charName & @CRLF)
    Exit 1
EndIf

ConsoleWrite("Found: " & WinGetTitle($hWnd) & @CRLF)
WinActivate($hWnd)
Sleep(500)
Local $safeName = StringReplace($charName, ' ', '')
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\' & $safeName & '_status.png', $hWnd)
ConsoleWrite("Screenshot saved" & @CRLF)
