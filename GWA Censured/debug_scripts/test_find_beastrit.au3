#RequireAdmin
#include "lib\Froggy_Includes.au3"
ScanAndUpdateGameClients()
ConsoleWrite("Clients: " & $game_clients[0][0] & @CRLF)
For $i = 1 To $game_clients[0][0]
	SelectClient($i)
	InitializeGameClientForGWA2(False)
	Local $name = GetCharacterName()
	Local $map = GetMapID()
	Local $myid = GetMyID()
	ConsoleWrite("  " & $i & ": PID=" & $game_clients[$i][0] & " char='" & $name & "' map=" & $map & " myid=" & $myid & @CRLF)
Next
