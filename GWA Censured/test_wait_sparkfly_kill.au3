#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Wait for Sparkfly Swamp then kill ===" & @CRLF)

; Find DISCO PANIC
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

; Poll map ID until we're in Sparkfly Swamp (explorable)
ConsoleWrite("Waiting for explorable area..." & @CRLF)
For $poll = 1 To 120  ; 2 minutes max
    Sleep(5000)
    Local $mapId = GetMapID()
    ConsoleWrite("  MapID: " & $mapId & @CRLF)

    ; Sparkfly Swamp explorable = MapID varies, but NOT Gadd's Encampment
    ; Gadd's = outpost. Once mapId changes from the outpost, we're in explorable
    ; Actually just check if IsAtCharSelect is false and we have a valid mapId
    If $mapId > 0 And $mapId <> 755 Then  ; 755 = Gadd's Encampment (approximate)
        ConsoleWrite("In map " & $mapId & " — killing process!" & @CRLF)
        ; Kill via TerminateProcess (faster than taskkill)
        DllCall($kernel_handle, 'bool', 'TerminateProcess', 'handle', $ph, 'uint', 0)
        ConsoleWrite("Process killed. Relaunch will get reconnect dialog." & @CRLF)
        Exit 0
    EndIf
Next

ConsoleWrite("Timeout waiting for explorable" & @CRLF)
