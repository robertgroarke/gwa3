#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Reconnect Dialog UIMessage Capture ===" & @CRLF)
ConsoleWrite("Phase 1: Launch, enter game, go to explorable, then kill" & @CRLF)

; --- Phase 1: Launch, get into explorable, kill ---
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")

ConsoleWrite("Launching DISCO PANIC..." & @CRLF)
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
Local $pid = $result[0]
ConsoleWrite("PID=" & $pid & @CRLF)

ConsoleWrite("Waiting 20s for char select..." & @CRLF)
Sleep(20000)

; Connect to press Play
ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    If $game_clients[$i][0] = $pid Then $targetIdx = $i
Next
If $targetIdx = -1 Then $targetIdx = $game_clients[0][0]

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $hWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Connected. Pressing Play..." & @CRLF)

; Press Play via mouse click
WinActivate($hWnd)
Sleep(1000)
Local $pos = WinGetPos($hWnd)
MouseClick('left', $pos[0] + Int($pos[2] * 0.78), $pos[1] + Int($pos[3] * 0.96), 1, 3)

; Wait for map to load (into Gadd's Encampment)
ConsoleWrite("Waiting 30s for map load..." & @CRLF)
Sleep(30000)

; Now travel to Sparkfly Swamp (explorable)
ConsoleWrite("Traveling to Sparkfly Swamp..." & @CRLF)
Local $processHandle = GetProcessHandle()
TravelToOutpost($Sparkfly_Swamp)
Sleep(15000)

; Verify we're in the explorable
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_in_explorable.png', $hWnd)
ConsoleWrite("In explorable. Killing process NOW..." & @CRLF)

; Kill the GW process while in explorable
DllCall($kernel_handle, 'bool', 'TerminateProcess', 'handle', $processHandle, 'uint', 0)
Sleep(3000)

; --- Phase 2: Relaunch — should get reconnect dialog ---
ConsoleWrite(@CRLF & "Phase 2: Relaunching for reconnect dialog..." & @CRLF)
Local $result2 = GWLauncher_LaunchAccount($accounts, $idx)
Local $pid2 = $result2[0]
ConsoleWrite("PID=" & $pid2 & @CRLF)

ConsoleWrite("Waiting 25s for reconnect dialog..." & @CRLF)
Sleep(25000)

; Connect to new process
ScanAndUpdateGameClients()
$targetIdx = -1
For $i = 1 To $game_clients[0][0]
    If $game_clients[$i][0] = $pid2 Then $targetIdx = $i
Next
If $targetIdx = -1 Then $targetIdx = $game_clients[0][0]

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
$processHandle = GetProcessHandle()
$hWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Connected to new process" & @CRLF)

; Enable sniffer
UISnifferEnable()
ConsoleWrite("Sniffer enabled" & @CRLF)

Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')
Local $lastCounter = MemoryRead($processHandle, $counterAddr, 'dword')

; Screenshot to see the reconnect dialog
WinActivate($hWnd)
Sleep(500)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_dialog.png', $hWnd)
ConsoleWrite("Screenshot saved: reconnect_dialog.png" & @CRLF)
$pos = WinGetPos($hWnd)
ConsoleWrite("Window: " & $pos[0] & "," & $pos[1] & " " & $pos[2] & "x" & $pos[3] & @CRLF)

; Flush counter
$lastCounter = MemoryRead($processHandle, $counterAddr, 'dword')

; ===================================================================
; Click NO on the reconnect dialog
; Will adjust coordinates after seeing the screenshot
; GW reconnect: dialog centered, Yes=left button, No=right button
; ===================================================================
ConsoleWrite(@CRLF & "=== Clicking NO on reconnect ===" & @CRLF)
WinActivate($hWnd)
Sleep(300)
; Reconnect dialog buttons are roughly in the center of the screen
; No button (right side): ~55% x, ~52% y
Local $noX = $pos[0] + Int($pos[2] * 0.55)
Local $noY = $pos[1] + Int($pos[3] * 0.52)
ConsoleWrite("Clicking NO at " & $noX & "," & $noY & @CRLF)
MouseClick('left', $noX, $noY, 1, 3)

; Capture messages
Local $count = 0
For $p = 1 To 50
    Sleep(100)
    Local $c = MemoryRead($processHandle, $counterAddr, 'dword')
    If $c <> $lastCounter Then
        Local $mid = MemoryRead($processHandle, $msgIdAddr, 'dword')
        Local $wp = MemoryRead($processHandle, $wParamAddr, 'dword')
        Local $lp = MemoryRead($processHandle, $lParamAddr, 'dword')
        Local $wp0 = 0, $wp1 = 0, $wp2 = 0, $wp3 = 0
        If $wp > 0x10000 Then
            $wp0 = MemoryRead($processHandle, $wp, 'dword')
            $wp1 = MemoryRead($processHandle, $wp + 4, 'dword')
            $wp2 = MemoryRead($processHandle, $wp + 8, 'dword')
            $wp3 = MemoryRead($processHandle, $wp + 12, 'dword')
        EndIf
        ConsoleWrite("  [NO] 0x" & Hex($mid, 8) & " wP=0x" & Hex($wp, 8) & _
            " [" & Hex($wp0,8) & "," & Hex($wp1,8) & "," & Hex($wp2,8) & "," & Hex($wp3,8) & "] lP=" & $lp & @CRLF)
        $lastCounter = $c
        $count += 1
    EndIf
Next
ConsoleWrite("  (" & $count & " messages after NO click)" & @CRLF)

; Screenshot after NO
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_after_no.png', $hWnd)

; Now click Play
ConsoleWrite(@CRLF & "=== Clicking PLAY ===" & @CRLF)
$lastCounter = MemoryRead($processHandle, $counterAddr, 'dword')
WinActivate($hWnd)
Sleep(500)
MouseClick('left', $pos[0] + Int($pos[2] * 0.78), $pos[1] + Int($pos[3] * 0.96), 1, 3)

$count = 0
For $p = 1 To 80
    Sleep(100)
    Local $c2 = MemoryRead($processHandle, $counterAddr, 'dword')
    If $c2 <> $lastCounter Then
        Local $mid2 = MemoryRead($processHandle, $msgIdAddr, 'dword')
        Local $wp2v = MemoryRead($processHandle, $wParamAddr, 'dword')
        Local $lp2 = MemoryRead($processHandle, $lParamAddr, 'dword')
        ConsoleWrite("  [PLAY] 0x" & Hex($mid2, 8) & " wP=0x" & Hex($wp2v, 8) & " lP=" & $lp2 & @CRLF)
        $lastCounter = $c2
        $count += 1
    EndIf
Next
ConsoleWrite("  (" & $count & " messages after PLAY)" & @CRLF)

; Final
Sleep(3000)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\reconnect_final.png', $hWnd)
Local $mapStatus = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("Final StatusCode: " & $mapStatus & @CRLF)

UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
