#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Material Trader Crash Test ===" & @CRLF)

; Connect to running client
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("No GW clients" & @CRLF)
    Exit
EndIf
; Wait for a client that's already in-game (launched by Froggy autolaunch)
ConsoleWrite("Waiting for in-game client..." & @CRLF)
For $w = 1 To 60
    ScanAndUpdateGameClients()
    If $game_clients[0][0] > 0 Then
        SelectClient(1)
        ; Only init if not already initialized
        InitializeGameClientForGWA2(False)
        Local $ml = MemoryRead(GetProcessHandle(), GetLabel('MapIsLoaded'), 'dword')
        If $ml = 1 Then
            ConsoleWrite("Client is in-game (MapIsLoaded=1)" & @CRLF)
            ExitLoop
        EndIf
    EndIf
    Sleep(5000)
Next

Local $ph = GetProcessHandle()
ConsoleWrite("Map: " & GetMapID() & @CRLF)
ConsoleWrite("Character: " & GetCharacterName() & @CRLF)

; Check if in Gadd's (638)
If GetMapID() <> 638 Then
    ConsoleWrite("Not in Gadd's — traveling..." & @CRLF)
    TravelToOutpost(638)
    Sleep(15000)
EndIf

ConsoleWrite("In Gadd's. Walking to material trader..." & @CRLF)

; Material trader coordinates in Gadd's from the bot's waypoints
; The bot uses RareMaterialTrader() and MaterialTrader() which walk to NPCs
; Let's just go to the material trader NPC and interact
; Gadd's material trader is roughly at these coords
MoveTo(-11456, -23050)
Sleep(1000)

; Find nearest material trader NPC and interact
ConsoleWrite("Looking for material trader NPC..." & @CRLF)
Local $traderAgent = GetNearestNPCToCoords(-11456, -23050)
If $traderAgent > 0 Then
    ConsoleWrite("Found NPC agent: " & $traderAgent & @CRLF)
    GoToNPC($traderAgent)
    Sleep(2000)

    ; Try a RequestQuote — this uses the trader command structs
    ; that were crashing before
    ConsoleWrite("Requesting quote (tests trader command path)..." & @CRLF)

    ; Get first item in inventory to quote
    Local $bagPtr = GetBagPtr(1)
    If $bagPtr <> 0 Then
        Local $slots = MemoryRead($ph, $bagPtr + 32, 'long')
        ConsoleWrite("Bag 1 has " & $slots & " slots" & @CRLF)
        For $s = 1 To $slots
            Local $itemPtr = GetItemPtr(1, $s)
            If $itemPtr <> 0 Then
                ConsoleWrite("Requesting quote for item in bag 1, slot " & $s & @CRLF)
                TraderRequestSell($itemPtr)
                Sleep(2000)
                ConsoleWrite("Quote request sent — no crash!" & @CRLF)
                ExitLoop
            EndIf
        Next
    Else
        ConsoleWrite("No items in bag 1" & @CRLF)
    EndIf
Else
    ConsoleWrite("No NPC found near trader coords" & @CRLF)
EndIf

ConsoleWrite("=== Test complete — if you see this, no crash ===" & @CRLF)
