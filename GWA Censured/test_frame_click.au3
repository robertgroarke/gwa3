#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Test Programmatic Play Button ===" & @CRLF)

; Launch DISCO PANIC if needed
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("Launching DISCO PANIC..." & @CRLF)
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    GWLauncher_LaunchAccount($accounts, $idx)
    ConsoleWrite("Waiting 20s..." & @CRLF)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf

Local $targetIdx = $game_clients[0][0]
SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected to: " & $game_clients[$targetIdx][3] & @CRLF)

Local $hWnd = $game_clients[$targetIdx][2]

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\frame_click_before.png', $hWnd)

; Check frame states
ConsoleWrite(@CRLF & "=== Frame Detection ===" & @CRLF)
ConsoleWrite("Play button visible: " & IsFrameVisible($FRAME_HASH_PLAY_BUTTON) & @CRLF)
ConsoleWrite("Play greyed out: " & IsFrameVisible($FRAME_HASH_PLAY_GREYED) & @CRLF)
ConsoleWrite("Reconnect YES: " & IsFrameVisible($FRAME_HASH_RECONNECT_YES) & @CRLF)
ConsoleWrite("Reconnect NO: " & IsFrameVisible($FRAME_HASH_RECONNECT_NO) & @CRLF)
ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("Reconnect showing: " & IsReconnectDialogShowing() & @CRLF)

; If reconnect dialog, dismiss it
If IsReconnectDialogShowing() Then
    ConsoleWrite(@CRLF & "=== Dismissing Reconnect (No) ===" & @CRLF)
    DismissReconnectDialog('no')
    Sleep(3000)
    ConsoleWrite("After dismiss - Play visible: " & IsFrameVisible($FRAME_HASH_PLAY_BUTTON) & @CRLF)
EndIf

; Press Play
If IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Then
    ConsoleWrite(@CRLF & "=== Pressing Play ===" & @CRLF)
    Local $playResult = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
    ConsoleWrite("Play frame: ptr=0x" & Hex($playResult[0]) & " id=" & $playResult[1] & @CRLF)
    ; Clear result flag
    MemoryWrite(GetProcessHandle(), GetLabel('FrameClickResult'), 0, 'dword')

    Local $queueCounter = MemoryRead(GetProcessHandle(), GetLabel('QueueCounter'), 'dword')
    ConsoleWrite("Queue counter before: " & $queueCounter & @CRLF)

    PressPlayButton()
    Sleep(500)

    ; Read queue entry to see what was written
    Local $queueBase = GetLabel('QueueBase')
    ConsoleWrite("QueueBase = 0x" & Hex(Int($queueBase)) & @CRLF)
    ; Read the queue slot that was just written
    ; Queue counter increments mod QueueSize, each slot is 256 bytes
    Local $slotAddr = Int($queueBase) + ($queueCounter * 256)
    Local $slotVal = MemoryRead(GetProcessHandle(), $slotAddr, 'dword')
    ConsoleWrite("Queue slot[" & $queueCounter & "] first dword = 0x" & Hex($slotVal) & @CRLF)

    Sleep(3000)
    ; Check queue state after click
    Local $qcAfter = MemoryRead(GetProcessHandle(), GetLabel('QueueCounter'), 'dword')
    Local $rcpAfter = MemoryRead(GetProcessHandle(), GetLabel('RenderCmdPtr'), 'dword')
    ConsoleWrite("QueueCounter after: " & $qcAfter & @CRLF)
    ConsoleWrite("RenderCmdPtr after: 0x" & Hex($rcpAfter) & @CRLF)
    ; Check if ASM actually executed
    Local $execResult = MemoryRead(GetProcessHandle(), GetLabel('FrameClickResult'), 'dword')
    ConsoleWrite("FrameClickResult = " & $execResult & " (1 = ASM executed)" & @CRLF)
    Sleep(2000)
    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\frame_click_after.png', $hWnd)

    ; Check if we started loading
    Local $mapStatus = MemoryRead(GetProcessHandle(), GetLabel('StatusCode'), 'dword')
    ConsoleWrite("StatusCode after play: " & $mapStatus & @CRLF)

    ; Wait for map load
    If $mapStatus = 0 Then
        ConsoleWrite("Waiting 20s for map load..." & @CRLF)
        Sleep(20000)
        $mapStatus = MemoryRead(GetProcessHandle(), GetLabel('StatusCode'), 'dword')
        ConsoleWrite("StatusCode: " & $mapStatus & @CRLF)
    EndIf
Else
    ConsoleWrite("Play button not visible - cannot proceed" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
