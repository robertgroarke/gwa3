#RequireAdmin
#include "lib\Froggy_Includes.au3"
ScanAndUpdateGameClients()
ConsoleWrite("Running clients: " & $game_clients[0][0] & @CRLF)
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": " & $game_clients[$i][3] & " (PID=" & $game_clients[$i][0] & ")" & @CRLF)
Next

; Check which characters are missing
Local $allChars[] = ["B E A S T R I T", "D I S C O P A N I C", "B L U M P K I N S", "Starvin M A R V I N", "L I L B I S C U I T"]
For $c In $allChars
    Local $found = False
    For $i = 1 To $game_clients[0][0]
        If $game_clients[$i][3] = $c Then
            $found = True
            ExitLoop
        EndIf
    Next
    If Not $found Then ConsoleWrite("MISSING: " & $c & @CRLF)
Next
