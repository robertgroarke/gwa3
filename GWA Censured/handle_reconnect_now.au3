#RequireAdmin
; Dismiss reconnect dialog with Escape (closes dialog without accepting), then press Play
Local $hWnd = WinGetHandle('Guild Wars')
If $hWnd = 0 Then
    Local $winList = WinList()
    For $i = 1 To $winList[0][0]
        If StringInStr($winList[$i][0], 'Guild Wars') Then
            $hWnd = $winList[$i][1]
            ExitLoop
        EndIf
    Next
EndIf
If $hWnd = 0 Then Exit

WinActivate($hWnd)
Sleep(500)
Send('{ESCAPE}')
ConsoleWrite("Sent Escape to dismiss reconnect" & @CRLF)
Sleep(2000)
Send('{ENTER}')
ConsoleWrite("Sent Enter to press Play" & @CRLF)
