#RequireAdmin
; Find and close the Windows Error Reporting dialog
Local $errWnd = WinGetHandle("[CLASS:#32770]")
If $errWnd Then
    Local $title = WinGetTitle($errWnd)
    ConsoleWrite("Found dialog: " & $title & @CRLF)
    Local $pos = WinGetPos($errWnd)
    If IsArray($pos) Then
        ; Click "Don't send" button (right side of dialog)
        Local $btnX = $pos[0] + Int($pos[2] * 0.88)
        Local $btnY = $pos[1] + Int($pos[3] * 0.45)
        MouseClick('left', $btnX, $btnY, 1, 3)
        ConsoleWrite("Clicked Don't Send" & @CRLF)
    EndIf
Else
    ConsoleWrite("No error dialog found" & @CRLF)
EndIf
