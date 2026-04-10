#include "..\lib\Froggy_Includes.au3"

Global Const $TARGET_CHARACTER = "B E A S T R I T"
Global Const $ACCOUNTS_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
Global Const $LOG_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_beastrit_via_gwlauncher.log"

ConsoleWrite("=== Launch BEASTRIT via GWLauncher ===" & @CRLF)
FileDelete($LOG_PATH)
FileWrite($LOG_PATH, "=== Launch BEASTRIT via GWLauncher ===" & @CRLF)

Local $accounts = GWLauncher_LoadAccounts($ACCOUNTS_PATH)
If UBound($accounts) = 0 Then
    ConsoleWrite("ERROR: no accounts loaded from " & $ACCOUNTS_PATH & @CRLF)
    FileWrite($LOG_PATH, "ERROR: no accounts loaded from " & $ACCOUNTS_PATH & @CRLF)
    Exit 1
EndIf

Local $idx = GWLauncher_FindAccountByCharacter($accounts, $TARGET_CHARACTER)
If $idx < 0 Then
    ConsoleWrite("ERROR: character not found: " & $TARGET_CHARACTER & @CRLF)
    FileWrite($LOG_PATH, "ERROR: character not found: " & $TARGET_CHARACTER & @CRLF)
    Exit 2
EndIf

FileWrite($LOG_PATH, "ACCOUNT_INDEX=" & $idx & @CRLF)
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
If $result = 0 Then
    ConsoleWrite("ERROR: GWLauncher_LaunchAccount failed" & @CRLF)
    FileWrite($LOG_PATH, "ERROR: GWLauncher_LaunchAccount failed" & @CRLF)
    Exit 3
EndIf

ConsoleWrite("GWLAUNCHER_PID=" & $result[0] & @CRLF)
FileWrite($LOG_PATH, "GWLAUNCHER_PID=" & $result[0] & @CRLF)
Exit 0
