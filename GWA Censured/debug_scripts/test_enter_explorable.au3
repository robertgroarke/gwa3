#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Enter Explorable → Kill → Reconnect Test ===" & @CRLF)

; Wait for GW client
ConsoleWrite("Waiting for GW client..." & @CRLF)
For $wait = 1 To 30
    ScanAndUpdateGameClients()
    If $game_clients[0][0] > 0 Then ExitLoop
    Sleep(2000)
Next

If $game_clients[0][0] = 0 Then
    ConsoleWrite("No GW clients found" & @CRLF)
    Exit
EndIf

SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Press Play if at char select
If IsAtCharSelect() Then
    ConsoleWrite("At char select — pressing Play..." & @CRLF)
    If IsReconnectDialogShowing() Then
        ConsoleWrite("Reconnect dialog! Clicking NO..." & @CRLF)
        DismissReconnectDialog('no')
        Sleep(3000)
    EndIf
    ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
    ConsoleWrite("Play pressed, waiting for map load..." & @CRLF)
    Sleep(10000)
EndIf

; Wait for map to fully load
For $i = 1 To 30
    Local $ml = MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword')
    If $ml = 1 Then ExitLoop
    Sleep(1000)
Next
ConsoleWrite("MapIsLoaded=" & MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword') & @CRLF)

; Check current map — should be Gadd's (638)
Local $currentMap = GetMapID()
ConsoleWrite("Current map ID: " & $currentMap & @CRLF)

If $currentMap <> 638 Then
    ConsoleWrite("Not in Gadd's — traveling there..." & @CRLF)
    TravelToOutpost(638)
    Sleep(15000)
    ConsoleWrite("Map after travel: " & GetMapID() & @CRLF)
EndIf

; Walk from Gadd's to Sparkfly Swamp exit
ConsoleWrite("Walking to Sparkfly Swamp exit..." & @CRLF)
MoveTo(-10018, -21892)
MoveTo(-9550, -20400)
Do
    Move(-9451, -19766)
    Sleep(500)
Until GetMapID() = 558 Or TimerDiff(TimerInit()) > 30000

; Wait for zone to complete
Sleep(5000)
Local $newMap = GetMapID()
ConsoleWrite("Now in map: " & $newMap & @CRLF)

If $newMap = 558 Then
    ConsoleWrite("Successfully entered Sparkfly Swamp (explorable)!" & @CRLF)
    ConsoleWrite("Waiting 5 seconds then killing GW to trigger reconnect..." & @CRLF)
    Sleep(5000)
    ProcessClose("Gw.exe")
    ConsoleWrite("GW killed! Next launch will show reconnect dialog." & @CRLF)
Else
    ConsoleWrite("Failed to enter explorable. Map=" & $newMap & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
