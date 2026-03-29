#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== UIMessage Discovery: Click Capture ===" & @CRLF)

; Launch DISCO PANIC
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
Local $pid = $result[0]
ConsoleWrite("Launched PID=" & $pid & @CRLF)

; Wait for char select
ConsoleWrite("Waiting 25s for char select..." & @CRLF)
Sleep(25000)

; Connect
ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": '" & $game_clients[$i][3] & "' PID=" & $game_clients[$i][0] & @CRLF)
    If $game_clients[$i][0] = $pid Then $targetIdx = $i
Next
If $targetIdx = -1 Then $targetIdx = $game_clients[0][0]

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected to client " & $targetIdx & @CRLF)

Local $processHandle = GetProcessHandle()
Local $hWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Window handle: " & $hWnd & @CRLF)

; Enable sniffer
ConsoleWrite(@CRLF & "=== Enabling UISniffer ===" & @CRLF)
UISnifferEnable()

; Helper: capture messages for N seconds, return array of [msgid, wparam, lparam]
Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')
Local $lastCounter = MemoryRead($processHandle, $counterAddr, 'dword')

Func ScreenshotAndCapture($label, $captureSecs)
    ; Take screenshot
    WinActivate($hWnd)
    Sleep(500)
    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\sniffer_' & $label & '.png', $hWnd)
    ConsoleWrite("[" & $label & "] Screenshot saved" & @CRLF)

    ; Flush current counter
    $lastCounter = MemoryRead($processHandle, $counterAddr, 'dword')

    ; Capture for N seconds
    Local $count = 0
    For $p = 1 To ($captureSecs * 10)
        Sleep(100)
        Local $c = MemoryRead($processHandle, $counterAddr, 'dword')
        If $c <> $lastCounter Then
            Local $mid = MemoryRead($processHandle, $msgIdAddr, 'dword')
            Local $wp = MemoryRead($processHandle, $wParamAddr, 'dword')
            Local $lp = MemoryRead($processHandle, $lParamAddr, 'dword')

            ; Try to read wparam data if it's a pointer
            Local $wp0 = 0, $wp1 = 0, $wp2 = 0, $wp3 = 0
            If $wp > 0x10000 Then
                $wp0 = MemoryRead($processHandle, $wp, 'dword')
                $wp1 = MemoryRead($processHandle, $wp + 4, 'dword')
                $wp2 = MemoryRead($processHandle, $wp + 8, 'dword')
                $wp3 = MemoryRead($processHandle, $wp + 12, 'dword')
            EndIf

            ConsoleWrite("  [" & $label & "] 0x" & Hex($mid, 8) & _
                " wP=0x" & Hex($wp, 8) & " [" & Hex($wp0,8) & "," & Hex($wp1,8) & "," & Hex($wp2,8) & "," & Hex($wp3,8) & "]" & _
                " lP=" & $lp & @CRLF)
            $lastCounter = $c
            $count += 1
        EndIf
    Next
    ConsoleWrite("  (" & $count & " messages)" & @CRLF)
EndFunc

Func ClickAt($label, $xPct, $yPct)
    WinActivate($hWnd)
    Sleep(300)
    Local $pos = WinGetPos($hWnd)
    If Not IsArray($pos) Then Return
    Local $cx = $pos[0] + Int($pos[2] * $xPct)
    Local $cy = $pos[1] + Int($pos[3] * $yPct)
    ConsoleWrite("[" & $label & "] Clicking at " & $cx & "," & $cy & " (" & $xPct & "," & $yPct & ")" & @CRLF)
    MouseClick('left', $cx, $cy, 1, 3)
EndFunc

; ===================================================================
; Step 1: Screenshot the initial state (char select, possibly with reconnect dialog)
; ===================================================================
ConsoleWrite(@CRLF & "========== STEP 1: Initial state ==========" & @CRLF)
ScreenshotAndCapture("01_initial", 3)

; ===================================================================
; Step 2: If reconnect dialog is showing, click YES on it
; The reconnect dialog YES button is typically on the left side of the dialog
; We'll screenshot, then click, then screenshot again to see what changed
; ===================================================================
ConsoleWrite(@CRLF & "========== STEP 2: Click reconnect YES ==========" & @CRLF)
; GW reconnect dialog: "Do you want to reconnect?" with Yes/No buttons
; YES is on the left, NO is on the right
; Dialog appears roughly center of screen
; YES button: ~42% x, ~58% y (rough estimate, will adjust after seeing screenshot)
ClickAt("02_reconnect_yes", 0.42, 0.58)
ScreenshotAndCapture("02_after_reconnect", 5)

; ===================================================================
; Step 3: Screenshot current state - are we loading? still at char select?
; ===================================================================
ConsoleWrite(@CRLF & "========== STEP 3: Post-reconnect state ==========" & @CRLF)
Sleep(3000)
ScreenshotAndCapture("03_state", 3)

; ===================================================================
; Step 4: If still at char select, click on DISCO PANIC character portrait
; Character portraits are at the bottom of screen in a carousel
; ===================================================================
ConsoleWrite(@CRLF & "========== STEP 4: Click character portrait ==========" & @CRLF)
; Take screenshot first to see current state
WinActivate($hWnd)
Sleep(500)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\sniffer_04_before_portrait.png', $hWnd)

; Click on character portraits (bottom center area)
; The currently selected character is the large center one
; Other characters are smaller on left/right
; Let's click the center character to ensure it's selected
ClickAt("04_portrait_center", 0.50, 0.82)
ScreenshotAndCapture("04_after_portrait", 3)

; ===================================================================
; Step 5: Click PLAY button
; Play button is at the bottom right area of the screen
; ===================================================================
ConsoleWrite(@CRLF & "========== STEP 5: Click PLAY button ==========" & @CRLF)
WinActivate($hWnd)
Sleep(500)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\sniffer_05_before_play.png', $hWnd)

ClickAt("05_play", 0.78, 0.96)
ScreenshotAndCapture("05_after_play", 8)

; Final screenshot
ConsoleWrite(@CRLF & "========== FINAL STATE ==========" & @CRLF)
Sleep(3000)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\sniffer_06_final.png', $hWnd)

; Check map status
Local $mapStatus = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("Final StatusCode: " & $mapStatus & @CRLF)

UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
