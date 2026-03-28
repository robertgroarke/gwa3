#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== UISniffer Click Capture ===" & @CRLF)

Local $targetPID = 15504

ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    If $game_clients[$i][0] = $targetPID Then $targetIdx = $i
Next

If $targetIdx = -1 Then
    ConsoleWrite("ERROR: PID " & $targetPID & " not found" & @CRLF)
    Exit 1
EndIf

SelectClient($targetIdx)
InitializeGameClientData(True, False)
ConsoleWrite("Connected to PID " & $targetPID & @CRLF)

Local $processHandle = GetProcessHandle()
Local $hWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Window: " & $hWnd & @CRLF)

; Take a screenshot first to see the current state
Local $ssPath = @ScriptDir & "\tests\charselect_before.png"
WinActivate($hWnd)
Sleep(1000)
Local $pos = WinGetPos($hWnd)
If IsArray($pos) Then
    ConsoleWrite("Window pos: " & $pos[0] & "," & $pos[1] & " size: " & $pos[2] & "x" & $pos[3] & @CRLF)
    _ScreenCapture_CaptureWnd($ssPath, $hWnd)
    ConsoleWrite("Screenshot saved: " & $ssPath & @CRLF)
EndIf

; Enable sniffer
ConsoleWrite(@CRLF & "=== Enabling UISniffer ===" & @CRLF)
UISnifferEnable()

; Helper: flush and capture messages for N seconds
Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')
Local $lastCounter = MemoryRead($processHandle, $counterAddr, 'dword')

Func CaptureMessages($duration, $label)
    ConsoleWrite(@CRLF & "--- " & $label & " (capturing " & $duration & "s) ---" & @CRLF)
    Local $count = 0
    For $p = 1 To ($duration * 10)
        Sleep(100)
        Local $c = MemoryRead($processHandle, $counterAddr, 'dword')
        If $c <> $lastCounter Then
            Local $mid = MemoryRead($processHandle, $msgIdAddr, 'dword')
            Local $wp = MemoryRead($processHandle, $wParamAddr, 'dword')
            Local $lp = MemoryRead($processHandle, $lParamAddr, 'dword')
            ConsoleWrite("  [" & $label & "] 0x" & Hex($mid, 8) & " wP=0x" & Hex($wp, 8) & " lP=" & $lp & @CRLF)
            $lastCounter = $c
            $count += 1
        EndIf
    Next
    ConsoleWrite("  (" & $count & " messages)" & @CRLF)
    Return $count
EndFunc

; Baseline: capture idle messages for 5 seconds (nothing clicked)
CaptureMessages(5, "IDLE")

; Click 1: Click on a character portrait (roughly center-left of screen)
; GW char select: portraits are on the left ~1/4 of screen, stacked vertically
ConsoleWrite(@CRLF & "=== Click: Character portrait area ===" & @CRLF)
WinActivate($hWnd)
Sleep(500)
If IsArray($pos) Then
    ; Click roughly in the character list area (left side, upper portion)
    Local $clickX = $pos[0] + Int($pos[2] * 0.15)
    Local $clickY = $pos[1] + Int($pos[3] * 0.35)
    ConsoleWrite("Clicking at " & $clickX & "," & $clickY & @CRLF)
    MouseClick('left', $clickX, $clickY, 1, 5)
EndIf
CaptureMessages(5, "CHAR_PORTRAIT_1")

; Click 2: Click on a different character portrait (lower in the list)
ConsoleWrite(@CRLF & "=== Click: Second character portrait ===" & @CRLF)
WinActivate($hWnd)
Sleep(500)
If IsArray($pos) Then
    Local $clickX2 = $pos[0] + Int($pos[2] * 0.15)
    Local $clickY2 = $pos[1] + Int($pos[3] * 0.50)
    ConsoleWrite("Clicking at " & $clickX2 & "," & $clickY2 & @CRLF)
    MouseClick('left', $clickX2, $clickY2, 1, 5)
EndIf
CaptureMessages(5, "CHAR_PORTRAIT_2")

; Click 3: Click on Play button (bottom center of screen)
ConsoleWrite(@CRLF & "=== Click: Play button ===" & @CRLF)
WinActivate($hWnd)
Sleep(500)
If IsArray($pos) Then
    Local $clickX3 = $pos[0] + Int($pos[2] * 0.50)
    Local $clickY3 = $pos[1] + Int($pos[3] * 0.92)
    ConsoleWrite("Clicking at " & $clickX3 & "," & $clickY3 & @CRLF)
    MouseClick('left', $clickX3, $clickY3, 1, 5)
EndIf
CaptureMessages(10, "PLAY_BUTTON")

; Take another screenshot
Local $ssPath2 = @ScriptDir & "\tests\charselect_after.png"
_ScreenCapture_CaptureWnd($ssPath2, $hWnd)
ConsoleWrite("Screenshot saved: " & $ssPath2 & @CRLF)

; Check if we started loading
Local $mapStatus = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite(@CRLF & "StatusCode after clicks: " & $mapStatus & @CRLF)

; If loading, capture during load
If $mapStatus <> 0 Then
    CaptureMessages(15, "MAP_LOADING")
EndIf

UISnifferDisable()
ConsoleWrite(@CRLF & "=== DONE ===" & @CRLF)
