#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== GameTick Hook + Play Button Test ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("No GW clients found" & @CRLF)
    Exit
EndIf
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Check GameTick hook was installed
Local $gtStart = GetLabel('GameTickStart')
Local $gtReturn = GetLabel('GameTickReturn')
Local $gtOrig = GetLabel('GameTickOrigCode')
ConsoleWrite("GameTickStart:    " & $gtStart & @CRLF)
ConsoleWrite("GameTickReturn:   " & $gtReturn & @CRLF)
ConsoleWrite("GameTickOrigCode: " & $gtOrig & @CRLF)

If $gtStart = -1 Or $gtStart = 0 Then
    ConsoleWrite("ERROR: GameTick scan pattern not found!" & @CRLF)
    Exit
EndIf

; Read the detour at GameTickStart — should be E9 (JMP)
Local $detourByte = MemoryRead($ph, $gtStart, 'byte')
ConsoleWrite("Byte at GameTickStart: 0x" & Hex($detourByte, 2) & " (expected 0xE9 = JMP)" & @CRLF)

; Read the saved original bytes at GameTickOrigCode
Local $origBuf = DllStructCreate('byte[10]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr(Int($gtOrig)), _
    'ptr', DllStructGetPtr($origBuf), 'ulong_ptr', 10, 'ulong_ptr*', 0)
Local $origHex = ''
For $i = 1 To 10
    $origHex &= Hex(DllStructGetData($origBuf, 1, $i), 2) & ' '
Next
ConsoleWrite("GameTickOrigCode bytes: " & $origHex & @CRLF)
ConsoleWrite("  (first 5 = saved prologue, next 5 = JMP to GameTickReturn)" & @CRLF)

; Check if at char select
Local $atCharSelect = IsAtCharSelect()
ConsoleWrite("At char select: " & $atCharSelect & @CRLF)

If Not $atCharSelect Then
    ConsoleWrite("Not at char select — cannot test Play button" & @CRLF)
    ConsoleWrite("=== Hook verification complete ===" & @CRLF)
    Exit
EndIf

; Check for Play button
Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
ConsoleWrite("Play button: ptr=0x" & Hex(Int($pf[0])) & " id=" & $pf[1] & @CRLF)

If $pf[0] = 0 Then
    ConsoleWrite("Play button not found" & @CRLF)
    Exit
EndIf

; Check for reconnect dialog first
If IsReconnectDialogShowing() Then
    ConsoleWrite("Reconnect dialog detected — dismissing with NO" & @CRLF)
    DismissReconnectDialog('no')
    Sleep(2000)
EndIf

; Press Play via GameTick hook
ConsoleWrite("Pressing Play button via ClickFrameButton (GameTick queue)..." & @CRLF)
Local $result = ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
ConsoleWrite("ClickFrameButton returned: " & $result & @CRLF)

; Wait and check if we left char select
Sleep(3000)
Local $stillAtCharSelect = IsAtCharSelect()
ConsoleWrite("Still at char select after click: " & $stillAtCharSelect & @CRLF)

If Not $stillAtCharSelect Then
    ConsoleWrite("SUCCESS! Play button click worked via GameTick hook!" & @CRLF)
Else
    ConsoleWrite("Play button click did not transition — may need investigation" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
