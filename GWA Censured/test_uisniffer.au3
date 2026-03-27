#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== UIMessage Sniffer Test ===" & @CRLF)

; Launch BEASTRIT
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
If $result = 0 Then
    ConsoleWrite("ERROR: Failed to launch" & @CRLF)
    Exit 1
EndIf
ConsoleWrite("GW launched PID=" & $result[0] & @CRLF)

; Wait for character select
ConsoleWrite("Waiting 35s for char select..." & @CRLF)
Sleep(35000)

; Find the NEW client (should be the last one, or one without a name yet)
ScanAndUpdateGameClients()
ConsoleWrite("Clients: " & $game_clients[0][0] & @CRLF)
Local $targetClient = $game_clients[0][0]  ; use the LAST client (newest)
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": " & $game_clients[$i][3] & @CRLF)
Next
ConsoleWrite("Using client " & $targetClient & @CRLF)

SelectClient($targetClient)
InitializeGameClientData(True, False)
ConsoleWrite("Connected" & @CRLF)

; Init and enable sniffer
UISnifferInit()
UISnifferEnable()
ConsoleWrite("=== SNIFFER READY ===" & @CRLF)
ConsoleWrite("WAITING_FOR_CLICKS" & @CRLF)

; Wait 90 seconds for clicks
Sleep(90000)

UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
