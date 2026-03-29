#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== UISniffer In-Game Test ===" & @CRLF)

Local $targetPID = 15504

ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": " & $game_clients[$i][3] & " (PID=" & $game_clients[$i][0] & ")" & @CRLF)
    If $game_clients[$i][0] = $targetPID Then $targetIdx = $i
Next

If $targetIdx = -1 Then
    ConsoleWrite("ERROR: PID " & $targetPID & " not found" & @CRLF)
    Exit 1
EndIf

SelectClient($targetIdx)
InitializeGameClientData(True, False)
ConsoleWrite("Connected to PID " & $targetPID & @CRLF)

Local $hWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Window handle: " & $hWnd & @CRLF)

Local $processHandle = GetProcessHandle()

; Check map status before pressing play
Local $mapStatus = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode before play: " & $mapStatus & @CRLF)

; Read PreGameContext to check if we're at char select
Local $preGamePtr = MemRead($pre_game_address)
ConsoleWrite("PreGameContext ptr: 0x" & Hex($preGamePtr) & @CRLF)

; Try pressing Play using Send (foreground) instead of ControlSend
ConsoleWrite(@CRLF & "=== Pressing Play ===" & @CRLF)
WinActivate($hWnd)
Sleep(1500)

; First attempt: just Enter (for char select without reconnect dialog)
Send('{ENTER}')
ConsoleWrite("Sent Enter via Send()" & @CRLF)
Sleep(5000)

$mapStatus = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode after Enter: " & $mapStatus & @CRLF)

If $mapStatus = 0 Then
    ; Maybe there's a reconnect dialog — try Right+Enter then Enter again
    ConsoleWrite("Not loaded. Trying reconnect dismiss..." & @CRLF)
    WinActivate($hWnd)
    Sleep(500)
    Send('{RIGHT}')
    Sleep(300)
    Send('{ENTER}')
    ConsoleWrite("Sent Right+Enter (dismiss reconnect)" & @CRLF)
    Sleep(3000)

    ; Now press Play
    WinActivate($hWnd)
    Sleep(500)
    Send('{ENTER}')
    ConsoleWrite("Sent Enter (press Play)" & @CRLF)
    Sleep(5000)

    $mapStatus = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
    ConsoleWrite("StatusCode after reconnect+play: " & $mapStatus & @CRLF)
EndIf

If $mapStatus = 0 Then
    ConsoleWrite("Waiting 30s for map load..." & @CRLF)
    Sleep(30000)
    $mapStatus = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
    ConsoleWrite("StatusCode: " & $mapStatus & @CRLF)
EndIf

; Enable sniffer
ConsoleWrite(@CRLF & "=== Enabling UISniffer ===" & @CRLF)
UISnifferEnable()

; Poll for 20 seconds
ConsoleWrite(@CRLF & "=== Polling for 20 seconds ===" & @CRLF)
Local $lastCounter = 0
Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')
Local $captureCount = 0
Local $uniqueMsgs[]

For $poll = 1 To 200
    Sleep(100)
    Local $counter = MemoryRead($processHandle, $counterAddr, 'dword')
    If $counter <> $lastCounter Then
        Local $msgId = MemoryRead($processHandle, $msgIdAddr, 'dword')
        Local $wParamPtr = MemoryRead($processHandle, $wParamAddr, 'dword')
        Local $lParamVal = MemoryRead($processHandle, $lParamAddr, 'dword')

        Local $msgHex = "0x" & Hex($msgId, 8)
        If Not MapExists($uniqueMsgs, $msgHex) Then $uniqueMsgs[$msgHex] = 0
        $uniqueMsgs[$msgHex] += 1

        If $captureCount < 50 Then
            ConsoleWrite("[CAPTURE] #" & $counter & " " & $msgHex & _
                " wP=0x" & Hex($wParamPtr, 8) & " lP=" & $lParamVal & @CRLF)
        EndIf

        $lastCounter = $counter
        $captureCount += 1
    EndIf
Next

ConsoleWrite(@CRLF & "=== Results ===" & @CRLF)
ConsoleWrite("Total: " & $captureCount & " messages in 20s" & @CRLF)
ConsoleWrite("Unique IDs:" & @CRLF)
Local $keys = MapKeys($uniqueMsgs)
For $k In $keys
    ConsoleWrite("  " & $k & " x" & $uniqueMsgs[$k] & @CRLF)
Next

UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
