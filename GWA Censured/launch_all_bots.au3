#RequireAdmin
;~ Launch all 5 GW clients with Froggy bot, correct hero configs, and Buy Consets enabled.
;~ Each client is launched with a 3-minute delay between them to avoid interference.
;~ Uses -autolaunch flag so each instance handles char select, reconnect, and Play automatically.

#include <Array.au3>

; Account launch order and character names
Local $characters[5] = [ _
    "B E A S T R I T", _
    "D I S C O P A N I C", _
    "B L U M P K I N S", _
    "Starvin M A R V I N", _
    "L I L B I S C U I T"]

Local $delayBetween = 180  ; 3 minutes between launches

Local $froggyScript = @ScriptDir & "\Froggy_HM_v1.6.au3"
Local $autoitExe = "C:\Program Files (x86)\AutoIt3\AutoIt3.exe"

ConsoleWrite("=== Launching All 5 Bots ===" & @CRLF)
ConsoleWrite("Delay between launches: " & $delayBetween & "s" & @CRLF & @CRLF)

For $i = 0 To 4
    Local $char = $characters[$i]
    ConsoleWrite("[" & @HOUR & ":" & @MIN & ":" & @SEC & "] Launching " & $char & "..." & @CRLF)

    ; Launch Froggy with -autolaunch flag
    ; The script handles: GW launch, multiclient patch, char select, reconnect, Play, hero config
    Run('"' & $autoitExe & '" "' & $froggyScript & '" -autolaunch "' & $char & '"')

    If $i < 4 Then
        ConsoleWrite("  Waiting " & $delayBetween & "s before next launch..." & @CRLF)
        Sleep($delayBetween * 1000)
    EndIf
Next

ConsoleWrite(@CRLF & "=== All 5 bots launched ===" & @CRLF)
ConsoleWrite("Hero configs and Buy Consets are set via GWLauncher_ConfigureFroggyGUI" & @CRLF)
ConsoleWrite("  BEASTRIT = Mercs, all others = Standard" & @CRLF)
ConsoleWrite("  Buy Consets enabled via GUI checkbox after GUI_Create" & @CRLF)
