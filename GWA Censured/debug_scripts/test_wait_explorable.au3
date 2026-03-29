#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Wait for explorable then kill ===" & @CRLF)

ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    If $game_clients[$i][3] = "D I S C O P A N I C" Then
        $targetIdx = $i
        ExitLoop
    EndIf
Next
If $targetIdx = -1 Then
    ConsoleWrite("DISCO PANIC not found" & @CRLF)
    Exit 1
EndIf

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()
Local $pid = $game_clients[$targetIdx][0]
ConsoleWrite("Connected to PID " & $pid & @CRLF)

; Gadd's Encampment outpost IDs: 640 (Gadd's)
; Sparkfly Swamp outpost: 638
; Bogroot Growths Level 1: 649
; Bogroot Growths Level 2: 650
; We need to be in an EXPLORABLE (dungeon or open area), not an outpost

Local $outpostIDs = "640,638,492"  ; known outpost map IDs

ConsoleWrite("Monitoring map ID..." & @CRLF)
For $poll = 1 To 60  ; 5 minutes max
    Sleep(5000)
    Local $mapId = GetMapID()
    Local $isOutpost = StringInStr($outpostIDs, String($mapId))
    ConsoleWrite("  MapID=" & $mapId & " outpost=" & ($isOutpost > 0) & @CRLF)

    If $mapId > 0 And Not $isOutpost Then
        ConsoleWrite("IN EXPLORABLE MAP " & $mapId & "! Killing in 5s..." & @CRLF)
        Sleep(5000)
        DllCall($kernel_handle, 'bool', 'TerminateProcess', 'handle', $ph, 'uint', 0)
        ConsoleWrite("Killed PID " & $pid & ". Relaunch will show reconnect dialog." & @CRLF)
        Exit 0
    EndIf
Next

ConsoleWrite("Timeout" & @CRLF)
